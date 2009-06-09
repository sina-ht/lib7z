#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT([7z],[20050725],[sian@big.or.jp])
AC_CONFIG_SRCDIR([src/7z.c])
AC_CONFIG_HEADER([config.h])
AM_INIT_AUTOMAKE

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AC_PROG_LIBTOOL

# Checks for libraries.
# FIXME: Replace `main' with a function in `-lm':
AC_CHECK_LIB([m], [main])

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([fcntl.h stddef.h stdlib.h string.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_CONST
AC_C_INLINE
AC_TYPE_OFF_T
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_MEMCMP
AC_FUNC_REALLOC
AC_CHECK_FUNCS([memmove memset strcasecmp strdup])

AC_ARG_ENABLE(debug,
	[  --enable-debug          enable debugging code ])
if test "${enable_debug-set}" = "set"; then
  enable_debug=no
elif test "$enable_debug" = "yes"; then
  AC_DEFINE(DEBUG,, [Define if you want to debug.])
fi

AC_ARG_WITH(enfle,
        [  --with-enfle=prefix     specify enfle installed prefix (e.g. /usr) ])
if test "${with_enfle-set}" = "set"; then
  use_enfle=no
else
  if test "$with_enfle" = "yes"; then
    Enfle_dir=""
    for dir in /usr /usr/local; do
      if test -f $dir/bin/enfle; then
        Enfle_dir=$dir
      fi
    done
  else
    Enfle_dir=$with_enfle
  fi
  if test ! -f $Enfle_dir/bin/enfle; then
    AC_MSG_WARN([No enfle binary found.])
  else
    use_enfle=yes
    AC_SUBST(Enfle_dir)
  fi
fi

AC_ARG_WITH(enfle-src,
	[  --with-enfle-src=dir    build enfle plugin (needs enfle source) ])
if test "${with_enfle_src-set}" = "set"; then
  build_enfle=no
elif test "$with_enfle_src" = "yes"; then
  AC_MSG_WARN([Please specify where enfle source is.])
  build_enfle=no
else
  Enfle_srcdir=$with_enfle_src
  if test -d $Enfle_srcdir; then
    AC_SUBST(Enfle_srcdir)
    build_enfle=yes
  else
    AC_MSG_WARN([Specified enfle source directory [$Enfle_srcdir] is not found.])
    build_enfle=no
  fi
fi
AM_CONDITIONAL(BUILD_ENFLE, test "$build_enfle" = "yes")

AC_CONFIG_FILES([Makefile
                 include/Makefile
                 lib/Makefile
                 src/Makefile
                 enfle/Makefile
		 ])
AC_OUTPUT
