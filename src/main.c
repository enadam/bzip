/*
 * main.c
 */

/* Include files */
#include "config.h"

#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <limits.h>
#include <stdarg.h>
#include <setjmp.h>
#include <errno.h>
#include <assert.h>

#include <string.h>
#include <stdio.h>
#include <fcntl.h>

#include <sys/stat.h>

#include "main.h"
#ifdef CONFIG_FANCY_UI
# include "cmdline.h"
#endif
#include "bitstream.h"
#include "lc_common.h"

/* Standard definitions */
#define DFLT_COMPRESSION_LEVEL		9

/* Function prototypes */
static void die(int exitcode, char const *fmt, ...)
	__attribute__ ((format (printf, 2, 3)))
	__attribute__ ((noreturn));
static void unexpected_eof(struct bitstream_st const *bs)
	__attribute__ ((noreturn));

static unsigned lc_atou(char const *str, unsigned base);
static int issymlink(int fd, char const *fname);
static char const *makeup_output_fname(struct bitstream_st const *ibs);
static mode_t makeup_output_perms(struct bitstream_st const *bs);

static void bs_open_input(struct bitstream_st *bs, char const *fname);
static void bs_open_output(struct bitstream_st *obs, char const *fname);

static unsigned bs_read(struct bitstream_st const *bs,
	void *data, size_t size);
static void bs_write(struct bitstream_st *bs,
	void const *data, size_t size);

static void bs_crc_init(struct bitstream_st *bs);
static void bs_align(struct bitstream_st *bs);
static int bs_eof(struct bitstream_st *bs);
static void bs_rewind(struct bitstream_st *bs);

static void bs_close(struct bitstream_st *bs);
static void bs_close_input(struct bitstream_st *bs);
static void bs_close_output(struct bitstream_st *bs);

static void bzip(struct bitstream_st *ibs, struct bitstream_st *obs);
static void bunzip(struct bitstream_st *ibs, struct bitstream_st *obs);
static void parse_cmdline(int argc, char *argv[]);
#ifndef HAVE_BASENAME
static char const *basename(char const *fname);
#endif

/* Private variables */
static char const *bzip_prgname;
static jmp_buf exception_handler;

/* Global variable definifions */
struct main_runtime_st main_runtime;
struct bitstream_st input_bs, output_bs;

/* Program code */
void logf(char const *fmt, ...)
{
	va_list printf_args;

	va_start(printf_args, fmt);
	fprintf(stderr, "%s: ", bzip_prgname);
	vfprintf(stderr, fmt, printf_args);
	va_end(printf_args);
	fputc('\n', stderr);
} /* logf */

void panic(char const *fmt, ...)
{
	va_list printf_args;

	fprintf(stderr, "%s: internal error: ", bzip_prgname);
	va_start(printf_args, fmt);
	vfprintf(stderr, fmt, printf_args);
	va_end(printf_args);
	fputc('\n', stderr);

	exit(EXIT_ERR_INSIDE);
} /* panic */

void throw_exception(int errorcode)
{
	longjmp(exception_handler, errorcode);
} /* throw_exception */

void lc_recallocp(void *ptrp, size_t newsize)
{
	void *newptr;

	if (!(newptr = realloc(*(void **)ptrp, newsize)))
	{
		logf("realloc(%u): %s", newsize, strerror(errno));
		throw_exception(EXIT_ERR_OTHER);
	}

	memset(newptr, 0, newsize);
	*(void **)ptrp = newptr;
} /* lc_reallocp */

unsigned bs_fill_byte(struct bitstream_st *bs, int eofok)
{
	unsigned n;

	assert(INRANGE(bs->byte_end,
		bs->byte_window, AFTER_OF(bs->byte_window)));
	assert(INRANGE(bs->byte_p, bs->byte_window, bs->byte_end));

	n = bs_read(bs, bs->byte_window, sizeof(bs->byte_window));
	bs->eof = n != sizeof(bs->byte_window);

	bs->byte_p = bs->byte_window;
	bs->byte_end = &bs->byte_window[n];

	if (!n && !eofok)
		unexpected_eof(bs);
	return n;
} /* bs_fill_byte */

void bs_flush_byte(struct bitstream_st *bs)
{
	u_int8_t *endp;

	assert(INRANGE(bs->byte_end,
		bs->byte_window, AFTER_OF(bs->byte_window)));
	assert(INRANGE(bs->byte_p, bs->byte_window, bs->byte_end));

	endp = bs->byte_p;
	bs->byte_p = bs->byte_window;
	if (bs->blocked)
		return;

	bs_write(bs, bs->byte_window, endp - bs->byte_window);
} /* bs_flush_byte */

unsigned bs_fill_bit(struct bitstream_st *bs, int eofok)
{
#ifdef CONFIG_DECOMPRESS
	unsigned n;
	u_int8_t *wp;

	assert(INRANGE(bs->bit_end,
		bs->bit_window, AFTER_OF(bs->bit_window)));
	assert(INRANGE(bs->bit_p, bs->bit_window, bs->bit_end));

	n = bs->byte_end > bs->byte_p
		? bs->byte_end - bs->byte_p
		: bs_fill_byte(bs, eofok);
	n *= BITS_OF(u_int8_t);

	bs->bit_p = bs->bit_window;
	bs->bit_end = n > MEMBS_OF(bs->bit_window)
		? AFTER_OF(bs->bit_window)
		: &bs->bit_window[n];

	wp = bs->bit_window;
	while (wp < bs->bit_end)
	{
		u_int8_t c;

		c = *bs->byte_p++;
		*wp++ = (c & (1 << 7)) != 0;
		*wp++ = (c & (1 << 6)) != 0;
		*wp++ = (c & (1 << 5)) != 0;
		*wp++ = (c & (1 << 4)) != 0;
		*wp++ = (c & (1 << 3)) != 0;
		*wp++ = (c & (1 << 2)) != 0;
		*wp++ = (c & (1 << 1)) != 0;
		*wp++ = (c & (1 << 0)) != 0;
	} /* while */
	assert(bs->byte_p <= bs->byte_end);

	return n;
#endif /* CONFIG_DECOMPRESS */
} /* bs_fill_bit */

void bs_flush_bit(struct bitstream_st *bs)
{
#ifdef CONFIG_COMPRESS
	u_int8_t *endp, *wp;

	assert(INRANGE(bs->bit_end,
		bs->bit_window, AFTER_OF(bs->bit_window)));
	assert(INRANGE(bs->bit_p, bs->bit_window, bs->bit_end));
	assert((bs->bit_p - bs->bit_window) % 8 == 0);

	endp = bs->bit_p;
	bs->bit_p = bs->bit_window;
	if (bs->blocked)
		return;

	if ((bs->byte_end - bs->byte_p) * BITS_OF(u_int8_t)
			< endp - bs->bit_window)
		bs_flush_byte(bs);

	wp = bs->bit_window;
	while (wp < endp)
	{
		u_int8_t c;

		assert((*wp & ~1) == 0);

		c = 0;
		c |= *wp++;
		c <<= 1;
		c |= *wp++;
		c <<= 1;
		c |= *wp++;
		c <<= 1;
		c |= *wp++;
		c <<= 1;
		c |= *wp++;
		c <<= 1;
		c |= *wp++;
		c <<= 1;
		c |= *wp++;
		c <<= 1;
		c |= *wp++;

		*bs->byte_p++ = c;
	} /* while */
	assert(bs->byte_p <= bs->byte_end);
#endif /* CONFIG_COMPRESS */
} /* bs_flus_bit */

/* Private functions */
void die(int exitcode, char const *fmt, ...)
{
	va_list printf_args;

	fprintf(stderr, "%s: ", bzip_prgname);
	va_start(printf_args, fmt);
	vfprintf(stderr, fmt, printf_args);
	va_end(printf_args);
	fputc('\n', stderr);

	exit(exitcode);
} /* die */

void unexpected_eof(struct bitstream_st const *bs)
{
	logf("%s: unexpeced EOF", bs->fname);
	throw_exception(EXIT_ERR_INPUT);
} /* unexpected_eof */

unsigned lc_atou(char const *str, unsigned base)
{
	unsigned result;

#ifdef CONFIG_FANCY_UI
	assert(INRANGE(base, 2, 10));

	for (result = 0; *str; result *= base, result += *str++ - '0')
		if (!INRANGE(*str, '0', '9'))
			die(EXIT_ERR_USER, "%s: unsigned integer "
				"expected", str);
#endif

	return result;
} /* lc_atou */

int issymlink(int fd, char const *fname)
{
#if defined(HAVE_LSTAT) && defined(CONFIG_FANCY_UI)
	struct stat lsb, rsb;

	if (fstat(fd, &rsb) < 0)
	{
		logf("fstat: %s: %s", fname, strerror(errno));
		return 0;
	} else if (lstat(fname, &lsb) < 0)
	{
		logf("lstat: %s: %s", fname, strerror(errno));
		return 0;
	}

	return rsb.st_dev != lsb.st_dev || rsb.st_ino != lsb.st_ino;
#else /* ! HAVE_LSTAT || ! CONFIG_FANCY_UI */
	return 0;
#endif
} /* issymlink */

char const *makeup_output_fname(struct bitstream_st const *ibs)
{
#ifdef CONFIG_FANCY_UI
	static char generated_fname[PATH_MAX];

	unsigned lfname, sext;
	char const *fname, *ext;

	if (ibs->stdfd)
		return "-";

	fname = ibs->fname;
	lfname = strlen(fname);
	if (IS_COMPRESS())
	{
		ext = ".bz";
		sext = sizeof(".bz");
	} else if (lfname > 3 && !memcmp(&fname[lfname - 3], ".bz", 3))
	{
		lfname -= 3;
		ext = "";
		sext = sizeof("");
	} else
	{
		ext = ".bunz";
		sext = sizeof(".bunz");
	}

	if (lfname + sext > sizeof(generated_fname))
		lfname = sizeof(generated_fname) - sext;
	memcpy(generated_fname, fname, lfname);
	memcpy(&generated_fname[lfname], ext, sext);

	return generated_fname;

#else /* ! CONFIG_FANCY_UI */
	return "-";
#endif
} /* makeup_output_fname */

mode_t makeup_output_perms(struct bitstream_st const *bs)
{
#ifdef CONFIG_FANCY_UI
	struct stat sb;

	if (main_runtime.has_perms)
		return main_runtime.perms;
	else if (input_bs.stdfd)
		return 0666;
	else if (fstat(bs->fd, &sb) < 0)
	{
		logf("fstat: %s: %s", bs->fname, strerror(errno));
		throw_exception(EXIT_ERR_OTHER);
	} else
		return sb.st_mode & 0666;
#else /* ! CONFIG_FANCY_UI */
	return 0;
#endif
} /* makeup_output_perms */

void bs_open_input(struct bitstream_st *bs, char const *fname)
{
	bs->eof = 0;
	bs->byte_p = bs->byte_window;
	bs->byte_end = bs->byte_window;
	bs->bit_p = bs->bit_window;
	bs->bit_end = bs->bit_window;

	if (fname[0] == '-' && fname[1] == '\0')
	{
		bs->fd = STDIN_FILENO;
		bs->fname = "(stdin)";
		bs->stdfd = 1;
		return;
	}

#ifdef CONFIG_FANCY_UI
	if ((bs->fd = open(fname, O_RDONLY)) < 0)
	{
		logf("open: %s: %s", fname, strerror(errno));
		goto out;
	}

	bs->fname = fname;
	bs->stdfd = 0;
	return;

out:
	bs->fname = NULL;
	throw_exception(EXIT_ERR_OTHER);
#endif /* CONFIG_FANCY_UI */
} /* bs_open_input */

void bs_open_output(struct bitstream_st *bs, char const *fname)
{
	int flags;
	mode_t perms;

	bs->blocked = main_runtime.drop_output;
	bs->byte_p = bs->byte_window;
	bs->byte_end = AFTER_OF(bs->byte_window);
	bs->bit_p = bs->bit_window;
	bs->bit_end = AFTER_OF(bs->bit_window);

	if (!fname)
		fname = makeup_output_fname(&input_bs);
	if (fname[0] == '-' && fname[1] == '\0')
	{
		bs->fd = STDOUT_FILENO;
		bs->fname = "(stdout)";
		bs->stdfd = 1;
		return;
	}

#ifdef CONFIG_FANCY_UI
	flags = O_CREAT | O_WRONLY | O_APPEND;
	if (main_runtime.append)
		;
	else if (!main_runtime.overwrite)
		flags |= O_EXCL;
	else if (main_runtime.symfollow)
		flags |= O_TRUNC;

	perms = makeup_output_perms(&input_bs);

	if ((bs->fd = open(fname, flags, perms)) < 0)
	{
		logf("open: %s: %s", fname, strerror(errno));
		goto out0;
	}
	if (!main_runtime.symfollow && issymlink(bs->fd, fname))
	{
	
		logf("%s: is a symlink", fname);
		goto out1;
	}
	if (main_runtime.overwrite && !main_runtime.symfollow
		&& ftruncate(bs->fd, 0) < 0)
	{
		logf("ftruncate: %s: %s", fname, strerror(errno));
		goto out1;
	}

	bs->fname = fname;
	bs->stdfd = 0;
	return;

out1:
	close(bs->fd);
	bs->fd = -1;
out0:
	bs->fname = NULL;
	throw_exception(EXIT_ERR_OTHER);
#endif /* CONFIG_FANCY_UI */
} /* bs_create */

unsigned bs_read(struct bitstream_st const *bs, void *data, size_t size)
{
	int n;

	if ((n = read(bs->fd, data, size)) < 0)
	{
		logf("read: %s: %s", bs->fname, strerror(errno));
		throw_exception(EXIT_ERR_OTHER);
	}

	return n;
} /* bs_read */

void bs_write(struct bitstream_st *bs, void const *data, size_t size)
{
	if (write(bs->fd, data, size) < 0)
	{
		logf("write: %s: %s", bs->fname, strerror(errno));
		throw_exception(EXIT_ERR_OTHER);
	}
} /* bs_write */

void bs_crc_init(struct bitstream_st *bs)
{
	bs->crc = ~0;
} /* bs_crc_init */

void bs_align(struct bitstream_st *bs)
{
	unsigned d;

	d = (bs->bit_p - bs->bit_window) % 8;
	if (d == 0)
		return;
	for (d = 8 - d; d > 0; d--)
		*bs->bit_p++ = 0;
} /* bs_align */

int bs_eof(struct bitstream_st *bs)
{
	if (bs->byte_p < bs->byte_end || bs->bit_p < bs->bit_end)
		return 0;
	else if (bs->eof)
		return 1;
	else if (IS_COMPRESS())
		return !bs_fill_byte(bs, 1);
	else
		return !bs_fill_bit(bs, 1);
} /* bs_eof */

void bs_rewind(struct bitstream_st *bs)
{
	bs->byte_p = bs->byte_window;
	bs->bit_p = bs->bit_window;
} /* bs_rewind */

void bs_close(struct bitstream_st *bs)
{
	int failed;

	if (!bs->fname || bs->fd < 0)
		return;

	failed = 0;
#ifdef CONFIG_FANCY_UI
	if (!bs->stdfd && close(bs->fd) < 0)
	{
		logf("close: %s: %s", bs->fname, strerror(errno));
		failed = 1;
	}
#endif /* CONFIG_FANCY_UI */

	bs->fd = -1;
	bs->fname = NULL;
	if (failed)
		throw_exception(EXIT_ERR_OTHER);
} /* bs_close */

void bs_close_input(struct bitstream_st *bs)
{
#ifdef CONFIG_FANCY_UI
	if (!bs->stdfd && !main_runtime.keep_input
		&& unlink(bs->fname) < 0)
	{
		logf("unlink: %s: %s", bs->fname, strerror(errno));
		throw_exception(EXIT_ERR_OTHER);
	}
#endif /* CONFIG_FANCY_UI */

	bs_close(bs);
} /* bs_close_input */

void bs_close_output(struct bitstream_st *bs)
{
	if (!bs->fname || bs->fd < 0)
		return;

	if (IS_COMPRESS())
		bs_flush_bit(bs);
	bs_flush_byte(bs);

	bs_close(bs);
} /* bs_close_output */

void bzip(struct bitstream_st *ibs, struct bitstream_st *obs)
{
#ifdef CONFIG_COMPRESS
	bs_crc_init(ibs);
	compress();
	bs_align(obs);
#endif
} /* bzip */

void bunzip(struct bitstream_st *ibs, struct bitstream_st *obs)
{
#ifdef CONFIG_DECOMPRESS
# ifdef CONFIG_FANCY_UI
	if (main_runtime.decompress_frag)
	{
		unsigned i;
		int blocked;

		blocked = output_bs.blocked;
		output_bs.blocked = 1;
		for (i = main_runtime.decompress_frag; i > 1; i--)
		{
			bs_crc_init(obs);
			decompress();
			bs_align(ibs);
		}
		output_bs.blocked = blocked;
		bs_rewind(obs);
	}
# endif /* CONFIG_FANCY_UI */

	do
	{
		bs_crc_init(obs);
		decompress();
		bs_align(ibs);
		if (main_runtime.decompress_frag)
			break;
	} while (!bs_eof(ibs));
#endif /* CONFIG_DECOMPRESS */
} /* bunzip */

void parse_cmdline(int argc, char *argv[])
{
	static char const *empty_input[] = { "-", NULL };

	int optchar;

	main_runtime.compression_level = !strcmp(bzip_prgname, "bunzip")
		? 0 : DFLT_COMPRESSION_LEVEL;
	main_runtime.overwrite = 1;

#ifdef CONFIG_FANCY_UI
	while ((optchar = getopt(argc, argv, cmdopts)) != -1)
		switch (optchar)
		{
		/* Operation modes */
		case OPS_COMPRESS:
			if (!main_runtime.compression_level)
				main_runtime.compression_level
					= DFLT_COMPRESSION_LEVEL;
			break;

		case OPS_COMPRESS_LEVEL:
			main_runtime.compression_level
				= lc_atou(optarg, 10);
			if (main_runtime.compression_level + '0' > 255)
				die(EXIT_ERR_USER,
					"%u: compression level too high",
					main_runtime.compression_level);
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
			main_runtime.compression_level = optchar - '0';
			break;

		case OPS_DECOMPRESS:
			main_runtime.compression_level = 0;
			break;

		case OPS_DECOMPRESS_FRAG:
			main_runtime.compression_level = 0;
			main_runtime.decompress_frag
				= lc_atou(optarg, 10);
			break;

		/* I/O options */
		case OPS_OUTPUT:
			main_runtime.output = optarg;
			main_runtime.keep_input = 1;
			break;

		case OPS_DEVNULL:
			main_runtime.drop_output = 1;

		case OPS_STDOUT:
			main_runtime.output = "-";
			main_runtime.keep_input = 1;
			break;

		case OPS_PERMS:
			main_runtime.perms = lc_atou(optarg, 8);
			main_runtime.has_perms = 1;
			break;

		case OPS_TOLERANT:
			main_runtime.tolerant = 1;
			break;

		case OPS_DONT_TOLERANT:
			main_runtime.tolerant = 0;
			break;

		case OPS_KEEPINPUT:
			main_runtime.keep_input = 1;
			break;

		case OPS_DONT_KEEPINPUT:
			main_runtime.keep_input = 0;
			break;

		case OPS_OVERWRITE:
			main_runtime.overwrite = 1;
			main_runtime.append = 0;
			break;

		case OPS_DONT_OVERWRITE:
			main_runtime.overwrite = 0;
			break;

		case OPS_APPEND:
			main_runtime.append = 1;
			main_runtime.overwrite = 0;
			break;

		case OPS_DONT_APPEND:
			main_runtime.append = 0;
			break;

		case OPS_SYMFOLLOW:
			main_runtime.symfollow = 1;
			break;

		case OPS_DONT_SYMFOLLOW:
			main_runtime.symfollow = 0;
			break;

		/* Common options */
		case OPS_VERSION:
			printf("bzip %s\nConfigured features: %s\n",
				bzip_version, CONFIG_FEATURES);
			exit(EXIT_HAPPILY);
			break;

		case OPS_HELP:
			printf(msg_usage, bzip_prgname);
			exit(EXIT_HAPPILY);
			break;

		/* Unknown */
		default:
			exit(EXIT_ERR_USER);
			break;
		} /* switch in while */

	main_runtime.inputs = argv[optind] == NULL
		? empty_input : (char const **)&argv[optind];

#else /* ! CONFIG_FANCY_UI */
	main_runtime.inputs = empty_input;
#endif

#ifndef CONFIG_COMPRESS
	if (main_runtime.compression_level > 0)
		die(EXIT_ERR_USER, "this version will not "
			"compress anything.");
#endif
#ifndef CONFIG_DECOMPRESS
	if (!main_runtime.compression_level)
		dief(EXIT_ERR_USER, "this version will not "
			"decompress anything.");
#endif
} /* parse_cmdline */

#ifndef HAVE_BASENAME
char const *basename(char const *fname)
{
	char const *base;

	if ((base = strrchr(fname, '/')) != NULL)
		return base + 1;
	else
		return fname;
} /* basename */
#endif /* ! HAVE_BASENAME */

/* The main function */
int main(int argc, char *argv[])
{
	int errcode;
	volatile int last_error;
	char const *input, *const *inputs;

	/* Parsing command line */
	bzip_prgname = basename(argv[0]);
	parse_cmdline(argc, argv);

	/* The exception handler */
	last_error = 0;
	if ((errcode = setjmp(exception_handler)) != 0)
	{
		bs_close(&input_bs);
		bs_close(&output_bs);

		if (!main_runtime.tolerant)
			exit(errcode);
		last_error = errcode;
	}

	/* The main loop */
	for (inputs = main_runtime.inputs; (input = *inputs++) != NULL; )
	{
		bs_open_input(&input_bs, input);
		if (!main_runtime.output || !output_bs.fname)
			bs_open_output(&output_bs, main_runtime.output);

		if (IS_COMPRESS())
			bzip(&input_bs, &output_bs);
		else
			bunzip(&input_bs, &output_bs);

		if (!main_runtime.output || !*inputs)
			bs_close_output(&output_bs);
		bs_close_input(&input_bs);
	} /* while */

	exit(last_error);
} /* main */

/* End of main.c */
