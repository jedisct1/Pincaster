if ENABLE_EMBEDDED_YAJL
noinst_LIBRARIES = libyajl.a
endif

libyajl_a_SOURCES = \
  yajl.c \
  yajl_alloc.c \
  yajl_alloc.h \
  yajl_buf.c \
  yajl_buf.h \
  yajl_bytestack.h \
  yajl_encode.c \
  yajl_encode.h \
  yajl_gen.c \
  yajl_lex.c \
  yajl_lex.h \
  yajl_parser.c \
  yajl_parser.h \
  api/yajl_common.h \
  api/yajl_gen.h \
  api/yajl_parse.h

SUBDIRS = api

EXTRA_DIST = COPYING
