/* acconfig.h, config.h.in, config.h */
#ifndef CONFIG_H
#define CONFIG_H

#define _GNU_SOURCE

@TOP@

#undef HAVE_FATTRIBUTES

#undef u_int8_t
#undef u_int32_t

#undef CONFIG_FANCY_UI
#undef CONFIG_COMPRESS
#undef CONFIG_DECOMPRESS
#undef CONFIG_MULTITHREAD
#undef CONFIG_FEATURES

#undef CONFIG_DEBUG

@BOTTOM@

#include "confdeps.h"

#endif /* ! CONFIG_H */
