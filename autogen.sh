#!/bin/sh

(cd $(dirname $0);
 autoreconf --install --symlink &&
 ./configure $@)
