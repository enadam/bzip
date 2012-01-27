/*
 * compress.c
 */

/* Include files */
#include "config.h"

#include <stdlib.h>
#include <sys/types.h>
#include <assert.h>

#include <string.h>
#include <stdio.h>

#include "compress.h"
#include "main.h"
#include "bzip.h"
#include "bitstream.h"
#include "models.h"
#include "lc_common.h"

/* Standard definitions */
/* The DCC95 arithmetic coder */
#define MAX_BITS_OUTSTANDING	500000000

/* Move-to-front encoding/decoding */
#define NUM_FULLGT_UNROLLINGS	4
#define MAX_DENORM_OFFSET	(4 * NUM_FULLGT_UNROLLINGS)

/* Block-sorting machinery */
#define ISORT_BELOW		10

/* Type definitions */
/* Move-to-front encoding */
union words_t
{
	u_int32_t u;
	u_int16_t s[2];
	u_int8_t c[4];
} __attribute__ ((packed));

/* Function prototypes */
static void arithCodeStartEncoding(void);
static void arithCodeDoneEncoding(void);
static void write_magic(unsigned clevel, unsigned nthreads);
static void thread_slave(unsigned blocksize);

/* Bitstream machinery */
static inline int bs_get_byte(void);
static inline void bs_put_bit(unsigned bit);

/* The DCC95 arithmetic coder */
static inline void arithCodeBitPlusFollow(unsigned bit);
static inline void arithCodeRenormalize_Encode(void);
static void putSymbol(struct Model *m, unsigned symbol);
static void putUInt32(u_int32_t i);

/* Move-to-front encoding */
static inline void sendMTFVal(unsigned n);
static inline void sendZeroes(unsigned zeroesPending);

/* Block-sorting machinery */
static inline int trivialGt(unsigned i1, unsigned i2);
static inline void shellTrivial(void);
static inline int fullGt(unsigned i1, unsigned i2);
static inline void qsortFull(unsigned wuL, unsigned wuR);
static inline void stripe(void);
static inline void sortIt(void);

/* The Reversible Transformation (tm) */
static inline unsigned getRLEpair(u_int8_t *chp);

/* The main driver machinery */
static int loadAndRLEsource(unsigned blocksize);
static void spotBlock(void);
static unsigned doReversibleTransformation(void);
static void moveToFrontCodeAndSend(int finish, unsigned origPtr);

/* Private variables */
/* The DCC95 arithmetic coder */
static u_int32_t bigL, bigR;
static u_int32_t bitsOutstanding;

/* Move-to-front encoding/decoding */
static unsigned words_end;
static union words_t *words = NULL;

/* Block-sorting machinery */
static unsigned *zptr = NULL;

/* Program code */
/* Interface functions */
void compress(void)
{
	unsigned clevel, nthreads;

	clevel = main_runtime.compression_level;
	nthreads = main_runtime.compress_threads;

	write_magic(clevel, nthreads);
	initBogusModel();
	arithCodeStartEncoding();

	thread_slave(clevel * 100000);
} /* compress */

/* Private functions */
void write_magic(unsigned clevel, unsigned nthreads)
{
	char magic[4];

	unsigned i, o;

	/* bs_get_byte() cannot be used because bit_window might be
	 * partially filled (if we encode more than one input files
	 * into exactly one).  Doing bs_flush_bit() would suffice
	 * but the same trick wouldn't work in read_magic() so
	 * to avoid inconsistency, we falled back to this solution. */
	magic[0] = 'B';
	magic[1] = 'Z';
	magic[2] = nthreads > 0 ? 'M' + nthreads : '0';
	magic[3] = clevel + '0';
	for (i = 0; i < MEMBS_OF(magic); i++)
		for (o = 8; o > 0; o--)
		{
			bs_put_bit((magic[i] & (1 << 7)) != 0);
			magic[i] <<= 1;
		}
} /* write_magic */

void arithCodeStartEncoding(void)
{
	bigL = 0;
	bigR = TWO_TO_THE(smallB - 1);
	bitsOutstanding = 0;
} /* arithCodeStartEncoding */

void arithCodeDoneEncoding(void)
{
	u_int32_t i;

	for (i = 1 << smallB; (i >>= 1) > 0; )
		arithCodeBitPlusFollow(!(bigL & i));
} /* arithCodeDoneEncoding */

/*
 * This routine is a part of the not-yet-written multithread support.
 */
void thread_slave(unsigned blocksize)
{
	int finish;

	lc_recallocp(&words, (blocksize + MAX_DENORM_OFFSET)
		* sizeof(*words));
	lc_recallocp(&zptr, blocksize * sizeof(*zptr));

	do
	{
		unsigned origPtr;

		finish = loadAndRLEsource(blocksize);
		spotBlock();
		origPtr = doReversibleTransformation();
		moveToFrontCodeAndSend(finish, origPtr);
	} while (!finish);

	putUInt32(~input_bs.crc);
	arithCodeDoneEncoding();
} /* thread_slave */

/*------------------------------------------------------*/
/* Bitstream machinery					*/
/*------------------------------------------------------*/
int bs_get_byte(void)
{
	u_int8_t c;

	assert(INRANGE(input_bs.byte_p,
		input_bs.byte_window, input_bs.byte_end));

	if (input_bs.byte_p == input_bs.byte_end
			&& !bs_fill_byte(&input_bs, 1))
		return -1;
	c = *input_bs.byte_p++;
	updateCRC(input_bs.crc, c);
	return c;
} /* bs_get_byte */

void bs_put_bit(unsigned bit)
{
	assert(INRANGE(output_bs.bit_p,
		output_bs.bit_window, output_bs.bit_end));

	if (output_bs.bit_p == output_bs.bit_end)
		bs_flush_bit(&output_bs);

	assert((bit & ~1) == 0);
	*output_bs.bit_p++ = bit;
} /* bs_put_bit */

/*------------------------------------------------------*/
/* The DCC95 arithmetic coder				*/
/*------------------------------------------------------*/
/*							*/
/* This is a clean-room (ie, my own) implementation of	*/
/* the coder described in ``Arithmetic Coding		*/
/* Revisited'', by Alistair Moffat, Radford Neal and	*/
/* Ian Witten, originally presented at the 1995 IEEE	*/
/* Data Compression Conference, Snowbird, Utah, USA	*/
/* in March 1995.					*/
/*							*/
/* The paper has evolved somewhat since then.  This	*/
/* implementation pertains to the June 1996 version of	*/
/* the paper.  In particular, we have an initial value	*/
/* for R of 2^(b-1) rather than 2^(b-1) - 1, and	*/
/* termination of coding (overly conservative here)	*/
/* is different.					*/
/*							*/
/* I don't use the shift-add multiply/divide machinery;	*/
/* I could, but it adds complexity & I'm not convinced	*/
/* aboutthe long-term architectural benefit of that	*/
/* approach.  I could be wrong.				*/
/*------------------------------------------------------*/
void arithCodeBitPlusFollow(unsigned bit)
{
	bs_put_bit(!bit);
	for (; bitsOutstanding > 0; bitsOutstanding--)
		bs_put_bit(bit);
} /* arithCodeBitPlusFollow */

void arithCodeRenormalize_Encode(void)
{
	for (; bigR <= TWO_TO_THE(smallB - 2); bigL <<=1, bigR <<= 1)
		if (bigL + bigR <= TWO_TO_THE(smallB - 1))
		{
			arithCodeBitPlusFollow(1);
		} else if (bigL >= TWO_TO_THE(smallB - 1))
		{
			bigL -= TWO_TO_THE(smallB - 1);
			arithCodeBitPlusFollow(0);
		} else
		{
			bigL -= TWO_TO_THE(smallB - 2);
			bitsOutstanding++;
		}
} /* arithCodeRenormalize_Encode */

void putSymbol(struct Model *m, unsigned symbol)
{
	unsigned const *f;
	u_int32_t smallL, smallH, smallT, smallR, smallR_x_smallL;

	assert(TWO_TO_THE(smallB - 2) < bigR);
	assert(bigR <= TWO_TO_THE(smallB - 1));
	assert(bigL < TWO_TO_THE(smallB) - TWO_TO_THE(smallB - 2));
	assert((bigL + bigR) <= TWO_TO_THE(smallB));

	/* Set smallL and smallH to the cumfreq values 
	 * respectively prior to and including symbol. */
	smallT = m->totFreq;
	smallL = 0;
	for (f = m->freq; f < &m->freq[symbol]; f++)
		smallL += *f;
	smallH = smallL + *f;

	smallR = bigR / smallT;
	smallR_x_smallL = smallR * smallL;

	bigL += smallR_x_smallL;
	if (smallH < smallT)
		bigR = smallR * *f;
	else
		bigR -= smallR_x_smallL;

	arithCodeRenormalize_Encode();
	assert(bitsOutstanding <= MAX_BITS_OUTSTANDING);

	updateModel(m, symbol);
} /* putSymbol */

void putUInt32(u_int32_t i)
{
	putSymbol(&model_bogus, (i & 0xFF000000) >> 24);
	putSymbol(&model_bogus, (i & 0xFF0000) >> 16);
	putSymbol(&model_bogus, (i & 0xFF00) >>  8);
	putSymbol(&model_bogus, (i & 0xFF));
} /* putUInt32 */

/*------------------------------------------------------*/
/* Move-to-front encoding/decoding			*/
/*------------------------------------------------------*/
void sendMTFVal(unsigned n)
{
	assert(INRANGE(n, 2, 255));

	putSymbol(&models[MODEL_BASIS], MTFVals_encode[n].v);
	putSymbol(MTFVals_encode[n].m, MTFVals_encode[n].n);
} /* sendMTFVal */

void sendZeroes(unsigned zeroesPending)
{
	unsigned *bp, bitsToSend[BITS_OF(zeroesPending)];;

	assert(zeroesPending > 0);

	bp = bitsToSend;
	do
	{
		zeroesPending--;
		*bp++ = (zeroesPending & 1) ? VAL_RUNA : VAL_RUNB;
		zeroesPending >>= 1;
	} while (zeroesPending);

	do
	{
		bp--;
		putSymbol(&models[MODEL_BASIS], *bp);
	} while (bp > bitsToSend);
} /* sendZeroes */

/*------------------------------------------------------*/
/* Block-sorting machinery				*/
/*------------------------------------------------------*/
int trivialGt(unsigned i1, unsigned i2)
{
	unsigned k;

	/* Use of NORMALIZEHI is safe here, for any
	 * lastPP >= 1 (which is guaranteed), since
	 * the max denormalisation is 1. */
	for (k = 0; k < words_end; k++)
	{
		unsigned char c1, c2;

		c1 = GETFIRST(i1);
		c2 = GETFIRST(i2);
		if (c1 != c2)
			return c1 > c2;
		i1 = NORMALIZEHI(i1 + 1);
		i2 = NORMALIZEHI(i2 + 1);
	}

	return 0;
} /* trivialGt */

/* Always works */
void shellTrivial(void)
{
	unsigned i, j, h, v;

	/* shellTrivial *must* be used for
	 * lastPP <= 4 * NUM_FULLGT_UNROLLINGS.
	 * The 1024 limit is much higher; the
	 * purpose is to avoid lumbering sorting
	 * of small blocks with the fixed overhead
	 * of the full counting-sort mechanism. */
	for (i = 0; i < words_end; i++)
		zptr[i] = i;

	h = 1;
	do
		h *= 3;
	while (h++ < words_end);

	do
	{
		h /= 3;
		for (i = h; i < words_end; i++)
		{
			j = i;
			v = zptr[i];
			while (trivialGt(zptr[j - h], v))
			{
				zptr[j] = zptr[j - h];
				j -= h;
				if (j < h)
					break;
			}
			zptr[j] = v;
		}
	} while (h != 1);
} /* shellTrivial */

int fullGt(unsigned i1, unsigned i2)
{
	unsigned i1orig;

	assert(words_end >= 4 * NUM_FULLGT_UNROLLINGS);

	if (i1 == i2)
		return 0;
	i1orig = i1;

	do
	{
		u_int32_t w1, w2;

		assert(i1 != i2);
		assert(i1 < words_end);
		assert(i2 < words_end);

		w1 = GETALL(i1);
		w2 = GETALL(i2);
		if (w1 != w2)
			return w1 > w2;
		i1 += 4;
		i2 += 4;

		assert(i1 < words_end + 1 * NUM_FULLGT_UNROLLINGS);
		assert(i2 < words_end + 1 * NUM_FULLGT_UNROLLINGS);

		w1 = GETALL(i1);
		w2 = GETALL(i2);
		if (w1 != w2)
			return w1 > w2;
		i1 += 4;
		i2 += 4;

		assert(i1 < words_end + 2 * NUM_FULLGT_UNROLLINGS);
		assert(i2 < words_end + 2 * NUM_FULLGT_UNROLLINGS);

		w1 = GETALL(i1);
		w2 = GETALL(i2);
		if (w1 != w2)
			return w1 > w2;
		i1 += 4;
		i2 += 4;

		assert(i1 < words_end + 3 * NUM_FULLGT_UNROLLINGS);
		assert(i2 < words_end + 3 * NUM_FULLGT_UNROLLINGS);

		w1 = GETALL(i1);
		w2 = GETALL(i2);
		if (w1 != w2)
			return w1 > w2;
		i1 += 4;
		i2 += 4;

		assert(i1 < words_end + 4 * NUM_FULLGT_UNROLLINGS);
		assert(i2 < words_end + 4 * NUM_FULLGT_UNROLLINGS);

		i1 = NORMALIZEHI(i1);
		i2 = NORMALIZEHI(i2);

		assert(i1 != i2);
		assert(i1 < words_end);
		assert(i2 < words_end);
	} while (i1 != i1orig);

	return 0;
} /* fullGt */

/*
 * Requires striping, and therefore doesn't
 * work when lastPP < 4.  This qsort is 
 * derived from Weiss' book "Data Structures and
 * Algorithm Analysis in C", Section 7.7.
 */
void qsortFull(unsigned wuL, unsigned wuR)
{
	unsigned sp, stackL[40], stackR[40];

	/* At the beginning of this loop, wuL and wuR hold the
	 * bounds of the next work-unit. */
	sp = 0;
	for (;;)
	{
		if (wuR - wuL > ISORT_BELOW)
		{ /* A large Work Unit; partition-exchange */
			unsigned i, j, wuC, pivot;

			wuC = (wuL + wuR) / 2;
			if (fullGt(zptr[RC(wuL)], zptr[RC(wuC)]))
				SWAP(wuL, wuC);
			if (fullGt(zptr[RC(wuL)], zptr[RC(wuR)]))
				SWAP(wuL, wuR);
			if (fullGt(zptr[RC(wuC)], zptr[RC(wuR)]))
				SWAP(wuC, wuR);

			SWAP(wuC, wuR - 1);
			pivot = zptr[RC(wuR - 1)];

			i = wuL;
			j = wuR - 1;
			for (;;)
			{
				do i++; while (fullGt(pivot, zptr[RC(i)]));
				do j--; while (fullGt(zptr[RC(j)], pivot));

				if (i >= j)
					break;
				SWAP(i, j);
			}
			SWAP(i, wuR - 1);

			if (2 * i > wuL + wuR)
			{
				stackL[sp] = wuL;
				stackR[sp] = i - 1;
				wuL = i + 1;
			} else
			{
				stackL[sp] = i + 1;
				stackR[sp] = wuR;
				wuR = i - 1;
			}

			sp++;
			assert(sp <= 14);
		} else
		{ /* A small Work-Unit; insertion-sort it */
			unsigned i, j, v;

			for (i = wuL + 1; i <= wuR; i++)
			{
				j = i;
				v = zptr[RC(j)];
				while (fullGt(zptr[RC(j - 1)], v))
				{
					zptr[RC(j)] = zptr[RC(j - 1)];
					j--;
					if (j <= wuL)
						break;
				}
				zptr[RC(j)] = v;
			}

			if (sp == 0)
				break;
			sp--;

			wuL = stackL[sp];
			wuR = stackR[sp];
		} /* if */
	} /* for */
} /* qsortFull */

void stripe(void)
{
	u_int8_t c;
	unsigned i;

	assert(words_end >= 4);

	c = GETFIRST(0);
	SETSECOND(NORMALIZELO(-1), c);
	SETTHIRD(NORMALIZELO(-2), c);
	SETFOURTH(NORMALIZELO(-3), c);

	c = GETFIRST(1);
	SETSECOND(0, c);
	SETTHIRD(NORMALIZELO(-1), c);
	SETFOURTH(NORMALIZELO(-2), c);

	c = GETFIRST(2);
	SETSECOND(1, c);
	SETTHIRD(0, c);
	SETFOURTH(NORMALIZELO(-1), c);

	for (i = 3; i < words_end; i++)
	{
		c = GETFIRST(i);
		SETSECOND(i - 1, c);
		SETTHIRD(i - 2, c);
		SETFOURTH(i - 3, c);
	}
} /* stripe */

void sortIt(void)
{
	static struct 
	{
		unsigned lo, hi;
	} const *grade, bounds[] =
	{
		{ 2, 15 },
		{ 16, 255 },
		{ 256, 4095 },
		{ 4096, 65535 },
		{ 65536, 900000 },
		{ 0, 0 }
	};

	unsigned i, ftab[65537];

	stripe();

	memset(ftab, 0, sizeof(ftab));
	for (i = 0; i < words_end; i++)
		ftab[GETFIRST16(i)]++;
	for (i = 1; i < MEMBS_OF(ftab); i++)
		ftab[i] += ftab[i - 1];
	for (i = 0; i < words_end; i++)
	{
		unsigned f;

		f = --ftab[GETFIRST16(i)];
		zptr[f] = i;
	}

	memcpy(&words[words_end], words, sizeof(*words) * 4 * NUM_FULLGT_UNROLLINGS);
	for (grade = bounds; grade->lo && grade->lo <= words_end; grade++)
	{
		unsigned const *lower, *upper;

		for (lower = ftab, upper = &ftab[1]; upper < AFTER_OF(ftab);
			lower++, upper++)
		{
			unsigned j, k, freqHere;

			freqHere = *upper - *lower;
			if (!INRANGE(freqHere, grade->lo, grade->hi))
				continue;

			qsortFull(*lower, *upper - 1);
			if (freqHere >= 65535)
				continue;

			for (j = *lower, k = 0; j < *upper; j++, k++)
			{
				unsigned a2update;

				a2update = zptr[j];
				SETREST16(a2update, k);
				if (a2update < 4 * NUM_FULLGT_UNROLLINGS)
					SETREST16(words_end + a2update, k);
			}
		} /* for */
	} /* for */
} /* sortIt */

/*------------------------------------------------------*/
/* The Reversible Transformation (tm)			*/
/*------------------------------------------------------*/
unsigned getRLEpair(u_int8_t *chp)
{
	static int ch = -1;
	static int inited = 0;

	unsigned runLength;

	if (ch < 0)
	{
		if (inited)
		{
			inited = 0;
			return 0;
		}

		inited = 1;
		ch = bs_get_byte();
		if (ch < 0)
		{
			inited = 0;
			return 0;
		}
	}

	*chp = ch;
	runLength = 0;
	do
	{
		runLength++;
		ch = bs_get_byte();
	} while (ch == *chp && runLength < 255);

	return runLength;
} /* getRLEpair */

/*------------------------------------------------------*/
/* The main driver machinery 				*/
/*------------------------------------------------------*/
int loadAndRLEsource(unsigned blocksize)
{
	/* 20 is just a paranoia constant */
	for (words_end = 0; words_end <= blocksize - 20; )
	{
		u_int8_t ch;
		unsigned runLen;

		runLen = getRLEpair(&ch);
		assert(runLen <= 255);
		switch (runLen)
		{
		default:
			SETFIRST(words_end++, ch);
			SETFIRST(words_end++, ch);
			SETFIRST(words_end++, ch);
			SETFIRST(words_end++, ch);
			SETFIRST(words_end++, runLen - 4);
			break;

		case 3:
			SETFIRST(words_end++, ch);
		case 2:                
			SETFIRST(words_end++, ch);
		case 1:                
			SETFIRST(words_end++, ch);
			break;

		case 0:
			SETFIRST(words_end++, 42);
			return 1;
		} /* switch */
	} /* for */

	return 0;
} /* loadAndRLEsource */

void spotBlock(void)
{
	int delta;
	unsigned pos;

	delta = 1;
	pos = SPOT_BASIS_STEP;
	while (pos < words_end - 1)
	{
		static unsigned const newdeltas[]
			= { 0, 4, 6, 1, 5, 9, 7, 3, 8, 2 };

		SETFIRST(pos, GETFIRST(pos) + 1);
		delta = newdeltas[delta];
		pos += SPOT_BASIS_STEP + 17 * (delta - 5);
	}
} /* spotBlock */

/*
 * We have to be pretty careful for small block
 * sizes; the usual mechanism won't work properly,
 * all, ultimately, because the pointer normalisation
 * machinery doesn't work right whenever the amount
 * of denormalisation exceeds lastPP.  And the
 * greatest possible amount of denormalisation here
 * is generated in fullGt, as 4 * NUM_FULLGT_UNROLLINGS.
 *
 * To make blocks smaller than 4 * NUM_... sort
 * correctly, it seems easiest simply
 * to forget about striping, &c, and just do a simple
 * shellsort on the un-striped block.  The performance
 * loss has to be inconsequential, since 4 * NUM_... is
 * tiny (16 at present).
 *
 * In the various block-sized structures, live data runs
 * from 0 to words_end inclusive, so lastPP is the number
 * of live data items.
 */
unsigned doReversibleTransformation(void)
{
	unsigned origPtr;

	if (words_end > 1024)
		sortIt();
	else
		shellTrivial();

	/* Index in ptr[] of original string after sorting */
	for (origPtr = 0; origPtr < words_end; origPtr++)
		if (zptr[origPtr] == 0)
			return origPtr;
	panic("doReversibleTransformation");
} /* doReversibleTransformation */

void moveToFrontCodeAndSend(int finish, unsigned origPtr)
{
	char yy[256];
	unsigned i, zeroesPending;

	putUInt32(finish ? -(origPtr + 1) : origPtr + 1);
	initModels();

	for (i = 0; i < 256; i++)
		yy[i] = i;

	zeroesPending = 0;
	for (i = 0; i < words_end; i++)
	{
		char ll_i;

		ll_i = GETFIRST(NORMALIZELO(zptr[i] - 1));
		if (ll_i == yy[0])
		{
			zeroesPending++;
			continue;
		}

		switch (zeroesPending)
		{
		default:
			sendZeroes(zeroesPending);
			zeroesPending = 0;
		case 0:
			break;
		}

		if (ll_i == yy[1])
		{
			yy[1] = yy[0];
			yy[0] = ll_i;

			putSymbol(&models[MODEL_BASIS], VAL_ONE);
		} else
		{
			unsigned j;
			char const *yyfrom;

#if defined(HAVE_RAWMEMCHR) && !defined(CONFIG_DEBUG)
			j = (char *)rawmemchr(&yy[2], ll_i) - &yy[0];
#else
			yyfrom = memchr(&yy[2], ll_i, sizeof(yy) - 2);
			assert(yyfrom != NULL);
			j = yyfrom - &yy[0];
#endif
			memmove(&yy[1], &yy[0], j);
			yy[0] = ll_i;

			sendMTFVal(j);
		} /* if */
	} /* for */

	if (zeroesPending)
		sendZeroes(zeroesPending);
	putSymbol(&models[MODEL_BASIS], VAL_EOB);
} /* moveToFrontCodeAndSend */

/* End of compress.c */
