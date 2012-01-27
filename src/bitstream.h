/* bitstream.h */
#ifndef BITSTREAM_H
#define BITSTREAM_H

/* Include files */
#include "config.h"

#include <sys/types.h>
#include <assert.h>

#include "main.h"
#include "crc.h"
#include "lc_common.h"

/* Standard definitions */
#define BS_WINDOW_SIZE			(80 * 1024)
#define BS_BIT_WINDOW			16

/* Type definitions */
/*
 * It is a mule thing.
 * Variables declared as `struct bitstream_st' can be used in four
 * different fashion:
 * -- input bitstream (decompress)
 * -- output bitstream (compress)
 * -- input bytestream (compress)
 * -- output bytestream (decompress)
 *
 * These roles do overlap, but unfortunely in non-OO environment it is
 * very hard (and cumbersome) to make use of this.  Thus they are merged
 * into one structure, which doesn't increase overhead very much
 * (in terms of memory usage) but helps keeping things simple.
 *
 * And BTW, for now #define byte octet till the end of the sources.
 */
struct bitstream_st
{
	/* Used in every roles */
	int fd, stdfd;
	char const *fname;
	u_int8_t *byte_p, *byte_end;
	u_int8_t byte_window[BS_WINDOW_SIZE];

	/* Input */
	int eof;

	/* Output */
	int blocked;

	/* Bytestream */
	u_int32_t crc;

	/* Bitstream */
	u_int8_t *bit_p, *bit_end;
	u_int8_t bit_window[BITS_OF(u_int8_t) * BS_BIT_WINDOW];
};

/* Function prototypes */
extern unsigned bs_fill_byte(struct bitstream_st *bs, int eofok);
extern void bs_flush_byte(struct bitstream_st *bs);
extern unsigned bs_fill_bit(struct bitstream_st *bs, int eofok);
extern void bs_flush_bit(struct bitstream_st *bs);

/* Global variables */
extern struct bitstream_st input_bs, output_bs;

#endif /* ! BITSTREAM_H */
