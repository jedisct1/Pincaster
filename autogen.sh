#! /bin/sh

glibtoolize --copy --install || libtoolize --copy --install
autoheader -I m4 && \
aclocal -I m4 && \
automake --add-missing --force-missing --include-deps && \
autoconf
