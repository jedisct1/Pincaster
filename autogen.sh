#! /bin/sh

export LIBTOOL=glibtool
export LIBTOOLIZE=glibtoolize

$LIBTOOLIZE -f -i --recursive && \
aclocal && \
autoheader && \
automake --gnu --add-missing --include-deps && \
autoconf

(cd src/levent2 && \
$LIBTOOLIZE -f -i --subproject && \
aclocal -I m4 && \
autoheader && \
automake --gnu --add-missing --include-deps && \
autoconf)
