#!/bin/bash

[ -z "$INITRD" ] && INITRD="/boot/initrd-$(/bin/uname -r).img"

if [ -z "$NEW_INITRD" ]; then
  NEW_INITRD="$(/bin/dirname $INITRD)/$(/bin/basename $INITRD .img)-plymouth.img"
fi

TMPDIR="$(mktemp -d $PWD/initrd.XXXXXXXXXX)"

(
    cd $TMPDIR
    zcat $INITRD | cpio -q -Hnewc -i --make-directories
    sed -i -e 's@^#!\(.*\)@#!/bin/plymouth \1@' init 
    rm -f $NEW_INITRD
    find | cpio -q -Hnewc -o | gzip -9 > $NEW_INITRD
    [ $? -eq 0 ] && echo "Wrote $NEW_INITRD"
)

rm -rf "$TMPDIR"
