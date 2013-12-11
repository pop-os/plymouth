#!/bin/sh

cd $(dirname $0)
autoreconf --install --symlink -Wno-portability
if test -z "$NOCONFIGURE"; then
   exec ./configure $@
fi
