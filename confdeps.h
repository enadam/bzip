/* confdeps.h */
#ifndef CONFDEPS_H
#define CONFDEPS_H

/* Include files */
#include <limits.h>

/* Macros */
#ifndef HAVE_FATTRIBUTES
# define __attribute__(attrs)	/* */
#endif

/* Standard definitions */
#if !defined(PATH_MAX)
# if defined(_POSIX_PATH_MAX)
#  define PATH_MAX		_POSIX_PATH_MAX
# elif defined(MAXPATHLEN)
#  define PATH_MAX		MAXPATHLEN
# else
#  define PATH_MAX		255
# endif
#endif

#if !defined(HAVE_GETOPT_H) && !defined(HAVE_VAR_OPTARG)
extern char *optarg;
extern int optind, opterr, optopt;
#endif

#undef NDEBUG
#ifndef CONFIG_DEBUG
# define NDEBUG 1
#endif

#endif /* ! CONFDEPS_H */
