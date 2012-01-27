/* compress.h */
#ifndef COMPRESS_H
#define COMPRESS_H

/* Include files */
#include "config.h"

#include <endian.h>

#include "bzip.h"
#include "models.h"

/* Macros */
/* Move-to-front encoding/decoding */
/*
 * For good performance, fullGt() allows pointers to
 * get partially denormalized.  As a consequence, we
 * have to copy some small quantity of data from the
 * beginning of a block to the end of it so things
 * still work right.  These constants control that.
 *
 * The normalizers below are quick but only work when
 * p exceeds the block by less than words_end, since
 * they renormalize merely by adding or subtracting
 * words_end.
 */
#define NORMALIZELO(p)		((int)(p) >= 0 ? (p) : words_end + (int)(p))
#define NORMALIZEHI(p)		((p) < words_end ? (p) : (p) - words_end)

#if BYTE_ORDER == LITTLE_ENDIAN
# define GETALL(a)		words[a].u

# define GETFIRST16(a)		words[a].s[1]
# define GETREST16(a)		words[a].s[0]

# define GETFIRST(a)		words[a].c[3]
# define GETSECOND(a)		words[a].c[2]
# define GETTHIRD(a)		words[a].c[1]
# define GETFOURTH(a)		words[a].c[0]
#elif BYTE_ORDER == BIG_ENDIAN
# define GETALL(a)		words[a].u

# define GETFIRST16(a)		words[a].s[0]
# define GETREST16(a)		words[a].s[1]

# define GETFIRST(a)		words[a].c[0]
# define GETSECOND(a)		words[a].c[1]
# define GETTHIRD(a)		words[a].c[2]
# define GETFOURTH(a)		words[a].c[3]
#else
		*Whats up there*
#endif

#define SETALL(a, w)		GETALL(a) = (w)

#define SETFIRST16(a, w)	GETFIRST16(a) = (w)
#define SETREST16(a, w)		GETREST16(a) = (w)

#define SETFIRST(a, w)		GETFIRST(a) = (w)
#define SETSECOND(a, w)		GETSECOND(a) = (w)
#define SETTHIRD(a, w)		GETTHIRD(a) = (w)
#define SETFOURTH(a, w)		GETFOURTH(a) = (w)

/* Block-sorting machinery */
#ifdef CONFIG_DEBUG
# define RC(x) \
	((wuL <= (x) && (x) <= wuR) \
		? (x) \
		: (panic("Range error: %d (%d, %d)", \
			(x), wuL, wuR), 1))
#else /* ! CONFIG_DEBUG */
# define RC(x)		(x)
#endif

#define SWAP(za, zb) \
do \
{ \
	unsigned zt; \
	\
	zt = zptr[za]; \
	zptr[RC(za)] = zptr[RC(zb)]; \
	zptr[zb] = zt; \
} while (0)

#endif /* ! COMPRESS_H */
