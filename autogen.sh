#!/bin/sh

(cd $(dirname $0);
 autoreconf --install --symlink &&
 ./configure --enable-maintainer-mode $@)
