/* plymouth-drm-escrow.c - hold on to drm fd at shutdown to stop flicker
 *
 * Copyright (C) 2018 Red Hat, Inc
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License,
 * or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this; see the file COPYING.  If not, write to the Free
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Written by: Ray Strode <rstrode@redhat.com>
 */
#include "config.h"
#include <stdbool.h>
#include <sysexits.h>
#include <unistd.h>

int
main (int    argc,
      char **argv)
{
        /* Make the first byte in argv be '@' so that we can survive systemd's killing
         * spree when going from initrd to /, and so we stay alive all the way until
         * the power is killed at shutdown.
         * http://www.freedesktop.org/wiki/Software/systemd/RootStorageDaemons
         */
        argv[0][0] = '@';

        /* We don't actually do anything.  The drm fd was passed to us on some fd,
         * and we just need to keep it open until by staying alive until we're killed
         */
        while (true) pause ();

        return EX_SOFTWARE;
}
/* vim: set ts=4 sw=4 expandtab autoindent cindent cino={.5s,(0: */
