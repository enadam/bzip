/* cmdline.h */
#ifndef CMDLINE_H
#define CMDLINE_H

/* Include files */
#include "config.h"

/* Standard definitions */
/* General options */
#define OPS_VERBOSE	'v'
#define OPS_QUIET	'q'
#define OPS_TOLERANT	't'
#define OPS_BLIND	'n'

/* Operation modes */
#define OPS_COMPRESS	'f'
#define OPS_DECOMPRESS	'd'

/* I/O options */
#define OPS_OUTPUT	'o'
#define OPS_STDOUT	'c'
#define OPS_KEEPINPUT	'k'
#define OPS_CAREFUL	'i'
#define OPS_PERMS	'm'

/* Compression level */
#define OPS_BLOCKSIZE	'b'

/* Common options */
#define OPS_VERSION	'V'
#define OPS_HELP	'h'

/* Private variables */
static char const cmdopts[] =
{
	/* General options */
	OPS_VERBOSE,
	OPS_QUIET,
	OPS_TOLERANT,
	OPS_BLIND,

	/* Operation modes */
	OPS_COMPRESS,
	OPS_DECOMPRESS,

	/* I/O options */
	OPS_OUTPUT,	':',
	OPS_STDOUT,
	OPS_KEEPINPUT,
	OPS_CAREFUL,
	OPS_PERMS,	':',

	/* Compression level */
	OPS_BLOCKSIZE,	':',
	'1', '2', '3', '4', '5', '6', '7', '8', '9',

	/* Common options */
	OPS_VERSION,
	OPS_HELP
};

/* Brief help */
static char const msg_usage[] =
"Usage: %s [<options>] [<input-file>] ...\n"
"(de)compresses files using several nontrivial algorithm.\n"
"\n"
"Options:\n"
"General options:\n"
"  -v                   tell more about what's going on\n"
"  -q                   be less verbose\n"
"  -t                   treat most errors nonfatal\n"
"  -n                   follow symlinks when creates files\n"
"\n"
"Operation modes:\n"
"  -f                   force compression\n"
"  -d                   force decompression\n"
"\n"
"I/O options:\n"
"  -o <file-name>       specify output file name\n"
"  -c                   write output to stdout\n"
"  -k                   don't delete input files\n"
"  -i                   don't overwrite files\n"
"  -m <perm>            specify output file permissions\n"
"\n"
"Compression level:\n"
"  -1 .. -9\n"
"  -b <level>           specify compression level\n"
"\n"
"Common options:\n"
"  -V                   display version information\n"
"  -h                   display this text\n"
"\n"
"Report bugs to <borso@enjoy-unix.org>.\n";

#endif /* ! CMDLINE_H */
