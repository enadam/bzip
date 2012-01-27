/*
 * decompress.c
 */

/* Include files */
#include "config.h"

#include <stdlib.h>
#include <sys/types.h>
#include <assert.h>

#include <string.h>

#include "main.h"
#include "bzip.h"
#include "bitstream.h"
#include "models.h"
#include "lc_common.h"

/* Function prototypes */
static void invalid_input(char const *msg)
	__attribute__ ((noreturn));
static unsigned read_magic(void);
static void arithCodeStartDecoding(void);

/* Bitstream machinery */
static inline unsigned bs_get_bit(void);
static inline void bs_put_byte(u_int8_t c);

/* The DCC95 arithmetic coder */
static unsigned getSymbol(struct Model *m);
static u_int32_t getUInt32(void);

/* Move-to-front decoding */
static inline unsigned getMTFVal(unsigned symbol);

/* The main driver machinery */
static int getAndMoveToFrontDecode(unsigned limit);
static void undoReversibleTransformation(void);
static void spotBlock(void);
static void unRLEandDump(int finish);

/* Private variables */
/* The DCC95 arithmetic coder */
static u_int32_t bigR, bigD;

static unsigned *zptr = NULL;
static unsigned origPtr;

static unsigned char *ll = NULL;
static unsigned char *block = NULL;
static unsigned block_end;

/* Program code */
/* Interface functions */
void decompress(void)
{
	int finish;
	unsigned blocksize;

	blocksize = read_magic() * 100000;

	lc_recallocp(&block, blocksize * sizeof(*block));
	lc_recallocp(&ll, blocksize * sizeof(*ll));
	lc_recallocp(&zptr, blocksize * sizeof(*zptr));

	initBogusModel();
	arithCodeStartDecoding();

	do
	{
		finish = getAndMoveToFrontDecode(blocksize);
		undoReversibleTransformation();
		spotBlock();
		unRLEandDump(finish);
	} while (!finish);

	if (~getUInt32() != output_bs.crc)
		invalid_input("CRC error");
} /* decompress */

/* Private functions */
void invalid_input(char const *msg)
{
	logf("%s: %s", input_bs.fname, msg);
	throw_exception(EXIT_ERR_INPUT);
} /* invalid_input */

unsigned read_magic(void)
{
	char magic[4];
	unsigned i, o;

	for (i = 0; i < MEMBS_OF(magic); i++)
	{
		magic[i] = 0;
		for (o = 8; o > 0; o--)
		{
			magic[i] <<= 1;
			magic[i] |= bs_get_bit();
		}
	}

	if (magic[0] != 'B' || magic[1] != 'Z'
			|| magic[2] != '0' || magic[3] < '1')
		invalid_input("invalid magic");
	return magic[3] - '0';
} /* read_magic */

void arithCodeStartDecoding(void)
{
	unsigned i;

	bigR = TWO_TO_THE(smallB - 1);
	bigD = 0;
	for (i = 1; i <= smallB; i++)
	{
		bigD <<= 1;
		bigD |= bs_get_bit();
	}
} /* arithCodeStartDecoding */

/*------------------------------------------------------*/
/* Bitstream machinery					*/
/*------------------------------------------------------*/
unsigned bs_get_bit(void)
{
	assert(INRANGE(input_bs.bit_p,
		input_bs.bit_window, input_bs.bit_end));

	if (input_bs.bit_p == input_bs.bit_end)
		bs_fill_bit(&input_bs, 0);

	assert((*input_bs.bit_p & ~1) == 0);
	return *input_bs.bit_p++;
} /* bs_get_bit */

void bs_put_byte(u_int8_t c)
{
	assert(INRANGE(output_bs.byte_p,
		output_bs.byte_window, output_bs.byte_end));

	if (output_bs.byte_p == output_bs.byte_end)
		bs_flush_byte(&output_bs);
	updateCRC(output_bs.crc, c);
	*output_bs.byte_p++ = c;
} /* bs_put_byte */

/*------------------------------------------------------*/
/* The DCC95 arithmetic coder				*/
/*------------------------------------------------------*/
unsigned getSymbol(struct Model *m)
{
	unsigned symbol;
	unsigned const *f;
	u_int32_t smallL, smallH, smallT, smallR, smallR_x_smallL, target;

	smallT = m->totFreq;

	/* Get target value */
	smallR = bigR / smallT;
	target = bigD / smallR;
	if (target >= smallT)
		target = smallT - 1;

	smallH = 0;
	for (f = m->freq; ; f++)
	{
		assert(f < &m->freq[m->numSymbols]);

		smallH += *f;
		if (smallH > target)
			break;
	}
	smallL = smallH - *f;

	smallR_x_smallL = smallR * smallL;
	bigD -= smallR_x_smallL;

	if (smallH < smallT)
		bigR = smallR * *f;
	else
		bigR -= smallR_x_smallL;

	while (bigR <= TWO_TO_THE(smallB - 2))
	{
		bigR <<= 1;
		bigD <<= 1;
		bigD |= bs_get_bit();
	}

	symbol = f - m->freq;
	assert(INRANGE(symbol, 0, m->numSymbols - 1));
	updateModel(m, symbol);

	return symbol;
} /* getSymbol */

u_int32_t getUInt32(void)
{
	return (getSymbol(&model_bogus) << 24)
		| (getSymbol(&model_bogus) << 16)
		| (getSymbol(&model_bogus) << 8)
		| getSymbol(&model_bogus);
} /* getUInt32 */

/*------------------------------------------------------*/
/* Move-to-front encoding/decoding			*/
/*------------------------------------------------------*/
unsigned getMTFVal(unsigned symbol)
{
	return symbol == VAL_ONE
		? 1
		: (getSymbol(MTFVals_decode[symbol].m)
			| MTFVals_decode[symbol].n);
} /* getMFTVal */

/*------------------------------------------------------*/
/* The main driver machinery 				*/
/*------------------------------------------------------*/
int getAndMoveToFrontDecode(unsigned limit)
{
	char yy[256];
	int32_t i, tmpOrigPtr;

	tmpOrigPtr = getUInt32();
	origPtr = (tmpOrigPtr < 0 ? -tmpOrigPtr : tmpOrigPtr) - 1;
	initModels();

	for (i = 0; i < 256; i++)
		yy[i] = i;

	for (block_end = 0; ; block_end++)
	{
		unsigned nextSym;

		nextSym = getSymbol(&models[MODEL_BASIS]);
		if (nextSym == VAL_RUNA || nextSym == VAL_RUNB)
		{ /* Acquire run-length bits, most significant first */
			unsigned n;

			n = 0;
			do
			{
				n <<= 1;
				n++;
				if (nextSym == VAL_RUNA)
					n++;
				nextSym = getSymbol(&models[MODEL_BASIS]);
			} while (nextSym == VAL_RUNA || nextSym == VAL_RUNB);

			if (block_end + n > limit)
				invalid_input("file corrupt");

			memset(&ll[block_end], yy[0], n);
			block_end += n;
		} /* if */

		if (nextSym == VAL_EOB)
			break;
		nextSym = getMTFVal(nextSym);
		assert(INRANGE(nextSym, 1, 255));

		if (block_end >= limit)
			invalid_input("file corrupt");
		ll[block_end] = yy[nextSym];
		memmove(&yy[1], &yy[0], nextSym);
		yy[0] = ll[block_end];
	} /* for */

	return tmpOrigPtr < 0;
} /* getAndMoveToFrontDecode */

/*
 * Use: ll[0 .. last] and origPtr
 * Def: block[0 .. last]
 */
void undoReversibleTransformation(void)
{
	unsigned i, j, cc[256], sum, orig_sum, ll_i;

	memset(cc, 0, sizeof(cc));
	for (i = 0; i < block_end; i++)
		zptr[i] = cc[ll[i]]++;

	sum = 0;
	for (i = 0; i < MEMBS_OF(cc); i++)
	{
		orig_sum = sum;
		sum += cc[i];
		cc[i] = orig_sum;
	};

	i = origPtr;
	for (j = block_end; j-- > 0; )
	{
		ll_i = ll[i];
		block[j] = ll_i;
		i = zptr[i] + cc[ll_i];
	}
} /* undoReversibleTransformation */

void spotBlock(void)
{
	int delta;
	unsigned pos;

	delta = 1;
	pos = SPOT_BASIS_STEP;
	while (pos < block_end - 1)
	{
		static unsigned const newdeltas[]
			= { 0, 4, 6, 1, 5, 9, 7, 3, 8, 2 };

		block[pos]--;
		delta = newdeltas[delta];
		pos += SPOT_BASIS_STEP + 17 * (delta - 5);
	}
} /* spotBlock */

void unRLEandDump(int finish)
{
	int chPrev;
	unsigned i, count;

	if (finish)
		block_end--;

	count = 0;
	chPrev = -1;
	for (i = 0; i < block_end; i++)
	{
		int ch;

		ch = block[i];
		bs_put_byte(ch);

		if (ch != chPrev)
		{
			chPrev = ch;
			count = 1;
			continue;
		}

		count++;
		if (count < 4)
			continue;

		i++;
		if (i >= block_end)
			invalid_input("file corrupt");

		for (count = block[i]; count > 0; count--)
			bs_put_byte(ch);
	} /* for */

	if (finish && block[i] != 42)
		invalid_input("file corrupt");
} /* unRLEandDump */

/* End of decompress.c */
