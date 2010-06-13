AC_PREREQ([2.65])
AC_INIT([pincaster], [0.4], [bugs at puretpd dot org])
AC_CONFIG_SRCDIR([src/app.c])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([1.9 dist-bzip2])
AM_MAINTAINER_MODE

AC_SUBST(VERSION)
ISODATE=`date +%Y-%m-%d`
AC_SUBST(ISODATE)

# Checks for programs.
AC_PROG_CC_C99
AC_USE_SYSTEM_EXTENSIONS
CPPFLAGS="$CPPFLAGS -D_XPG4_2=1"
AC_PROG_RANLIB

if test -d /usr/local/include; then
  CPPFLAGS="$CPPFLAGS -I/usr/local/include"
fi

if test -d /usr/local/lib; then
  LDFLAGS="$LDFLAGS -L/usr/local/lib"
fi

CFLAGS="$CFLAGS -pthread"
LDFLAGS="$LDFLAGS -pthread"

# Checks for libraries.
AC_CHECK_LIB([m], [cosf],, AC_ERROR([libm not found]))
AC_CHECK_LIB([rt], [clock_gettime])

# Checks for header files.
AC_SYS_LARGEFILE
AC_HEADER_STDBOOL
AC_CHECK_HEADERS([inttypes.h limits.h stddef.h stdlib.h string.h pthreads.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_RESTRICT
AC_C_FLEXIBLE_ARRAY_MEMBER
AC_C_VARARRAYS

AC_CHECK_ALIGNOF(void *)

AC_TYPE_SIGNAL
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_OFF_T
AC_TYPE_MODE_T
AC_TYPE_INTPTR_T
AC_TYPE_UINTPTR_T
AC_TYPE_UINT32_T
AC_CHECK_TYPES([ptrdiff_t])

# Switches
AC_HEADER_ASSERT

AC_ARG_WITH(debug,
[AS_HELP_STRING(--with-debug,For maintainers only - please do not use)],
[ if test "x$withval" = "xyes" ; then
    CFLAGS="$CFLAGS -DQUEUE_MACRO_DEBUG=1 -DDEBUG=1 -g -Wall -W -Wcast-align -Wbad-function-cast -Wstrict-prototypes -Wwrite-strings -Wreturn-type "
  fi ])

# Checks for library functions.
AC_FUNC_ALLOCA
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_FUNC_STRTOD
AC_CHECK_FUNCS([memmove strcasecmp strchr strdup])
AC_CHECK_FUNCS([pthread_spin_lock OSSpinLockLock fdatasync])
AC_CHECK_FUNCS([ffs ffsl ffsll])
AC_CHECK_FUNCS([strncasecmp strtol])
AC_CHECK_HEADERS([fcntl.h])

AC_SUBST([MAINT])

LEVENTDIR="levent2"
AC_CONFIG_SUBDIRS([src/levent2])
CPPFLAGS="-I$LEVENTDIR -I$LEVENTDIR/include $CPPFLAGS"

AC_CONFIG_FILES([Makefile
                 src/Makefile
                 src/ext/Makefile
                 src/yajl/Makefile
                 src/yajl/api/Makefile
                ])

AC_OUTPUT

(cd src/levent2 && $am_make)
