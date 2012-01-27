/*
 * bzip.c
 */

/* Include files */
#include "config.h"

#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <sys/types.h>
#include <stdarg.h>
#include <assert.h>
#include <setjmp.h>
#include <errno.h>

#include <string.h>
#include <stdio.h>
#ifdef HAVE_GETOPT_H
# include <getopt.h>
#endif

#include <fcntl.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/stat.h>

#include "version.h"
#include "cmdline.h"
#include "crc.h"

/* Standard definitions */
/* Default vaules */
#define DFLT_COMPRESSION_LEVEL	9
#define DFLT_VERBOSITY		BZIP_LOG_WARNING

/* Error codes */
#define EXIT_HAPPILY		0
#define EXIT_TERMINATED		1
#define EXIT_ERR_ARG		2
#define EXIT_ERR_OTHER		3
#define EXIT_ERR_INSIDE		4
#define EXIT_ERR_MAGIC		5
#define EXIT_ERR_CORRUPT	6

/* Message severities */
#define BZIP_LOG_ALERT		-2
#define BZIP_LOG_ERROR		-1
#define BZIP_LOG_WARNING	0
#define BZIP_LOG_INFO		1
#define BZIP_LOG_DEBUG		2

/* Main driver machivery */
#define BZIP_FORMAT_CODE 	'0'
#define BZIP_EXT		".bz"

/* Bitstream I/O */
#define BS_WINDOW		81920

/* The DCC95 arithmetic coder */
#define TWO_TO_THE(n)		(1 << (n))
#define MAX_BITS_OUTSTANDING	500000000

#define smallB			26
#define smallF			18

/* Generic frequency-table stuff */
#define MAX_SYMBOLS		256

/* The structured model proper */
#define BASIS			0
#define MODEL_2_3		1
#define MODEL_4_7		2
#define MODEL_8_15		3
#define MODEL_16_31		4
#define MODEL_32_63		5
#define MODEL_64_127		6
#define MODEL_128_255		7

#define VAL_RUNA		1
#define VAL_RUNB		2
#define VAL_ONE			3
#define VAL_2_3			4
#define VAL_4_7			5
#define VAL_8_15		6
#define VAL_16_31		7
#define VAL_32_63		8
#define VAL_64_127		9
#define VAL_128_255		10
#define VAL_EOB			11

#define RUNA			257
#define RUNB			258
#define EOB			259
#define INVALID			260

/* Move-to-front encoding/decoding */
#define NUM_FULLGT_UNROLLINGS	4
#define MAX_DENORM_OFFSET	(4 * NUM_FULLGT_UNROLLINGS)

/* Block-sorting machinery */
#define ISORT_BELOW		10

/* The block loader and RLEr */
#define SPOT_BASIS_STEP		8000

/* Macros */
/* Stringification (used only ford debugging) */
#define QQ(p)		#p
#define Q(p)		QQ(p)

/* Move-to-front encoding/decoding */
#define GETFIRST(a)	((unsigned char)(words[a] >> 24))
#define GETREST(a)	(words[a] & 0x00ffffff)
#define SETALL(a, w)	words[a] = (w)
#define GETFIRST16(a)	((u_int32_t)(words[a] >> 16))
#define GETREST16(a)	(words[a] & 0x0000ffff)

/* Block-sorting machinery */
#ifdef CONFIG_DEBUG
# define RC(x) \
	(((x) >= wuL && (x) <= wuR) \
		? (x) \
		: (panic("Range error: %d (%d, %d)\n", \
			(x), wuL, wuR), 1))
#else
# define RC(x)		(x)
#endif /* ! CONFIG_DEBUG */

#define SWAP(za, zb) \
do \
{ \
	int32_t zl = (za), zr = (zb), zt; \
	\
	zt = zptr[RC(zl)]; \
	zptr[zl] = zptr[RC(zr)]; \
	zptr[zr] = zt; \
} while (0)

/* Debugging */
#define DUMP_VAR(vn)		dump_variable(Q(vn), &vn, sizeof(vn))

#define DUMP_PTR(vn, size) \
	if ((vn) != NULL && blocksize >= 0) \
		dump_variable(Q(vn), (vn), (size))

#ifdef CONFIG_DEBUG
# define LOG_DEBUG(params)	bz_log_debug params;
#else
# define LOG_DEBUG(params)	/**/
#endif

/* Type definitions */
/* Bitstream I/O */
struct bitstream_st
{
	int fd;
	char const *fname;
	u_int8_t window[BS_WINDOW], *windowp, *window_end;
	unsigned livemask;
	unsigned processed;
};

/*
 * Generic frequency-table stuff 
 *
 * freq[0] is unused, and is kept at zero.
 * freq[MAX_SYMBOLS + 1] is also unused and
 * kept at zero.  This is for historical
 * reasons, and is no longer necessary.
 *
 * The counts for symbols 1..numSymbols are 
 * stored at freq[1] .. freq[numSymbols].
 *
 * Presumably one should make sure that 
 * ((incVal + noExceed) / 2) < noExceed
 * so that scaling always produces sensible
 * results.
 *
 * We take incValue == 0 to mean that the
 * counts shouldn't be incremented or scaled.
 *
 * This data decl has to go before the
 * arithmetic coding stuff.
 */
struct Model
{
	u_int32_t numScalings;
	u_int32_t numTraffic;
	u_int32_t totFreq;
	u_int32_t numSymbols;
	u_int32_t incValue;
	u_int32_t noExceed;
	char const *name;
	u_int32_t freq[MAX_SYMBOLS + 2];
};

/* Function prototypes */
/* General */
static void *bz_malloc(size_t const size);

/* 32-bit CRC grunge */
static void initializeCRC(void);
static u_int32_t getFinalCRC(void);
static u_int32_t getGlobalCRC(void);
static void setGlobalCRC(u_int32_t const newCrc);
static u_int32_t updateCRC(u_int32_t const crcVar, u_int8_t const cha)
	__attribute__ ((const));

/* Bitstream I/O */
static void bs_close(struct bitstream_st *bs);
static inline void bs_flush_window(struct bitstream_st *bs);
static inline int bs_fill_window(struct bitstream_st *bs);

static inline void bs_put_byte(struct bitstream_st *bs,
	u_int8_t const b);
static inline int bs_get_byte(struct bitstream_st *bs);
static inline void bs_unget_byte(struct bitstream_st *bs,
	u_int8_t const b);

static inline void bs_put_bit(struct bitstream_st *bs,
	unsigned const bit);
static inline unsigned bs_get_bit(struct bitstream_st *bs);

/* The DCC95 arithmetic coder */
static inline u_int32_t minUInt32(u_int32_t const a, u_int32_t const b)
	__attribute__ ((const));
static inline void arithCodeBitPlusFollow(struct bitstream_st *bs,
	unsigned const bit);

static void arithCodeStartEncoding(void);
static void arithCodeDoneEncoding(struct bitstream_st *bs);

static void arithCodeStartDecoding(struct bitstream_st *bs);
static void arithCodeDoneDecoding(void);

static inline void arithCodeRenormalize_Encode(struct bitstream_st *bs);
static void arithCodeSymbol(struct bitstream_st *bs,
	struct Model const *m, int32_t const symbol);
static int32_t arithDecodeSymbol(struct bitstream_st *bs,
	struct Model const *m);

/* Generic frequency-table stuff */
static void initModel(struct Model *m, char const *initName,
	int32_t const initNumSymbols, int32_t const initIncValue,
	int32_t const initNoExceed);
static void dumpModelStats(struct Model const *m);
static inline void updateModel(struct Model *m, int32_t const symbol);
static inline void putSymbol(struct Model *m, int32_t const symbol,
	struct bitstream_st *bs);
static inline int32_t getSymbol(struct Model *m, struct bitstream_st *bs);

/* For sending bytes/words thru arith coder */
static void initBogusModel(void);
static void putUChar(struct bitstream_st *bs, unsigned char const c);
static void putInt32(struct bitstream_st *bs, int32_t const i);
static void putUInt32(struct bitstream_st *bs, u_int32_t const i);
static unsigned char getUChar(struct bitstream_st *bs);
static int32_t getInt32(struct bitstream_st *bs);
static u_int32_t getUInt32(struct bitstream_st *bs);

/* The structured model proper */
static void initModels(void);
static void dumpAllModelStats(void);
static int32_t getMTFVal(struct bitstream_st *bs);
static void sendMTFVal(struct bitstream_st *bs, int32_t const n);

/* Move-to-front encoding/decoding */
static inline u_int32_t GETALL(int32_t const a);
static inline void SETREST16(int32_t const a, u_int32_t const w);
static inline void SETFIRST16(int32_t const a, u_int32_t const w);
static inline void SETREST(int32_t const a, u_int32_t const w);
static inline void SETFIRST(int32_t const a, unsigned char const c);
static inline void SETSECOND(int32_t const a, unsigned char const c);
static inline void SETTHIRD(int32_t const a, unsigned char const c);
static inline void SETFOURTH(int32_t const a, unsigned char const c);
static inline int32_t NORMALIZE(int32_t const p);
static inline int32_t NORMALIZEHI(int32_t const p);
static inline int32_t NORMALIZELO(int32_t const p);
static inline int32_t STRONG_NORMALIZE(int32_t const p);
static void sendZeroes(struct bitstream_st *outStream,
	int32_t zeroesPending);
static void moveToFrontCodeAndSend(struct bitstream_st *outStream,
	unsigned const thisIsTheLastBlock);
static int getAndMoveToFrontDecode(struct bitstream_st *inStream,
	unsigned const limit);

/* Block-sorting machinery */
static void stripe(void);
static void copyOffsetWords(void);
static inline int fullGt(int32_t i1, int32_t i2);
static void qsortFull(int32_t const left, int32_t const right);
static inline int trivialGt(int32_t i1, int32_t i2);
static void shellTrivial(void);
static void sortIt(void);

/* The Reversible Transformation (tm) */
static void doReversibleTransformation(void);
static void undoReversibleTransformation(void);
static void spotBlock(unsigned const weAreCompressing);
static inline int32_t getRLEpair(struct bitstream_st *ibs,
	unsigned *runLength);
static unsigned loadAndRLEsource(struct bitstream_st *ibs,
	unsigned const blocksize);
static void unRLEandDump(struct bitstream_st *obs,
	unsigned const thisIsTheLastBlock);

/* Error [non-] handling grunge */
static void exiterror(unsigned const exitcode, char const *fmt, ...)
	__attribute__ ((noreturn))
	__attribute__ ((format (printf, 2, 3)));
static void panic(char const *fmt, ...)
	__attribute__ ((noreturn))
	__attribute__ ((format (printf, 1, 2)));

#ifdef CONFIG_DEBUG
/* Debugging */
static void bz_log_debug(char const *fmt, ...)
	__attribute__ ((format (printf, 1, 2)));
static void dump_variable(char const *name, void const *addr,
	unsigned size);
static void dump_global_variables(int const blocksize,
	char const *id);
#endif

/* Main driver machinery */
static unsigned compress(struct bitstream_st *ibs,
	struct bitstream_st *obs, unsigned const clevel);
static unsigned decompress(struct bitstream_st *ibs,
	struct bitstream_st *obs);

static void output_cleanup(struct bitstream_st *bs);
static int output_generate_filename(char *ofname,
	unsigned const ofname_size, const char *ifname,
	unsigned const compress);
static int bz_open(char const *fname, int const flags,
	mode_t const mode, unsigned const paranoid);

static void my_sigint(int const signum) __attribute__ ((noreturn));
static void my_sigsegv(int const signum) __attribute__ ((noreturn));

static void bz_log(int const severity, char const *fmt, ...)
	__attribute__ ((format (printf, 2, 3)));

/* Private variables */
/* 32-bit CRC grunge */
static u_int32_t globalCrc;

/* The DCC95 arithmetic coder */
static u_int32_t bigL;
static u_int32_t bigR;
static u_int32_t bigD;
static u_int32_t bitsOutstanding;

/* The structured model proper */
static struct Model models[8];

/*
 * Move-to-front encoding/decoding
 *
 * Pointers to compression and decompression
 * structures.
 *
 * The structures are always set to be suitable
 * for a block of size 100000 * compression_level
 */
static u_int32_t *words = NULL;		/* compress */
static int32_t *ftab = NULL;		/* compress */
static int32_t *zptr = NULL;		/* compress & uncompress */

static unsigned char *block = NULL;	/* uncompress */
static unsigned char *ll = NULL;	/* uncompress */

/* Always: lastPP == last + 1.  See discussion in sortIt() */
static int32_t last;
static int32_t lastPP;

/* Index in ptr[] of original string after sorting */
static int32_t origPtr;

static const unsigned char init_yy[256] =
{ 
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
	0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13,
	0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D,
	0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
	0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31,
	0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B,
	0x3C, 0x3D, 0x3E, 0x3F, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45,
	0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
	0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
	0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F, 0x60, 0x61, 0x62, 0x63,
	0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D,
	0x6E, 0x6F, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
	0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F, 0x80, 0x81,
	0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x8B,
	0x8C, 0x8D, 0x8E, 0x8F, 0x90, 0x91, 0x92, 0x93, 0x94, 0x95,
	0x96, 0x97, 0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F,
	0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9,
	0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB0, 0xB1, 0xB2, 0xB3,
	0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD,
	0xBE, 0xBF, 0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7,
	0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF, 0xD0, 0xD1,
	0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB,
	0xDC, 0xDD, 0xDE, 0xDF, 0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5,
	0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF,
	0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9,
	0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF
};

/* The main driver machinery */
static jmp_buf exception_handler;
static struct bitstream_st *output_bs = NULL;
static const char *bzip_prgname;
static int bzip_verbosity = DFLT_VERBOSITY;

/*------------------------------------------------------*/
/* General - program code starts here			*/
/*------------------------------------------------------*/
void *bz_malloc(size_t const size)
{
	void *p;

	if ((p = malloc(size)) == NULL)
		exiterror(EXIT_ERR_OTHER, "malloc(%u): %s\n",
			size, strerror(errno));
	memset(p, 0, size);
	return p;
} /* bz_malloc */


/*------------------------------------------------------*/
/* 32-bit CRC grunge					*/
/*------------------------------------------------------*/
/*							*/
/* I think this is an implementation of the AUTODIN-II,	*/
/* Ethernet & FDDI 32-bit CRC standard.  Vaguely	*/
/* derived from code by Rob Warnock, in Section 51 of	*/
/* the comp.compression FAQ.				*/
/*------------------------------------------------------*/
void initializeCRC(void)
{
	globalCrc = 0xFFFFFFFF;
} /* initializeCRC */

u_int32_t getFinalCRC(void)
{
	return ~globalCrc;
} /* getFinalCRC */

u_int32_t getGlobalCRC(void)
{
	return globalCrc;
} /* getGlobalCRC */

void setGlobalCRC(u_int32_t newCrc)
{
	globalCrc = newCrc;
} /* secGlobalCRC */

u_int32_t updateCRC(u_int32_t const crcVar, u_int8_t const cha)
{
	return (crcVar << 8) ^ crc32Table[(crcVar >> 24) ^ cha];
} /* upateCRC */


/*------------------------------------------------------*/
/* Bitstream I/O					*/
/*------------------------------------------------------*/
void bs_close(struct bitstream_st *bs)
{
	if (bs->livemask != 1 << 7)
	{
		bs->windowp++;
		bs->livemask = 1 << 7;
	}

	bs_flush_window(bs);
} /* bs_close */

void bs_flush_window(struct bitstream_st *bs)
{
	int len;

	assert(bs->livemask == 1 << 7);

	len = bs->windowp - bs->window;
	if (write(bs->fd, bs->window, len) == -1)
		exiterror(EXIT_ERR_OTHER, "write: %s: %s\n",
			bs->fname, strerror(errno));
	memset(bs->window, 0, sizeof(bs->window));

	bs->processed += len;
	bs->windowp = bs->window;
} /* bs_flush_window */

int bs_fill_window(struct bitstream_st *bs)
{
	int len;

	assert(bs->livemask == 1 << 7);

	if ((len = read(bs->fd, &bs->window[1],
			sizeof(bs->window) - 1)) == -1)
		exiterror(EXIT_ERR_OTHER, "read: %s: %s\n",
			bs->fname, strerror(errno));

	bs->processed += len;
	bs->windowp = &bs->window[1];
	bs->window_end = &bs->window[1] + len;

	return len;
} /* bs_fill_window */

void bs_put_byte(struct bitstream_st *bs, u_int8_t b)
{
	assert(bs->livemask == 1 << 7);

	*bs->windowp = b;
	if (++bs->windowp >= bs->window_end)
		bs_flush_window(bs);
} /* bs_put_byte */

int bs_get_byte(struct bitstream_st *bs)
{
	assert(bs->livemask == 1 << 7);

	if (bs->windowp >= bs->window_end && bs_fill_window(bs) == 0)
		return -1;
	return *bs->windowp++;
} /* bs_get_byte */

void bs_unget_byte(struct bitstream_st *bs, u_int8_t const b)
{
	assert(bs->livemask == 1 << 7);

	*(--bs->windowp) = b;
} /* bs_unget_byte */

void bs_put_bit(struct bitstream_st *bs, unsigned const bit)
{
	assert(bs->livemask >= 0 && bs->livemask <= 1 << 7);

	if (bs->livemask == 0)
	{
		bs->livemask = 1 << 7;
		if (++bs->windowp >= bs->window_end)
			bs_flush_window(bs);
	}

	if (bit)
		*bs->windowp |= bs->livemask;
	bs->livemask >>= 1;
} /* bs_put_bit */

unsigned bs_get_bit(struct bitstream_st *bs)
{
	unsigned bit;

	assert(bs->livemask >= 0 && bs->livemask <= 1 << 7);

	if (bs->livemask == 0)
	{
		bs->livemask = 1 << 7;
		if (++bs->windowp >= bs->window_end
			&& bs_fill_window(bs) == 0)
		{
			bz_log(BZIP_LOG_ERROR, "%s: file is corrupt\n",
				bs->fname);
			longjmp(exception_handler, EXIT_ERR_CORRUPT);
		}
	}

	bit = *bs->windowp & bs->livemask;
	bs->livemask >>= 1;
	return (bit != 0);
} /* bs_get_bit */


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
u_int32_t minUInt32(u_int32_t const a, u_int32_t const b)
{
	if (a < b)
		return a;
	else
		return b;
} /* minUInt32 */

void arithCodeBitPlusFollow(struct bitstream_st *bs, unsigned const bit)
{
	bs_put_bit(bs, bit);
	while (bitsOutstanding > 0)
	{
		bs_put_bit(bs, 1 - bit);
		bitsOutstanding--;
	}
} /* arithCodeBitPlusFollow */

void arithCodeStartEncoding(void)
{
	bigL = 0;
	bigR = TWO_TO_THE(smallB - 1);
	bitsOutstanding = 0;
} /* arithCodeStartEncoding */

void arithCodeDoneEncoding(struct bitstream_st *bs)
{
	unsigned i;

	for (i = smallB; i >= 1; i--)
		arithCodeBitPlusFollow(bs, (bigL >> (i - 1)) & 0x1);
} /* arithCodeDoneEncoding */

void arithCodeStartDecoding(struct bitstream_st *bs)
{
	unsigned i;

	bigL = 0;
	bigR = TWO_TO_THE(smallB - 1);
	bigD = 0;
	for (i = 1; i <= smallB; i++)
		bigD = (bigD << 1) + bs_get_bit(bs);
} /* arithCodeStartDecoding */

void arithCodeDoneDecoding(void)
{
	/* No action necessary */
} /* arithCodeDoneDecoding */

void arithCodeRenormalize_Encode(struct bitstream_st *bs)
{
	while (bigR <= TWO_TO_THE(smallB - 2))
	{
		if ((bigL + bigR) <= TWO_TO_THE(smallB - 1))
		{
			arithCodeBitPlusFollow(bs, 0);
		} else if (TWO_TO_THE(smallB - 1) <= bigL)
		{
			arithCodeBitPlusFollow(bs, 1);
			bigL = bigL - TWO_TO_THE(smallB - 1);
		} else
		{
			bitsOutstanding++;
			bigL = bigL - TWO_TO_THE(smallB - 2);
		}
		bigL = 2 * bigL;
		bigR = 2 * bigR;
	}
} /* arithCodeRenormalize_Encode */

void arithCodeSymbol(struct bitstream_st *bs, struct Model const *m,
	int32_t const symbol)
{
	u_int32_t smallL, smallH, smallT, smallR, smallR_x_smallL;
	int32_t i;

	assert(TWO_TO_THE(smallB - 2) < bigR);
	assert(bigR <= TWO_TO_THE(smallB - 1));
	assert(0 <= bigL);
	assert(bigL < TWO_TO_THE(smallB) - TWO_TO_THE(smallB - 2));
	assert((bigL + bigR) <= TWO_TO_THE(smallB));

	/*
	 * Set smallL and smallH to the cumfreq values 
	 * respectively prior to and including symbol.
	 */
	smallT = m->totFreq;
	smallL = 0;
	for (i = 1; i < symbol; i++)
		smallL += m->freq[i];
	smallH = smallL + m->freq[symbol];

	smallR = bigR / smallT;

	smallR_x_smallL = smallR * smallL;
	bigL = bigL + smallR_x_smallL;

	if (smallH < smallT)
		bigR = smallR * (smallH - smallL);
	else
		bigR = bigR - smallR_x_smallL;

	arithCodeRenormalize_Encode(bs);

	if (bitsOutstanding > MAX_BITS_OUTSTANDING)
		panic("arithCodeSymbol: too many bits outstanding\n");
} /* arithCodeSymbol */

int32_t arithDecodeSymbol(struct bitstream_st *bs, struct Model const *m)
{
	u_int32_t smallL, smallH, smallT, smallR;
	u_int32_t smallR_x_smallL, target, symbol;

	smallT = m->totFreq;

	/* Get target value */
	smallR = bigR / smallT;
	target = minUInt32(smallT - 1, bigD / smallR);

	symbol = 0;
	smallH = 0;
	while (smallH <= target)
	{
		symbol++;
		smallH += m->freq[symbol];
	}
	smallL = smallH - m->freq[symbol];

	smallR_x_smallL = smallR * smallL;
	bigD = bigD - smallR_x_smallL;

	if (smallH < smallT)
		bigR = smallR * (smallH - smallL);
	else
		bigR = bigR - smallR_x_smallL;

	while (bigR <= TWO_TO_THE(smallB - 2))
	{
		bigR = 2 * bigR;
		bigD = 2 * bigD + bs_get_bit(bs);
	}

	return symbol;
} /* arithDecodeSymbol */


/*------------------------------------------------------*/
/* Generic frequency-table stuff			*/
/*------------------------------------------------------*/
void initModel(struct Model *m, char const *initName,
	int32_t const initNumSymbols, int32_t const initIncValue,
	int32_t const initNoExceed)
{
	int32_t i;

	if (initIncValue == 0)
	{
		m->totFreq = initNumSymbols;
		for (i = 1; i <= initNumSymbols; i++)
			m->freq[i] = 1;
	} else
	{
		m->totFreq = initNumSymbols * initIncValue;
		for (i = 1; i <= initNumSymbols; i++)
			m->freq[i] = initIncValue;
	}

	m->numSymbols = initNumSymbols;
	m->incValue = initIncValue;
	m->noExceed = initNoExceed;
	m->name = initName;
	m->freq[0] = 0;
	m->freq[initNumSymbols + 1] = 0;
	m->numScalings = 0;
} /* initModel */

void dumpModelStats(struct Model const *m)
{
	LOG_DEBUG(("model %s:\tscalings %d\n", m->name, m->numScalings));
} /* dumpModelStats */

void updateModel(struct Model *m, int32_t const symbol)
{
	u_int32_t i;

	m->totFreq += m->incValue;
	m->freq[symbol] += m->incValue;
	if (m->totFreq > m->noExceed)
	{
		m->totFreq = 0;
		m->numScalings++;
		for (i = 1; i <= m->numSymbols; i++)
		{
			m->freq[i] = (m->freq[i] + 1) >> 1;
			m->totFreq += m->freq[i];
		}
	}
} /* updateModel */

void putSymbol(struct Model *m, int32_t const symbol,
	struct bitstream_st *bs)
{
#ifdef CONFIG_DEBUG
	if (symbol < 1 || symbol > m->numSymbols)
		panic("putSymbol: mod = %s, sym = %d, max = %d\n",
			m->name, symbol, m->numSymbols);
#endif

	arithCodeSymbol(bs, m, symbol);
	updateModel(m, symbol);
} /* putSymbol */

int32_t getSymbol(struct Model *m, struct bitstream_st *bs)
{
	int32_t symbol;

	symbol = arithDecodeSymbol(bs, m);
	updateModel(m, symbol);

	assert(symbol >= 1 && symbol <= m->numSymbols);

	return symbol;
} /* getSymbol */


/*------------------------------------------------------*/
/* For sending bytes/words thru arith coder		*/
/*------------------------------------------------------*/
static struct Model bogusModel;

void initBogusModel(void)
{
	initModel(&bogusModel, "bogus", 256, 0, 256);
} /* initBogusModel */

void putUChar(struct bitstream_st *bs, unsigned char const c)
{
	putSymbol(&bogusModel, 1 + (u_int32_t)c, bs);
} /* putUChar */

void putInt32(struct bitstream_st *bs, int32_t const i)
{
	putUChar(bs, (unsigned char)(((u_int32_t)i >> 24) & 0xFF));
	putUChar(bs, (unsigned char)(((u_int32_t)i >> 16) & 0xFF));
	putUChar(bs, (unsigned char)(((u_int32_t)i >> 8) & 0xFF));
	putUChar(bs, (unsigned char)((u_int32_t)i & 0xFF));
} /* putInt32 */

void putUInt32(struct bitstream_st *bs, u_int32_t const i)
{
	putUChar(bs, (unsigned char)((i >> 24) & 0xFF));
	putUChar(bs, (unsigned char)((i >> 16) & 0xFF));
	putUChar(bs, (unsigned char)((i >> 8) & 0xFF));
	putUChar(bs, (unsigned char)(i & 0xFF));
} /* putUInt32 */

unsigned char getUChar(struct bitstream_st *bs)
{
	return (unsigned char)(getSymbol(&bogusModel, bs) - 1);
} /* getUChar */

int32_t getInt32(struct bitstream_st *bs)
{
	u_int32_t res = 0;

	res |= (getUChar(bs) << 24);
	res |= (getUChar(bs) << 16);
	res |= (getUChar(bs) << 8);
	res |= (getUChar(bs));

	return (int32_t)res;
} /* getInt32 */

u_int32_t getUInt32(struct bitstream_st *bs)
{
	u_int32_t res = 0;

	res |= (getUChar(bs) << 24);
	res |= (getUChar(bs) << 16);
	res |= (getUChar(bs) << 8);
	res |= (getUChar(bs));

	return res;
} /* getUInt32 */


/*------------------------------------------------------*/
/* The structured model proper				*/
/*------------------------------------------------------*/
/*							*/
/* The parameters in these models and bogusModel	*/
/* -- specifically, the value of 1000 for max-total-	*/
/* frequency -- determine the lowest acceptable values	*/
/* for smallF and indirectly smallB in the arithmetic	*/
/* coder above.						*/
/*------------------------------------------------------*/
void initModels(void)
{
	initModel(&models[BASIS], "basis", 11, 12, 1000);
	initModel(&models[MODEL_2_3], "2-3", 2, 4, 1000);
	initModel(&models[MODEL_4_7], "4-7", 4, 3, 1000);
	initModel(&models[MODEL_8_15], "8-15", 8, 3, 1000);
	initModel(&models[MODEL_16_31], "16-31", 16, 3, 1000);
	initModel(&models[MODEL_32_63], "32-63", 32, 3, 1000);
	initModel(&models[MODEL_64_127], "64-127", 64, 2, 1000);
	initModel(&models[MODEL_128_255], "128-255", 128, 1, 1000);
} /* initModels */

void dumpAllModelStats(void)
{
	dumpModelStats(&bogusModel);
	dumpModelStats(&models[BASIS]);
	dumpModelStats(&models[MODEL_2_3]);
	dumpModelStats(&models[MODEL_4_7]);
	dumpModelStats(&models[MODEL_8_15]);
	dumpModelStats(&models[MODEL_16_31]);
	dumpModelStats(&models[MODEL_32_63]);
	dumpModelStats(&models[MODEL_64_127]);
	dumpModelStats(&models[MODEL_128_255]);
} /* dumpAllModelStats */

int32_t getMTFVal(struct bitstream_st *bs)
{
	int32_t retVal;

	switch (getSymbol(&models[BASIS], bs))
	{
	case VAL_EOB:
		retVal = EOB;
		break;
	case VAL_RUNA:
		retVal = RUNA;
		break;
	case VAL_RUNB:
		retVal = RUNB;
		break;
	case VAL_ONE:
		retVal = 1;
		break;
	case VAL_2_3:
		retVal = getSymbol(&models[MODEL_2_3], bs) + 2 - 1;
		break;
	case VAL_4_7:
		retVal = getSymbol(&models[MODEL_4_7], bs) + 4 - 1;
		break;
	case VAL_8_15:
		retVal = getSymbol(&models[MODEL_8_15], bs) + 8 - 1;
		break;
	case VAL_16_31:
		retVal = getSymbol(&models[MODEL_16_31], bs) + 16 - 1;
		break;
	case VAL_32_63:
		retVal = getSymbol(&models[MODEL_32_63], bs) + 32 - 1;
		break;
	case VAL_64_127:
		retVal = getSymbol(&models[MODEL_64_127], bs) + 64 - 1;
		break;
	default:
		retVal = getSymbol(&models[MODEL_128_255], bs) + 128 - 1;
		break;
	}
	return retVal;
} /* getMFTVal */

void sendMTFVal(struct bitstream_st *bs, int32_t const n)
{
	if (n == RUNA)
		putSymbol(&models[BASIS], VAL_RUNA, bs);
	else if (n == RUNB)
		putSymbol(&models[BASIS], VAL_RUNB, bs);
	else if (n == EOB)
		putSymbol(&models[BASIS], VAL_EOB, bs);
	else if (n == 1)
		putSymbol(&models[BASIS], VAL_ONE, bs);
	else if (n >= 2 && n <= 3)
	{
		putSymbol(&models[BASIS], VAL_2_3, bs);
		putSymbol(&models[MODEL_2_3], n - 2 + 1, bs);
	} else if (n >= 4 && n <= 7)
	{
		putSymbol(&models[BASIS], VAL_4_7, bs);
		putSymbol(&models[MODEL_4_7], n - 4 + 1, bs);
	} else if (n >= 8 && n <= 15)
	{
		putSymbol(&models[BASIS], VAL_8_15, bs);
		putSymbol(&models[MODEL_8_15], n - 8 + 1, bs);
	} else if (n >= 16 && n <= 31)
	{
		putSymbol(&models[BASIS], VAL_16_31, bs);
		putSymbol(&models[MODEL_16_31], n - 16 + 1, bs);
	} else if (n >= 32 && n <= 63)
	{
		putSymbol(&models[BASIS], VAL_32_63, bs);
		putSymbol(&models[MODEL_32_63], n - 32 + 1, bs);
	} else if (n >= 64 && n <= 127)
	{
		putSymbol(&models[BASIS], VAL_64_127, bs);
		putSymbol(&models[MODEL_64_127], n - 64 + 1, bs);
	} else if (n >= 128 && n <= 255)
	{
		putSymbol(&models[BASIS], VAL_128_255, bs);
		putSymbol(&models[MODEL_128_255], n - 128 + 1, bs);
	} else
		panic("sendMTFVal: bad value!\n");
} /* sendMFTVal */


/*------------------------------------------------------*/
/* Move-to-front encoding/decoding			*/
/*------------------------------------------------------*/
/*							*/
/* These are the main data structures for the		*/
/* Burrows-Wheeler transform.				*/
/*							*/
/* For good performance, fullGt() allows pointers to	*/
/* get partially denormalized.  As a consequence, we	*/
/* have to copy some small quantity of data from the	*/
/* beginning of a block to the end of it so things	*/
/* still work right.  These constants control that.	*/
/*------------------------------------------------------*/
u_int32_t GETALL(int32_t const a)
{
	assert(a >= 0 && a < lastPP + 4 * NUM_FULLGT_UNROLLINGS);
	if (a >= lastPP)
		assert(words[a] == words[a - lastPP]);

	return words[a];
} /* GETALL */

void SETREST16(int32_t const a, u_int32_t const w)
{
	words[a] = (words[a] & 0xffff0000)
		| (((u_int32_t) (w)) & 0x0000ffff);
} /* SETREST16 */

void SETFIRST16(int32_t const a, u_int32_t const w)
{
	words[a] = (words[a] & 0x0000ffff)
		| (((u_int32_t) (w)) << 16);
} /* SETFIRST16 */

void SETREST(int32_t const a, u_int32_t const w)
{
	words[a] = (words[a] & 0xff000000)
		| (((u_int32_t) (w)) & 0x00ffffff);
} /* SETREST */

void SETFIRST(int32_t const a, unsigned char const c)
{
	words[a] = (words[a] & 0x00ffffff)
		| (((u_int32_t) (c)) << 24);
} /* SETFIRST */

void SETSECOND(int32_t const a, unsigned char const c)
{
	words[a] = (words[a] & 0xff00ffff)
		| (((u_int32_t) (c)) << 16);
} /* SETSECOND */

void SETTHIRD(int32_t const a, unsigned char const c)
{
	words[a] = (words[a] & 0xffff00ff)
		| (((u_int32_t) (c)) << 8);
} /* SETTHIRD */

void SETFOURTH(int32_t const a, unsigned char const c)
{
	words[a] = (words[a] & 0xffffff00)
		| (((u_int32_t) (c)));
} /* SETFOURTH */

int32_t NORMALIZE(int32_t const p)
{
	if (p < 0)
		return p + lastPP;
	else
		return p < lastPP ? p : p - lastPP;
} /* NORMALIZE */

int32_t NORMALIZEHI(int32_t const p)
{
	return p < lastPP ? p : p - lastPP;
} /* NORMALIZEHI */

int32_t NORMALIZELO(int32_t const p)
{
	return p < 0 ? p + lastPP : p;
} /* NORMALIZELO */

/*
 * The above normalizers are quick but only work when
 * p exceeds the block by less than lastPP, since
 * they renormalize merely by adding or subtracting
 * lastPP.  This one always works, although slowly.
 */
int32_t STRONG_NORMALIZE(int32_t p)
{
	/*
	 * -ve number MOD +ve number always
	 * was one of life's little mysteries...
	 */
	while (p < 0)
		p += lastPP;

	return p % lastPP;
} /* STRONG_NORMALIZE */

void sendZeroes(struct bitstream_st *outStream, int32_t zeroesPending)
{
	u_int32_t bitsToSend;
	int32_t numBits;

	if (zeroesPending == 0)
		return;

	bitsToSend = 0;
	numBits = 0;
	while (zeroesPending != 0)
	{
		numBits++;
		bitsToSend <<= 1;
		zeroesPending--;
		if ((zeroesPending & 0x1) == 1)
			bitsToSend |= 1;
		zeroesPending >>= 1;
	}
	
	while (numBits > 0)
	{
		if ((bitsToSend & 0x1) == 1)
			sendMTFVal(outStream, RUNA);
		else
			sendMTFVal(outStream, RUNB);
		bitsToSend >>= 1;
		numBits--;
	}
} /* sendZeroes */

void moveToFrontCodeAndSend(struct bitstream_st *outStream,
	unsigned const thisIsTheLastBlock)
{
	unsigned char yy[256];
	int32_t i, j;
	unsigned char tmp;
	unsigned char tmp2;
	int32_t zeroesPending;

	zeroesPending = 0;
	if (thisIsTheLastBlock)
		putInt32(outStream, -(origPtr + 1));
	else
		putInt32(outStream, (origPtr + 1));

	initModels();
	memcpy(yy, init_yy, sizeof(yy));

	for (i = 0; i <= last; i++)
	{
		unsigned char ll_i;

		ll_i = GETFIRST(NORMALIZELO(zptr[i] - 1));

		j = 0;
		tmp = yy[j];
		while (ll_i != tmp)
		{
			j++;
			tmp2 = tmp;
			tmp = yy[j];
			yy[j] = tmp2;
		};
		yy[0] = tmp;

		if (j == 0)
		{
			zeroesPending++;
		} else
		{
			sendZeroes(outStream, zeroesPending);
			zeroesPending = 0;
			sendMTFVal(outStream, j);
		}
	}

	sendZeroes(outStream, zeroesPending);
	sendMTFVal(outStream, EOB);
} /* moveToFrontCodeAndSend */

int getAndMoveToFrontDecode(struct bitstream_st *inStream,
	unsigned limit)
{
	unsigned char yy[256];
	int32_t tmpOrigPtr, nextSym;

	tmpOrigPtr = getInt32(inStream);
	if (tmpOrigPtr < 0)
		origPtr = (-tmpOrigPtr) - 1;
	else
		origPtr = tmpOrigPtr - 1;

	initModels();
	memcpy(yy, init_yy, sizeof(yy));

	last = -1;

	nextSym = getMTFVal(inStream);

LOOPSTART:

	if (nextSym == EOB)
		return (tmpOrigPtr < 0);

	/* Acquire run-length bits, most significant first */
	if (nextSym == RUNA || nextSym == RUNB)
	{
		int32_t n = 0;
		do
		{
			n <<= 1;
			if (nextSym == RUNA)
				n |= 1;
			n++;
			nextSym = getMTFVal(inStream);
		} while (nextSym == RUNA || nextSym == RUNB);

		while (n > 0)
		{
			last++;
			if (last >= limit)
			{
				bz_log(BZIP_LOG_ERROR,
					"%s: file is corrupt\n",
					inStream->fname);
				longjmp(exception_handler,
					EXIT_ERR_CORRUPT);
			}

			ll[last] = yy[0];
			n--;
		}
		goto LOOPSTART;
	}

	if (nextSym >= 1 && nextSym <= 255)
	{
		last++;
		if (last >= limit)
		{
			bz_log(BZIP_LOG_ERROR, "%s: file is corrupt\n",
				inStream->fname);
			longjmp(exception_handler, EXIT_ERR_CORRUPT);
		}

		ll[last] = yy[nextSym];

		memmove(&yy[1], yy, nextSym);

		yy[0] = ll[last];
		nextSym = getMTFVal(inStream);
		goto LOOPSTART;
	}

	panic("bad MTF value %d\n", nextSym);
} /* getAndMoveToFrontDecode */


/*------------------------------------------------------*/
/* Block-sorting machinery				*/
/*------------------------------------------------------*/

/* Doesn't work when lastPP < 4 */
void stripe(void)
{
	int32_t i;

	for (i = 0; i < lastPP; i++)
	{
		unsigned char c = GETFIRST(i);
		SETSECOND(NORMALIZELO(i - 1), c);
		SETTHIRD(NORMALIZELO(i - 2), c);
		SETFOURTH(NORMALIZELO(i - 3), c);
	}
} /* stripe */

/* Doesn't work when lastPP < 4 * NUM_FULLGT_UNROLLINGS */
void copyOffsetWords(void)
{
	int32_t i;

	for (i = 0; i < 4 * NUM_FULLGT_UNROLLINGS; i++)
		words[lastPP + i] = words[i];
} /* copyOffsetWords */

/* Doesn't work when lastPP < 4 * NUM_FULLGT_UNROLLINGS */
int fullGt(int32_t i1, int32_t i2)
{
	int32_t i1orig = i1;

	if (i1 == i2)
		return 0;

	do
	{
		u_int32_t w1;
		u_int32_t w2;

		assert(i1 >= 0);
		assert(i2 >= 0);
		assert(i1 != i2);
		assert(i1 < lastPP);
		assert(i2 < lastPP);

		w1 = GETALL(i1);
		w2 = GETALL(i2);
		if (w1 != w2)
			return (w1 > w2);
		i1 += 4;
		i2 += 4;

		assert(i1 < lastPP + 1 * NUM_FULLGT_UNROLLINGS);
		assert(i2 < lastPP + 1 * NUM_FULLGT_UNROLLINGS);

		w1 = GETALL(i1);
		w2 = GETALL(i2);
		if (w1 != w2)
			return (w1 > w2);
		i1 += 4;
		i2 += 4;

		assert(i1 < lastPP + 2 * NUM_FULLGT_UNROLLINGS);
		assert(i2 < lastPP + 2 * NUM_FULLGT_UNROLLINGS);

		w1 = GETALL(i1);
		w2 = GETALL(i2);
		if (w1 != w2)
			return (w1 > w2);
		i1 += 4;
		i2 += 4;

		assert(i1 < lastPP + 3 * NUM_FULLGT_UNROLLINGS);
		assert(i2 < lastPP + 3 * NUM_FULLGT_UNROLLINGS);

		w1 = GETALL(i1);
		w2 = GETALL(i2);
		if (w1 != w2)
			return (w1 > w2);
		i1 += 4;
		i2 += 4;

		assert(i1 < lastPP + 4 * NUM_FULLGT_UNROLLINGS);
		assert(i2 < lastPP + 4 * NUM_FULLGT_UNROLLINGS);

		i1 = NORMALIZEHI(i1);
		i2 = NORMALIZEHI(i2);

		assert(i1 >= 0);
		assert(i2 >= 0);
		assert(i1 != i2);
		assert(i1 < lastPP);
		assert(i2 < lastPP);

	} while (i1 != i1orig);

	return 0;
} /* fullGt */

/*
 * Requires striping, and therefore doesn't
 * work when lastPP < 4.  This qsort is 
 * derived from Weiss' book "Data Structures and
 * Algorithm Analysis in C", Section 7.7.
 */
void qsortFull(int32_t const left, int32_t const right)
{
	int32_t pivot, v;
	int32_t i, j;
	int32_t wuC;

	int32_t stackL[40];
	int32_t stackR[40];
	int32_t sp = 0;

	int32_t wuL = left;
	int32_t wuR = right;

	while (1)
	{

		/*
		 * At the beginning of this loop, wuL and wuR hold the
		 * bounds of the next work-unit.
		 */
		if (wuR - wuL > ISORT_BELOW)
		{

			/* A large Work Unit; partition-exchange */
			wuC = (wuL + wuR) >> 1;

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

				if (i < j)
					SWAP(i, j);
				else
					break;
			}
			SWAP(i, wuR - 1);

			if ((i - wuL) > (wuR - i))
			{
				stackL[sp] = wuL;
				stackR[sp] = i - 1;
				sp++;
				wuL = i + 1;
			} else
			{
				stackL[sp] = i + 1;
				stackR[sp] = wuR;
				sp++;
				wuR = i - 1;
			}

			assert(sp <= 14);
		} else
		{

			/* A small Work-Unit; insertion-sort it */
			for (i = wuL + 1; i <= wuR; i++)
			{
				v = zptr[RC(i)];
				j = i;
				while (fullGt(zptr[RC(j - 1)], v))
				{
					zptr[RC(j)] = zptr[RC(j - 1)];
					j = j - 1;
					if (j <= wuL)
						break;
				}
				zptr[RC(j)] = v;
			}
			if (sp == 0)
				return;
			sp--;
			wuL = stackL[sp];
			wuR = stackR[sp];

			assert(sp >= 0);
		} /* if this is a small work-unit */
	}
} /* qsortFull */

/*
 * Use of NORMALIZEHI is safe here, for any
 * lastPP >= 1 (which is guaranteed), since
 * the max denormalisation is 1.
 */
int trivialGt(int32_t i1, int32_t i2)
{
	int32_t k;

	for (k = 0; k <= last; k++)
	{
		unsigned char c1 = GETFIRST(i1);
		unsigned char c2 = GETFIRST(i2);
		if (c1 == c2)
		{
			i1++;
			i1 = NORMALIZEHI(i1);
			i2++;
			i2 = NORMALIZEHI(i2);
		} else
			return (c1 > c2);
	};
	return 0;
} /* trivialGt */

/* Always works */
void shellTrivial(void)
{
	int32_t i, j, h, bigN;
	int32_t v;

	int32_t ptrLo = 0;
	int32_t ptrHi = last;
	bigN = ptrHi - ptrLo + 1;
	h = 1;
	do
	{
		h = 3 * h + 1;
	} while (!(h > bigN));

	do
	{
		h = h / 3;
		for (i = ptrLo + h; i <= ptrHi; i++)
		{
			v = zptr[i];
			j = i;

			while (trivialGt(zptr[j - h], v))
			{
				zptr[j] = zptr[j - h];
				j = j - h;
				if (j <= (ptrLo + h - 1))
					goto zero;
			}
zero:
			zptr[j] = v;
		}
	} while (h != 1);
} /* shellTrivial */

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
 */
void sortIt(void)
{
	/*
	 * lastPP is `last++', ie, is always == last + 1.  
	 * The two (lastPP and last + 1) should be interchangeable.
	 * lastPP is more convenient for fast renormalisation,
	 * that's all.
	 *
	 * In the various block-sized structures, live data runs
	 * from 0 to last inclusive, so lastPP is the number
	 * of live data items.
	 */
	lastPP = last + 1;

	if (lastPP <= 1024)
	{

		/*
		 * shellTrivial *must* be used for
		 * lastPP <= 4 * NUM_FULLGT_UNROLLINGS.
		 * The 1024 limit is much higher; the
		 * purpose is to avoid lumbering sorting
		 * of small blocks with the fixed overhead
		 * of the full counting-sort mechanism.
		 */

		int32_t i;

		LOG_DEBUG(("trivialSort... "));
		for (i = 0; i <= last; i++)
			zptr[i] = i;
		shellTrivial();
		LOG_DEBUG(("done.\n"));
	} else
	{
		int32_t i;
		int32_t grade;
		int32_t notDone;

		stripe();

		LOG_DEBUG(("bucket sorting...\n"));

		memset(ftab, 0, 65537 * sizeof(int32_t));
		for (i = 0; i <= last; i++)
			ftab[GETFIRST16(i)]++;
		for (i = 1; i <= 65536; i++)
			ftab[i] += ftab[i - 1];

		for (i = 0; i <= last; i++)
		{
			u_int32_t j = GETFIRST16(i);
			ftab[j]--;
			zptr[ftab[j]] = i;
		}

		copyOffsetWords();

		notDone = lastPP;
		for (grade = 1; grade <= 5; grade++)
		{
			int32_t candNo;
			int32_t loBound;
			int32_t hiBound;

			switch (grade)
			{
			case 1:
				loBound = 2;
				hiBound = 15;
				break;
			case 2:
				loBound = 16;
				hiBound = 255;
				break;
			case 3:
				loBound = 256;
				hiBound = 4095;
				break;
			case 4:
				loBound = 4096;
				hiBound = 65535;
				break;
			case 5:
				loBound = 65536;
				hiBound = 900000;
				break;
			default:
				panic("gradedSort\n");
				break;
			}
			if (loBound > lastPP)
				continue;

			candNo = 0;
			for (i = 0; i <= 65535; i++)
			{
				int32_t freqHere = ftab[i + 1] - ftab[i];

				if (freqHere >= loBound
					&& freqHere <= hiBound)
				{
					int32_t j, k;
					int32_t lower = ftab[i];
					int32_t upper = ftab[i + 1] - 1;

					candNo++;
					notDone -= freqHere;

					LOG_DEBUG(("   %d -> %d:  "
						"cand %5d,"
						"freq = %6d,   "
						"notdone = %6d\n",
						loBound, hiBound, candNo,
						freqHere, notDone));

					qsortFull(lower, upper);

					if (freqHere < 65535)
					{
						for (j = lower, k = 0;
							j <= upper;
							j++, k++)
						{
							int32_t a2update = zptr[j];
							SETREST16(a2update, k);
							if (a2update < (4 * NUM_FULLGT_UNROLLINGS))
								SETREST16(a2update + lastPP, k);
						}
					}
				}
			}
		}
	}
} /* sortIt */


/*------------------------------------------------------*/
/* The Reversible Transformation (tm)			*/
/*------------------------------------------------------*/

/*
 * Use: block [0 .. last]
 * Def: origPtr.  ll [0 .. last] is synthesized later.
 */
void doReversibleTransformation(void)
{
	sortIt();

	for (origPtr = 0; origPtr <= last; origPtr++)
		if (zptr[origPtr] == 0)
			return;

	panic("doReversibleTransformation\n");
} /* doReversibleTransformation */

/*
 * Use: ll[0 .. last] and origPtr
 * Def: block[0 .. last]
 */
void undoReversibleTransformation(void)
{
	int32_t cc[256];
	int32_t i, j, ch, sum, orig_sum;

	memset(cc, 0, sizeof(cc));

	for (i = 0; i <= last; i++)
	{
		unsigned char ll_i = ll[i];
		zptr[i] = cc[ll_i];
		cc[ll_i]++;
	};

	sum = 0;
	for (ch = 0; ch <= 255; ch++)
	{
		orig_sum = sum;
		sum += cc[ch];
		cc[ch] = orig_sum;
	};

	i = origPtr;
	for (j = last; j >= 0; j--)
	{
		unsigned char ll_i = ll[i];
		block[j] = ll_i;
		i = zptr[i] + cc[ll_i];
	};
} /* undoReversibleTransformation */


/*------------------------------------------------------*/
/* The block loader and RLEr				*/
/*------------------------------------------------------*/
void spotBlock(unsigned const weAreCompressing)
{
	int delta;
	int32_t pos;
	static int const newdeltas[] =
		{ 0, 4, 6, 1, 5, 9, 7, 3, 8, 2 };

	pos = SPOT_BASIS_STEP;
	delta = 1;

	while (pos < last)
	{
		int32_t n;

		if (weAreCompressing)
			n = (int32_t) GETFIRST(pos) + 1;
		else
			n = (int32_t) block[pos] - 1;

		if (n == 256)
			n = 0;
		else if (n == -1)
			n = 255;

		if (n < 0 || n > 255)
			panic("spotBlock\n");

		if (weAreCompressing)
			SETFIRST(pos, (unsigned char) n);
		else
			block[pos] = (unsigned char) n;

		delta = newdeltas[delta];
		pos += SPOT_BASIS_STEP + 17 * (delta - 5);
	}
} /* spotBlock */

int getRLEpair(struct bitstream_st *ibs, unsigned *runLength)
{
	int ch, chLatest;
	unsigned ret_runLength;

	*runLength = 0;

	if ((ch = bs_get_byte(ibs)) == -1)
		return -1;

	ret_runLength = 0;
	do
	{
		chLatest = bs_get_byte(ibs);
		ret_runLength++;
	} while (ch == chLatest && ret_runLength < 255);

	if (chLatest != -1)
		bs_unget_byte(ibs, chLatest);

	*runLength = ret_runLength;

	do
		globalCrc = updateCRC(globalCrc, ch);
	while (--ret_runLength > 0);

	return ch;
} /* getRLEpair */

unsigned loadAndRLEsource(struct bitstream_st *ibs,
	unsigned const blocksize)
{
	unsigned runLen;
	int ch, allowableBlockSize;

	last = -1;

	/* 20 is just a paranoia constant */
	allowableBlockSize = blocksize - 20;

	while (last < allowableBlockSize)
	{
		ch = getRLEpair(ibs, &runLen);

		if (ch == -1)
		{
			last++;
			SETFIRST(last, ((unsigned char)42));

			return 1;
		}

		assert(runLen >= 1 && runLen <= 255);
		switch (runLen)
		{
		case 3:
			last++;
			SETFIRST(last, ((unsigned char)ch));
		case 2:
			last++;
			SETFIRST(last, ((unsigned char)ch));
		case 1:
			last++;
			SETFIRST(last, ((unsigned char)ch));
			break;

		default:
			last++;
			SETFIRST(last, ((unsigned char)ch));
			last++;
			SETFIRST(last, ((unsigned char)ch));
			last++;
			SETFIRST(last, ((unsigned char)ch));
			last++;
			SETFIRST(last, ((unsigned char)ch));
			last++;
			SETFIRST(last, ((unsigned char)(runLen - 4)));
			break;
		} /* switch */
	} /* while */

	return 0;
} /* loadAndRLEsource */

/*
 * This new version is derived from some code
 * sent to me Christian von Roques.
 */
void unRLEandDump(struct bitstream_st *obs,
	unsigned const thisIsTheLastBlock)
{
	int chPrev, ch;
	unsigned count;
	int32_t lastcharToSpew, i;
	u_int32_t localCrc;

	lastcharToSpew = last;
	if (thisIsTheLastBlock)
		lastcharToSpew--;

	count = 0;
	i = 0;
	ch = -1;
	localCrc = getGlobalCRC();

	while (i <= lastcharToSpew)
	{
		chPrev = ch;
		ch = block[i];
		i++;

		bs_put_byte(obs, ch);
		localCrc = updateCRC(localCrc, (unsigned char)ch);

		if (ch == chPrev)
		{
			count++;
			if (count >= 4)
			{
				int32_t j;
				for (j = 0; j < (int32_t)block[i]; j++)
				{
					bs_put_byte(obs, ch);
					localCrc = updateCRC(localCrc,
						(unsigned char)ch);
				}
				i++;
				count = 0;
			}
		} else
			count = 1;
	} /* while */

	setGlobalCRC(localCrc);

	if (thisIsTheLastBlock && block[last] != 42)
	{
		bz_log(BZIP_LOG_ERROR, "%s: file is corrupt\n",
			obs->fname);
		longjmp(exception_handler, EXIT_ERR_CORRUPT);
	}
} /* unRLEandDump */


/*------------------------------------------------------*/
/* Error [non-] handling grunge				*/
/*------------------------------------------------------*/
void exiterror(unsigned const exitcode, char const *fmt, ...)
{
	va_list printf_args;

	va_start(printf_args, fmt);
	fprintf(stderr, "%s: ", bzip_prgname);
	vfprintf(stderr, fmt, printf_args);
	va_end(printf_args);
	exit(exitcode);
} /* exiterror */

void panic(char const *fmt, ...)
{
	va_list printf_args;

	/* It's better not to cleanup */
	va_start(printf_args, fmt);
	fprintf(stderr, "%s: INTERNAL PROGRAM ERROR: ", bzip_prgname);
	vfprintf(stderr, fmt, printf_args);
	fprintf(stderr, "Please send a detailed bugreport "
		"to the author.  Thanks in advance.\n");
	va_end(printf_args);

	exit(EXIT_ERR_INSIDE);
} /* panic */

#ifdef CONFIG_DEBUG
/*------------------------------------------------------*/
/* Debugging						*/
/*------------------------------------------------------*/
void bz_log_debug(char const *fmt, ...)
{
	va_list printf_args;

	if (bzip_verbosity >= BZIP_LOG_DEBUG)
	{
		va_start(printf_args, fmt);
		fprintf(stderr, "%s: ", bzip_prgname);
		vfprintf(stderr, fmt, printf_args);
		va_end(printf_args);
	}
} /* bz_log_debug */

void dump_variable(char const *name, void const *addr, unsigned size)
{
	unsigned i;

	fprintf(stderr, "%s: ", name);
	for (i = 0; i < size; i++)
	{
		fprintf(stderr, "%X ", *((char *)addr + i));
		if (i % 35 == 0)
			fprintf(stderr, "\n");
	}
	fprintf(stderr, "\n\n");
} /* dump_variable */

void dump_global_variables(int const blocksize, char const *id)
{
	static unsigned count = 0;

	fprintf(stderr, "dump_state follows: %s (%u)\n\n",
		id, count++);

	DUMP_VAR(bigD);
	DUMP_VAR(bigL);
	DUMP_VAR(bigR);
	DUMP_VAR(bitsOutstanding);
	DUMP_PTR(block, blocksize);
	DUMP_VAR(bogusModel);
	DUMP_PTR(ftab, 65537 * sizeof(int32_t));
	DUMP_VAR(last);
	DUMP_VAR(lastPP);
	DUMP_PTR(ll, blocksize);
	DUMP_VAR(models);
	DUMP_VAR(origPtr);
	DUMP_PTR(words, (blocksize + MAX_DENORM_OFFSET) * sizeof(int32_t));
	DUMP_PTR(zptr, blocksize * sizeof(int32_t));
} /* dump_global_variables */
#endif /* CONFIG_DEBUG */

/*------------------------------------------------------*/
/* The main driver machinery				*/
/*------------------------------------------------------*/
unsigned compress(struct bitstream_st *ibs, struct bitstream_st *obs,
	unsigned const clevel)
{
	double cratio;
	u_int32_t crcComputed;
	unsigned exp_code, blockNo, blocksize, thisIsTheLastBlock;

	/* Install exception handler */
	if ((exp_code = setjmp(exception_handler)))
		goto cleanup;

	/* Allocate compressing structures */
	blocksize = clevel * 100000;

	words = bz_malloc((blocksize + MAX_DENORM_OFFSET)
		* sizeof(int32_t));
	zptr = bz_malloc(blocksize * sizeof(int32_t));
	ftab = bz_malloc(65537 * sizeof(int32_t));

	/* Write magic */
	bs_put_byte(obs, 'B');
	bs_put_byte(obs, 'Z');
	bs_put_byte(obs, BZIP_FORMAT_CODE);
	bs_put_byte(obs, '0' + clevel);

	initializeCRC();
	initBogusModel();
	arithCodeStartEncoding();

	blockNo = 0;
	do
	{
		LOG_DEBUG(("Begin block %u\n", blockNo++));
		thisIsTheLastBlock = loadAndRLEsource(ibs, blocksize);
		spotBlock(1);
		doReversibleTransformation();
		moveToFrontCodeAndSend(obs, thisIsTheLastBlock);
	} while (!thisIsTheLastBlock);

	crcComputed = getFinalCRC();
	bz_log(BZIP_LOG_DEBUG, "CRC: 0x%.8X\n", crcComputed);
	putUInt32(obs, crcComputed);

	arithCodeDoneEncoding(obs);
	bs_close(obs);

	dumpAllModelStats();

	cratio = ibs->processed
		? (obs->processed / ibs->processed) * 100.0
		: 0;

	bz_log(BZIP_LOG_INFO, "processed %u bytes, compression "
		"ratio is %5.2f (%u bytes)\n", ibs->processed,
		cratio, obs->processed);

	exp_code = 0;

cleanup:
	free(words);
	free(zptr);
	free(ftab);

	return exp_code;
} /* compress */

unsigned decompress(struct bitstream_st *ibs, struct bitstream_st *obs)
{
	double cratio;
	u_int32_t crcStored, crcComputed;
	unsigned exp_code, blockNo, blocksize, thisIsTheLastBlock;

	/* Install exception handler */
	if ((exp_code = setjmp(exception_handler)))
		goto cleanup;

	/* Check magic */
	if (bs_get_byte(ibs) != 'B' || bs_get_byte(ibs) != 'Z'
		|| bs_get_byte(ibs) != BZIP_FORMAT_CODE
		|| (blocksize = bs_get_byte(ibs)) < '1')
	{
		bz_log(BZIP_LOG_ERROR, "%s: invalid magic\n",
			ibs->fname);
		return EXIT_ERR_MAGIC;
	}

	/* Allocate decompressing structures */
	blocksize = (blocksize - '0') * 100000;
	block = bz_malloc(blocksize);
	ll = bz_malloc(blocksize);
	zptr = bz_malloc(blocksize * sizeof(int32_t));

	initializeCRC();
	initBogusModel();
	arithCodeStartDecoding(ibs);

	blockNo = 0;
	do
	{
		LOG_DEBUG(("[ %u: ac + mtf ", blockNo++));
		thisIsTheLastBlock = getAndMoveToFrontDecode(ibs,
			blocksize);

		LOG_DEBUG(("rt"));
		undoReversibleTransformation();
		spotBlock(0);

		LOG_DEBUG(("rld"));
		unRLEandDump(obs, thisIsTheLastBlock);

		LOG_DEBUG(("]\n"));
	} while (!thisIsTheLastBlock);

	crcStored = getUInt32(ibs);
	crcComputed = getFinalCRC();
	bz_log(BZIP_LOG_DEBUG, "stored CRC: 0x%.8X, "
		"computed CRC: 0x%.8X\n  ", crcStored, crcComputed);
	if (crcStored != crcComputed)
	{
		bz_log(BZIP_LOG_ERROR, "CRC error\n");
		return EXIT_ERR_CORRUPT;
	}

	arithCodeDoneDecoding();
	bs_close(obs);

	cratio = ibs->processed
		? (ibs->processed / obs->processed) * 100.0
		: 0;

	bz_log(BZIP_LOG_INFO, "processed %u bytes, compression "
		"ratio is %5.2f (%u bytes)\n", ibs->processed,
		cratio, obs->processed);

	exp_code = 0;

cleanup:
	free(block);
	free(ll);
	free(zptr);

	return exp_code;
} /* decompress */

void output_cleanup(struct bitstream_st *bs)
{
	/* This code contains a race condition.  Not a serious one
	 * but...  Could anyone suggest me a (reasonably cheap)
	 * solution to avoid it? */
	if (bs && bs->fname && bs->fd != STDOUT_FILENO)
	{
		unlink(bs->fname);
		bs->fname = NULL;
	}
} /* output_cleanup */

int output_generate_filename(char *ofname, unsigned const ofname_size,
	const char *ifname, unsigned const compress)
{
	unsigned ifname_len;

	ifname_len = strlen(ifname);
	if (compress)
	{
		if (ifname_len + sizeof(BZIP_EXT) > ofname_size)
			return -1;
		memcpy(ofname, ifname, ifname_len);
		memcpy(&ofname[ifname_len], BZIP_EXT, sizeof(BZIP_EXT));
		return 1;
	} else
	{
		int base_len;

		base_len = ifname_len - (sizeof(BZIP_EXT) - 1);
		if (base_len > 0 && !memcmp(&ifname[base_len],
			BZIP_EXT, sizeof(BZIP_EXT)))
		{
			if (base_len + 1 > ofname_size)
				return -1;
			memcpy(ofname, ifname, base_len);
			ofname[base_len] = '\0';
			return 1;
		} else
		{ /* Generate a (hopefully) unique temporary name */
			struct timeval tm;

			gettimeofday(&tm, NULL);
			sprintf(ofname, "%s.%u.%lu",
				bzip_prgname, getpid(), tm.tv_usec);
			return 0;
		}
	} /* if */
} /* output_generate_filename */

int bz_open(char const *fname, int const flags,
	mode_t const mode, unsigned const paranoid)
{
	int retval;

	if (paranoid)
		if (flags & O_EXCL)
		{
#ifdef HAVE_LSTAT
			struct stat statbuf;

			if (lstat(fname, &statbuf) == 0)
				errno = EEXIST;
			if (errno != ENOENT)
			{
				bz_log(BZIP_LOG_WARNING, "lstat: %s: %s\n",
					fname, strerror(errno));
				return -1;
			}
#endif /* HAVE_LSTAT */
		} else
			unlink(fname);

	if ((retval = open(fname, flags, mode)) < 0)
		bz_log(BZIP_LOG_WARNING, "open: %s: %s\n",
			fname, strerror(errno));
	return retval;
} /* bz_open */

void my_sigint(int const signum)
{
	output_cleanup(output_bs);
	exit(EXIT_TERMINATED);
} /* my_sigint */

void my_sigsegv(int const signum)
{
	panic("signal %d caught.\n", signum);
} /* my_sigsegv */

void bz_log(int const severity, char const *fmt, ...)
{
	va_list printf_args;

	if (bzip_verbosity >= severity)
	{
		va_start(printf_args, fmt);
		fprintf(stderr, "%s: ", bzip_prgname);
		vfprintf(stderr, fmt, printf_args);
		va_end(printf_args);
	}
} /* bz_log */

/* The main function */
int main(int argc, char *argv[])
{
	struct bitstream_st ibs, obs;
	int output_dontopen, output_dontclose;
	int optchar, output_flags, config_perms;
	unsigned compression_level, keep_input, tolerant, paranoid;

	/* Figure out program basename */
	if ((bzip_prgname = strrchr(argv[0], '/')) == NULL)
		bzip_prgname = argv[0];
	else
		bzip_prgname++;

	/* Set up defaults */
	compression_level = strcmp(bzip_prgname, "bunzip")
		? DFLT_COMPRESSION_LEVEL : 0;

	obs.fname = NULL;
	obs.window_end = obs.window + sizeof(obs.window);
	output_bs = &obs;

	output_dontopen = output_dontclose = 0;
	output_flags = O_CREAT | O_TRUNC | O_WRONLY;
	config_perms = -1;
	keep_input = 0;
	tolerant = 0;
	paranoid = 1;

	/* Parameter processing */
	opterr = 0;
	while ((optchar = getopt(argc, argv, cmdopts)) != EOF)
		switch (optchar)
		{
		/* General options */
		case OPS_VERBOSE:
			bzip_verbosity++;
			break;

		case OPS_QUIET:
			bzip_verbosity--;
			break;

		case OPS_TOLERANT:
			tolerant = 1;
			break;

		case OPS_BLIND:
			paranoid = 0;
			break;

		/* Operation modes */
		case OPS_COMPRESS:
			compression_level = DFLT_COMPRESSION_LEVEL;
			break;

		case OPS_DECOMPRESS:
			compression_level = 0;
			break;

		/* I/O options */
		case OPS_OUTPUT:
			obs.fname = optarg;
			keep_input = 1;
			break;

		case OPS_STDOUT:
			obs.fname = "-";
			keep_input = 1;
			break;

		case OPS_KEEPINPUT:
			keep_input = 1;
			break;

		case OPS_CAREFUL:
			output_flags |= O_EXCL;
			break;

		case OPS_PERMS:
			config_perms = strtoul(optarg, NULL, 0);
			break;

		/* Compression level */
		case OPS_BLOCKSIZE:
			compression_level = atoi(optarg);
			if (compression_level + '0' > 255)
				exiterror(EXIT_ERR_ARG, "%u: too big "
					"block size requested\n",
					compression_level);
			break;

		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			compression_level = optchar - '0';
			break;

		/* Common options */
		case OPS_VERSION:
			printf("%s", "bzip " BZIP_VERSION "\n");
			exit(EXIT_HAPPILY);
			break;

		case OPS_HELP:
			printf(msg_usage, bzip_prgname);
			exit(EXIT_HAPPILY);
			break;

		/* Unknown */
		default:
			fprintf(stderr, "%s: invalid option -- `%c'\n"
				"Try %s -h for more information.\n",
				bzip_prgname, optopt, bzip_prgname);
			exit(EXIT_ERR_ARG);
			break;
		} /* switch in while */
	
	if (optind > argc - 1)
	{ /* Zero input */
		ibs.fname = "-";

		/* Zero output */
		if (!obs.fname)
			obs.fname = "-";
	} else
	{ /* One or more input */
		ibs.fname = argv[optind];

		/* More input, one output */
		if (optind < argc - 1 && obs.fname)
			if (compression_level)
			{
				bz_log(BZIP_LOG_ERROR, "too many input "
					"files are specified\n");
				exit(EXIT_ERR_ARG);
			} else
				output_dontclose = 1;

		optind++;
	}

	if (obs.fname && obs.fname[0] == '-' && obs.fname[1] == '\0')
	{
		output_dontopen = output_dontclose = 1;
		obs.fd = STDOUT_FILENO;
		obs.fname = "stdout";
	}

	/* Install signal handlers */
	signal(SIGSEGV, my_sigsegv);
	signal(SIGBUS, my_sigsegv);
	signal(SIGINT, my_sigint);
	signal(SIGTERM, my_sigint);

	/* The main loop */
	do
	{
		char fnamebuf[PATH_MAX];
		int input_bz, output_perms, retval;

		/* Open input file */
		ibs.windowp = ibs.window_end = &ibs.window[1];
		ibs.livemask = 1 << 7;
		ibs.processed = 0;

		if (ibs.fname[0] == '-' && ibs.fname[1] == '\0')
		{
			if (!obs.fname)
				exiterror(EXIT_ERR_ARG, "no output "
					"filename specified\n");
			ibs.fd = STDIN_FILENO;
			ibs.fname = "stdin";
			keep_input = 1;
			output_perms = config_perms != -1
				? config_perms
				: (0600 & ~umask(0));
		} else
		{
			if ((ibs.fd = bz_open(ibs.fname, O_RDONLY,
				0, 0)) == -1)
			{
				if (!tolerant)
					exit(EXIT_ERR_OTHER);
				continue;
			}

			if (config_perms == -1)
			{
				struct stat statbuf;

				if (fstat(ibs.fd, &statbuf) != 0)
					exiterror(EXIT_ERR_OTHER,
						"stat: %s: %s\n",
						ibs.fname,
						strerror(errno));

				output_perms = statbuf.st_mode & 0666;
			} else
				output_perms = config_perms;
		} /* if */

		/* Output filename generation */
		input_bz = 1;
		if (obs.fname == NULL)
		{
			input_bz = output_generate_filename(fnamebuf,
				sizeof(fnamebuf), ibs.fname,
				compression_level);
			if (input_bz == -1)
			{
				bz_log(BZIP_LOG_WARNING, "%s: input "
					"filename too long\n", ibs.fname);
				if (!tolerant)
					exit(EXIT_ERR_ARG);
			} else if (!input_bz && keep_input)
			{
				bz_log(BZIP_LOG_WARNING, "%s: input "
					"filename doesn't "
					"end in \".bz\"\n", ibs.fname);
				if (!tolerant)
					exit(EXIT_ERR_ARG);
			}

			obs.fname = fnamebuf;
		}
		
		/* Open output file */
		obs.windowp = obs.window;
		obs.livemask = 1 << 7;
		obs.processed = 0;

		if (!output_dontopen)
		{
			if ((obs.fd = bz_open(obs.fname, output_flags,
				0000, paranoid)) == -1)
			{
				if (!tolerant)
					exit(EXIT_ERR_OTHER);
				close(ibs.fd);
				continue;
			}

#ifdef HAVE_FCHMOD
			if (fchmod(obs.fd, output_perms) != 0)
				exiterror(EXIT_ERR_OTHER,
					"fchmod: %s: %s\n", obs.fname,
					strerror(errno));
#else
			if (chmod(obs.fname, output_perms) != 0)
				exiterror(EXIT_ERR_OTHER,
					"chmod: %s: %s\n", obs.fname,
					strerror(errno));
#endif

			output_dontopen = output_dontclose;
		} /* if */

		/* (De)Compression */
		bz_log(BZIP_LOG_INFO, "converting %s -> %s "
			"with level %d\n", ibs.fname, obs.fname,
			compression_level);
		retval = compression_level
			? compress(&ibs, &obs, compression_level)
			: decompress(&ibs, &obs);

		/* Cleanup */
		close(ibs.fd);
		if (!output_dontclose && close(obs.fd) != 0)
		{
			bz_log(BZIP_LOG_ERROR, "close: %s: %s\n",
				obs.fname, strerror(errno));
			output_cleanup(&obs);
			exit(EXIT_ERR_OTHER);
		}

		if (retval != 0)
		{
			output_cleanup(&obs);
			if (!tolerant)
				exit(retval);
			continue;
		}

		if (!compression_level && !input_bz)
		{
			if (rename(obs.fname, ibs.fname) != 0)
				exiterror(EXIT_ERR_OTHER,
					"rename(%s, %s): %s\n",
					obs.fname, ibs.fname,
					strerror(errno));
		} else if (!keep_input)
			if (unlink(ibs.fname) != 0)
				exiterror(EXIT_ERR_OTHER,
					"unlink: %s: %s\n",
					ibs.fname,
					strerror(errno));

		if (!output_dontclose)
			obs.fname = NULL;
	} while ((ibs.fname = argv[optind++]) != NULL);

	if (output_dontclose && close(obs.fd) != 0)
	{
		bz_log(BZIP_LOG_ERROR, "close: %s: %s\n",
			obs.fname, strerror(errno));
		output_cleanup(&obs);
		exit(EXIT_ERR_OTHER);
	}

	exit(EXIT_HAPPILY);
	return 0;
} /* main */

/* End of bzip.c */
