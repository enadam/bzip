/* cmdline.h */
#ifndef CMDLINE_H
#define CMDLINE_H

/* Standard definitions */
/* Operation modes */
#define OPS_COMPRESS			'f'
#define OPS_COMPRESS_LEVEL		'b'
#define OPS_COMPRESS_THREADS		'p'

#define OPS_DECOMPRESS			'd'
#define OPS_DECOMPRESS_FRAG		'D'

/* I/O options */
#define OPS_OUTPUT			'o'
#define OPS_STDOUT			'c'
#define OPS_DEVNULL			'C'
#define OPS_PERMS			'm'

#define OPS_TOLERANT			't'
#define OPS_DONT_TOLERANT		'T'
#define OPS_KEEPINPUT			'k'
#define OPS_DONT_KEEPINPUT		'K'
#define OPS_OVERWRITE			'w'
#define OPS_DONT_OVERWRITE		'W'
#define OPS_APPEND			'a'
#define OPS_DONT_APPEND			'A'
#define OPS_SYMFOLLOW			's'
#define OPS_DONT_SYMFOLLOW		'S'

/* Common options */
#define OPS_VERSION			'V'
#define OPS_HELP			'h'

/* Private variables */
static char const cmdopts[] =
{
	/* Operation modes */
	OPS_COMPRESS,
	OPS_COMPRESS_LEVEL,	':',
	'1', '2', '3', '4', '5', '6', '7', '8', '9',

	OPS_DECOMPRESS,
	OPS_DECOMPRESS_FRAG,	':',

	/* I/O options */
	OPS_OUTPUT,		':',
	OPS_STDOUT,
	OPS_DEVNULL,
	OPS_PERMS,		':',

	OPS_TOLERANT,
	OPS_DONT_TOLERANT,
	OPS_KEEPINPUT,
	OPS_DONT_KEEPINPUT,
	OPS_OVERWRITE,
	OPS_DONT_OVERWRITE,
	OPS_APPEND,
	OPS_DONT_APPEND,
	OPS_SYMFOLLOW,
	OPS_DONT_SYMFOLLOW,

	/* Common options */
	OPS_VERSION,
	OPS_HELP
};

static char const msg_usage[] =
"Usage: %s [<options>] [<input-file>] ...\n"
"Compress and decompress files.\n"
"\n"
"Options:\n"
"Operation modes:\n"
#ifdef CONFIG_COMPRESS
"  -f                   compress input\n"
"  -1 .. -9\n"
"  -b <level>           specify compression level (default: 9)\n"
"\n"
#endif
#ifdef CONFIG_DECOMPRESS
"  -d                   decompress input\n"
"  -D                   specify which fragment to decompress (default: all)\n"
"\n"
#endif
"I/O options: (capital letters mean `do not')\n"
"  -o <file-name>       specify output file name (implies -k)\n"
"  -c                   write resoult to stdout (implies -k)\n"
"  -C                   do not write result to anywhere (implies -k)\n"
"  -m <perm>            specify output file permissions (default: 0666)\n"
"\n"
"  -t, -T               treat most errors nonfatal\n"
"  -k, -K               keep input files\n"
"  -w, -W               overwrite output (default)\n"
"  -a, -A               append to output\n"
"  -s, -S               follow symlinks\n"
"\n"
"Common options:\n"
"  -V                   display version information\n"
"  -h                   display this text\n"
"\n"
"Report bugs to <_tgz@enjoy-unix.org>.\n";

#endif /* ! CMDLINE_H */
