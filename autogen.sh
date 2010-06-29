#! /bin/sh

export LIBTOOL=libtool
export LIBTOOLIZE=libtoolize
if [ -x /usr/bin/glibtoolize ]; then
  export LIBTOOL=glibtool
  export LIBTOOLIZE=glibtoolize
fi

$LIBTOOLIZE -f -i --recursive && \
aclocal -I m4 && \
autoheader && \
automake --gnu --add-missing --include-deps && \
autoconf -I m4

(cd src/levent2 && \
$LIBTOOLIZE -f -i --subproject && \
aclocal -I m4 && \
autoheader && \
automake --gnu --add-missing --include-deps && \
autoconf)
