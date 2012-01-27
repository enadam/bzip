dnl
dnl aclocal.m4
dnl

dnl
dnl List of macros defined here:
dnl  - LC_INCLUDE
dnl  - LC_ADDTO_LIST
dnl  - LC_ADD_FEATURE
dnl  - LC_DEFINE_INST_DIRS
dnl  - LC_SUBST_CCFLAGS
dnl  - LC_INFO_CONFIG
dnl  - LC_SUBST_FILE
dnl  - LC_OUTPUT
dnl  - LC_CANONICAL_HOST
dnl  - LC_PROG_C_MAKEDEP
dnl  - LC_PROG_C_TAGS
dnl  - LC_C_FATTRIBUTES
dnl  - LC_C_FATTRIBUTE
dnl  - LC_C_TYPEOF
dnl  - LC_HEADER_STDC
dnl  - LC_HEADER_GETOPT
dnl  - LC_CHECK_TYPE
dnl  - LC_ARG_ENABLE_DEBUG
dnl  - LC_REQUIRE_COMPONENT
dnl  - LC_REQUIRE_PRESENT
dnl  - LC_RESOLVE_DIR

dnl
dnl Includes an M4 macro from $ac_aux_dir in compile time.
dnl
AC_DEFUN([LC_INCLUDE],
[
	builtin([include], LC_CONFIG_AUX_DIR/$1)
]) dnl LC_INCLUDE

dnl
dnl Appends $2 to $1, which must be a name of a variable.
dnl
AC_DEFUN([LC_ADDTO_LIST],
[
	if test "x$$1" = "x";
	then
		$1="$2";
	else
		$1="$$1 $2";
	fi
]) dnl LC_ADDTO_LIST

dnl
dnl Extends the feature-list by $1.
dnl
AC_DEFUN([LC_ADD_FEATURE],
[
	LC_ADDTO_LIST(FEATURES, $1)
]) dnl LC_ADD_FEATURE

dnl
dnl Resolves install location variables to directory names
dnl and calls AC_DEFINE on them.
dnl
AC_DEFUN([LC_DEFINE_INST_DIRS],
[
	test "$prefix" != "NONE" || prefix="$ac_default_prefix";
	test "$exec_prefix" != "NONE" || exec_prefix="$prefix";

	eval prefix="\"$prefix\"";
	eval exec_prefix="\"$exec_prefix\"";
	eval libexecdir="\"$libexecdir\"";

	eval bindir="\"$bindir\"";
	eval sbindir="\"$sbindir\"";

	eval sysconfdir="\"$sysconfdir\"";
	eval datadir="\"$datadir\"";

	eval localstatedir="\"$localstatedir\"";
	eval sharedstatedir="\"$sharedstatedir\"";

	eval libdir="\"$libdir\"";
	eval includedir="\"$includedir\"";

	eval mandir="\"$mandir\"";
	eval infodir="\"$infodir\"";

	AC_DEFINE_UNQUOTED([INST_DIR_PREFIX], ["$prefix"])
	AC_DEFINE_UNQUOTED([INST_DIR_EXECPREFIX], ["$exec_prefix"])
	AC_DEFINE_UNQUOTED([INST_DIR_BIN], ["$bindir"])
	AC_DEFINE_UNQUOTED([INST_DIR_SBIN], ["$sbindir"])
	AC_DEFINE_UNQUOTED([INST_DIR_ETC], ["$sysconfdir"])
	AC_DEFINE_UNQUOTED([INST_DIR_SHARED], ["$datadir"])
	AC_DEFINE_UNQUOTED([INST_DIR_VAR], ["$localstatedir"])
	AC_DEFINE_UNQUOTED([INST_DIR_COM], ["$sharedstatedir"])
]) dnl LC_DEFINE_INST_DIRS

AC_DEFUN([LC_SUBST_CCFLAGS],
[
	$1_cppflags="$CPPFLAGS [$]$1_cppflags";
	$1_cflags="$CFLAGS [$]$1_cflags";
	$1_ldflags="$LDFLAGS [$]$1_ldflags";
	$1_libs="$LIBS [$]$1_libs";

	AC_SUBST($1_cppflags)
	AC_SUBST($1_cflags)
	AC_SUBST($1_ldflags)
	AC_SUBST($1_libs)

	AC_SUBST($1_objs)
	AC_SUBST($1_lobjs)
]) dnl LC_SUBST_CCFLAGS

dnl
dnl Prints out some of ./configure's final decisions.
dnl
AC_DEFUN([LC_INFO_CONFIG],
[
	if test "X$silent" != "Xyes";
	then
		echo;
		echo "Guessed variables:";
		echo "	CPPFLAGS: ${CPPFLAGS}";
		echo "	CFLAGS: ${CFLAGS}";
		echo "	LDFLAGS: ${LDFLAGS}";
		echo "	LIBS: ${LIBS}";
		echo "Optional features: ${FEATURES}";
		echo "Install prefix: ${prefix}";
	fi
]) dnl LC_INFO_CONFIG

dnl
dnl Replaces file $1 with $2 if they differ.
dnl
AC_DEFUN([LC_SUBST_FILE],
[
	orig_file=$1;
	new_file=$2;

	echo "creating $orig_file";

	if ! cmp -s "$orig_file" "$new_file";
	then
		test "X$no_create" != "Xyes" \
			&& mv -f "$new_file" "$orig_file";
	else
		echo "$orig_file is unchagned";
		rm -f "$new_file";
	fi
]) dnl LC_SUBST_FILE

dnl
dnl Sets have_configured=yes and calls AC_SUBS on it.
dnl Finally, it calls AC_OUTPUT with arg $1.
dnl
AC_DEFUN([LC_OUTPUT],
[
	have_configured="yes";
	AC_SUBST([have_configured])

	AC_OUTPUT([$1])
]) dnl LC_OUTPUT

dnl
dnl Creates a C header file ($1) containing boolean and string
dnl #define:s for host_{arch,cpi,vendor,os}.
dnl Calls LC_SUBST_FILE on $1.
dnl
AC_DEFUN([LC_CANONICAL_HOST],
[
	AC_REQUIRE([AC_CANONICAL_HOST])

	host_arch=`echo "$host_cpu" | sed \
		-e 's/i.86/i386/' \
		-e 's/sun4u/sparc64/' \
		-e 's/arm.*/arm/' \
		-e 's/sa110/arm/'`;

	AC_SUBST([host_arch])
	AC_DEFINE_UNQUOTED([HOST_ARCH], ["$host_arch"])

	AC_SUBST([host_cpu])
	AC_DEFINE_UNQUOTED([HOST_CPU], ["$host_cpu"])

	AC_SUBST([host_vendor])
	AC_DEFINE_UNQUOTED([HOST_VENDOR], ["$host_vendor"])

	AC_SUBST([host_os])
	AC_DEFINE_UNQUOTED([HOST_OS], ["$host_os"])

	sysconf_fname=$1;
	if test "X$sysconf_fname" != "X";
	then
		sysconf_define=`echo "$sysconf_fname" \
			| tr 'abcdefghijklmnopqrstuvwxyz./-' \
				'ABCDEFGHIJKLMNOPQRSTUVWXYZ___'`;

		sysconf_arch=`echo "$host_arch" \
			| tr './+-' '____'`;
		sysconf_cpu=`echo "$host_cpu" \
			| tr './+-' '____'`;
		sysconf_vendor=`echo "$host_vendor" \
			| tr './+-' '____'`;
		sysconf_os=`echo "$host_os" \
			| tr './+-' '____'`;

		sysconf_contents="\
/* $sysconf_fname */
#ifndef $sysconf_define
#define $sysconf_define

/* Standard definitions */
#define HOST_ARCH_${sysconf_arch}
#define HOST_CPU_${sysconf_cpu}
#define HOST_VENDOR_${sysconf_vendor}
#define HOST_OS_${sysconf_os}

#endif /* ! $sysconf_define */";

		sysconf_tmpfname="${sysconf_fname}.new";
		echo "$sysconf_contents" > "$sysconf_tmpfname";
		LC_SUBST_FILE(["$sysconf_fname"], ["$sysconf_tmpfname"])
	fi
]) dnl LC_CANONICAL_HOST

dnl
dnl Looks for a program which is able to generate Makefile
dnl dependencies for C programs, stores in C_MAKEDEP and calls
dnl AC_SUBST. If doesn't find one, sets C_MAKEDEP to point to
dnl a poor man's script, which should be in ac_aux_dir.
dnl
AC_DEFUN([LC_PROG_C_MAKEDEP],
[
	if test "X$CC" = "Xgcc";
	then
		C_MAKEDEP='gcc -MG -MM';
		AC_SUBST([C_MAKEDEP])
	fi

	if test "x$C_MAKEDEP" = "x";
	then
		AC_CHECK_PROG([C_MAKEDEP], [makedepend], [makedepend -f-])
	fi

	if test "x$C_MAKEDEP" = "x";
	then
		AC_REQUIRE([AC_PROG_CPP])
		C_MAKEDEP="${ac_aux_dir}/mkdep-sh $CPP";
		AC_SUBST([C_MAKEDEP])
	fi
]) dnl LC_PROG_C_MAKEDEP

dnl
dnl Looks for a ctags program and provides some reasonably useful
dnl default switches. If doesn't find any, C_TAGS will be ':'.
dnl
AC_DEFUN([LC_PROG_C_TAGS],
[
	AC_CHECK_PROGS([C_TAGS], [exuberant-ctags ctags-elvis ctags], [:])
	case "$C_TAGS" in
	ctags-elvis)
		C_TAGS="$C_TAGS -stiv";
		;;
	exuberant-ctags)
		;;
	*)
		C_TAGS="$C_TAGS -w";
		;;
	esac
]) dnl LC_PROG_C_TAGS

dnl
dnl Checks whether the C compiler supports function attributes.
dnl Calls AC_DEFINE on HAVE_FATTRIBUTES.
dnl
AC_DEFUN([LC_C_FATTRIBUTES],
[
	AC_CACHE_CHECK([for function attributes], [lc_cv_c_fattributes],
		[AC_TRY_COMPILE(
			[],
			[void func(void) __attribute__ (());],
			[lc_cv_c_fattributes="yes";],
			[lc_cv_c_fattributes="no";])])

	if test "X$lc_cv_c_fattributes" = "Xyes";
	then
		AC_DEFINE([HAVE_FATTRIBUTES])
	fi
]) dnl LC_C_FATTRIBUTES

dnl
dnl Checks whether a particular function attribute ($1) is supported
dnl by the C compiler.  If so, sets HAVE_FATTR_*.
dnl
AC_DEFUN([LC_C_FATTRIBUTE],
[
	AC_CACHE_CHECK([if compiler knows __attribute__ (($1))],
		[lc_cv_c_fattribute_$1],
		[AC_TRY_COMPILE(
			[],
			[void func(void) __attribute__ (($1));],
			[lc_cv_c_fattribute_$1="yes";],
			[lc_cv_c_fattribute_$1="no";])])

	if test "X$lc_cv_c_fattribute_$1" = "Xyes";
	then
		AC_DEFINE([HAVE_FATTR_$1])
	fi
]) dnl LC_C_FATTRIBUTE

dnl
dnl Checks whether the C compiler supports the keyword typeof.
dnl If not, it will define typeof(obj) to be void *.
dnl
AC_DEFUN([LC_C_TYPEOF],
[
	AC_CACHE_CHECK([for if the compiler accepts the typeof keyword],
		[lc_cv_c_typeof],
		[AC_TRY_COMPILE(
			[],
			[int var1; typeof(var1) var2;],
			[lc_cv_c_typeof="yes";],
			[lc_cv_c_typeof="no";])])

	if test "X$lc_cv_c_typeof" = "Xyes";
	then
		AC_DEFINE([HAVE_TYPEOF])
	fi
]) dnl LC_C_TYPEOF

dnl
dnl Aborts configuration if ANSI libc envirornment cannot be found.
dnl
AC_DEFUN([LC_HEADER_STDC],
[
	AC_REQUIRE([AC_HEADER_STDC])
	if test "X$ac_cv_header_stdc" != "Xyes";
	then
		AC_MSG_ERROR([Your C library appears to be not fully ANSI compilant, which is required to compile this package.])
	fi
]) dnl LC_HEADER_STDC

dnl
dnl If getopt.h can be found, defines HAVE_GETOPT_H.
dnl Else if getopt() global variables are declared in some common header
dnl files, defines HAVE_VAR_OPTARG.  If neither is defined,
dnl you must provide the neccesary declarations in acconfig.h
dnl
AC_DEFUN([LC_HEADER_GETOPT],
[
	AC_CHECK_HEADER([getopt.h], [AC_DEFINE([HAVE_GETOPT_H])])
	AC_TRY_COMPILE(
[
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
],
		[optarg = (char *)0;], [AC_DEFINE([HAVE_VAR_OPTARG])])
]) dnl LC_HEADER_GETOPT

dnl
dnl Mimics LC_CHECK_TYPE, except that it accepts a second argument
dnl for additional include files.
dnl
AC_DEFUN([LC_CHECK_TYPE],
[
	AC_REQUIRE([AC_HEADER_STDC])

	AC_CACHE_CHECK([for $1], [lc_cv_type_$1],
		[AC_TRY_COMPILE(
[
#include <sys/types.h>
#ifdef STDC_HEADERS
# include <stdlib.h>
# include <stddef.h>
#endif
$3
],
			[$1 variable;],
			[lc_cv_type_$1="yes";],
			[lc_cv_type_$1="no";])])

	if test "X$lc_cv_type_$1" = "Xno";
	then
		AC_DEFINE($1, $2)
	fi
]) dnl LC_CHECK_TYPE

dnl
dnl Adds ./configure options for debugging and #define's
dnl CONFIG_DEBUG upon --enable-debug.  This case also calls
dnl AC_CHECK_LIB for ElectricFence if the user didn't
dnl instruct otherwise.  The symbol NDEBUG defined to 1 if
dnl CONFIG_DEBUG is false (needed by assert()).
dnl
AC_DEFUN([LC_ARG_ENABLE_DEBUG],
[
	AC_ARG_ENABLE([debug],
[  --enable-debug          produce a binary suitable for debugging],
		[], [enable_debug="no";])
	if test "X$enable_debug" = "Xyes";
	then
		AC_DEFINE([CONFIG_DEBUG])
	fi
	AC_SUBST([enable_debug])

	AC_ARG_WITH([efence],
[  --without-efence        do not link with ElectricFence (only effective when debugging is enabled)],
		[], [with_efence="yes";])
	if test "X$enable_debug" = "Xyes" -a "X$with_efence" = "Xyes";
	then
		AC_CHECK_LIB([efence], [main])
	fi
]) dnl LC_ARG_ENABLE_DEBUG

dnl
dnl Aborts if test $1, which is expected to be a check for some
dnl component, fails.  The second argument is passed unquoted
dnl to the test as its first argument(s).
dnl
AC_DEFUN([LC_REQUIRE_COMPONENT],
[
	$1($2, [AC_MSG_ERROR([Required component is missing.])])
]) dnl LC_REQUIRE_COMPONENT

dnl
dnl If $1, which should be an environment variable, doesn't
dnl expand to "yes", aborts the configuration process.
dnl
AC_DEFUN([LC_REQUIRE_PRESENT],
[
	if test "X$1" != "Xyes";
	then
		AC_MSG_ERROR([Required component is missing.])
	fi
]) dnl LC_REQUIRE_PRESENT

dnl
dnl Makes $1 absolute; it should be an environment variable
dnl with no further references to other variables.
dnl
AC_DEFUN([LC_RESOLVE_DIR],
[
	lc_old_pwd=`pwd`;
	cd "[$]$1";
	lc_new_pwd=`pwd`;
	cd "$lc_old_pwd";
	$1="$lc_new_pwd";
]) dnl LC_RESOLVE_DIR

dnl End of aclocal.m4
