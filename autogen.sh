#!/bin/sh

(cd $(dirname $0);
 autoreconf --install --symlink &&
 intltoolize --force &&
 autoreconf --install --symlink &&
 ./configure --enable-maintainer-mode $@)
