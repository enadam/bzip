/* main.h */
#ifndef MAIN_H
#define MAIN_H

/* Include files */
#include "config.h"

#include <unistd.h>
#include <sys/types.h>

/* Standard definitions */
#define NOINLINE(func)		((void (*)())func)
#define NOINLINE_T(type, func)	((type (*)())func)

/* Macros */
#if defined(CONFIG_COMPRESS) && defined(CONFIG_DECOMPRESS)
# define IS_COMPRESS()		(main_runtime.compression_level > 0)
#elif defined(CONFIG_COMPRESS)
# define IS_COMPRESS()		1
#elif defined(CONFIG_DECOMPRESS)
# define IS_COMPRESS()		0
#endif

#if 0
# define LOG_DEBUG(params)	logf params;
#else
# define LOG_DEBUG(params)	/* */
#endif

/* Type definitions */
struct main_runtime_st
{
	char const *output, *const *inputs;
	int drop_output, has_perms;
	mode_t perms;

	unsigned compression_level, compress_threads;
	unsigned decompress_frag;

	int tolerant, keep_input, symfollow, overwrite, append;
};

/* Function ptototypes */
extern void logf(char const *fmt, ...)
	__attribute__ ((format (printf, 1, 2)));
extern void panic(char const *fmt, ...)
	__attribute__ ((format (printf, 1, 2)))
	__attribute__ ((noreturn));
extern void throw_exception(int errorcode)
	__attribute__ ((noreturn));
extern void lc_recallocp(void *ptrp, size_t newsize);

extern int start_threads(int (*tcomp)[2], unsigned nthreads);
extern void stop_threads(void);

extern void compress(void);
extern void decompress(void);

/* Global variables */
extern struct main_runtime_st main_runtime;
extern char const bzip_version[];

#endif /* ! MAIN_H */
