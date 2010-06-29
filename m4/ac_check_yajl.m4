# ===========================================================================
#
# SYNOPSIS
#
#   AC_CHECK_YAJL([action-if-found], [action-if-not-found])
#
# DESCRIPTION
#
#   Tests for the YAJL library.
#
#   This macro calls:
#
#     AC_SUBST(YAJL_CFLAGS) / AC_SUBST(YAJL_LIBS)
#
# LAST MODIFICATION
#
#   2010-06-29
#
# LICENSE
#
#   Copyright (c) 2010 Cristian Greco <cristian@regolo.cc>
#
#   Copying and distribution of this file, with or without modification, are
#   permitted in any medium without royalty provided the copyright notice
#   and this notice are preserved.

AC_DEFUN([AC_CHECK_YAJL], [
  ac_yajl_found="no"

  CPPFLAGS_SAVED="$CPPFLAGS"
  LDFLAGS_SAVED="$LDFLAGS"
  CFLAGS_SAVED="$CFLAGS"
  LIBS_SAVED="$LIBS"

  AC_CHECK_HEADER([yajl/yajl_parse.h], [
    AC_CHECK_LIB([yajl], [yajl_parse], [
      YAJL_CFLAGS="-I/usr/include/yajl"
      YAJL_LIBS="-lyajl"
      ac_yajl_found="yes"
     ], [
      AC_MSG_WARN([yajl library not found])
    ])
   ], [
    for ac_yajl_path in /usr /usr/local /opt /opt/local; do
      AC_MSG_CHECKING([for yajl_parse.h in $ac_yajl_path])
      if test -d "$ac_yajl_path/include/yajl/" -a -r "$ac_yajl_path/include/yajl/yajl_parse.h"; then
        AC_MSG_RESULT([yes])
        YAJL_CFLAGS="-I$ac_yajl_path/include/yajl"
        YAJL_LIBS="-lyajl"
        break;
      else
        AC_MSG_RESULT([no])
      fi
    done

    CFLAGS="YAJL_CFLAGS $CFLAGS"
    export CFLAGS
    LIBS="$YAJL_LIBS $LIBS"
    export LIBS

    AC_MSG_CHECKING([for yajl_gen_alloc in -lyajl])
    AC_LINK_IFELSE([
      AC_LANG_PROGRAM([[ #include <yajl/yajl_gen.h> ]], [[ yajl_gen_config conf = { 1, "  " };  yajl_gen g = yajl_gen_alloc(&conf, (void *)0); ]])
     ], [
      AC_MSG_RESULT([yes])
      ac_yajl_found="yes"
     ], [
      AC_MSG_RESULT([no])
    ])
  ])

  CPPFLAGS="$CPPFLAGS_SAVED"
  LDFLAGS="$LDFLAGS_SAVED"
  CFLAGS="$CFLAGS_SAVED"
  LIBS="$LIBS_SAVED"

  AS_IF([ test "x$ac_yajl_found" != xno ], [$1], [$2])

AC_SUBST([YAJL_CFLAGS])
AC_SUBST([YAJL_LIBS])
])
