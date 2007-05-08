#!/bin/sh

(cd $(dirname $0);
 autoreconf --install --symlink;
 intltoolize --force;
 ./configure --enable-maintainer-mode $@)
