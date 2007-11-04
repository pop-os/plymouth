/* ply-copy-dir-test.c - tests some of the copy utility functions
 *
 * Copyright (C) 2007 Red Hat, Inc.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Written by: Ray Strode <rstrode@redhat.com>
 */
#include "config.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "ply-utils.h"

bool
test_dir_copy (void)
{
  int dir_fd;

  if (!ply_create_directory ("test-dir-copy-source"))
    return false;

  system ("cp ../* test-dir-copy-source");

  if (!ply_create_directory ("test-dir-copy-dest"))
    return false;

  if (!ply_copy_directory ("test-dir-copy-source", "test-dir-copy-dest"))
    return false;

  if (!ply_create_detachable_directory ("/foo/test-dir-copy-scratch"))
    return false;

  if (!ply_copy_directory ("test-dir-copy-dest", "/foo/test-dir-copy-scratch"))
    return false;

  system ("ls /foo/test-dir-copy-scratch");

  dir_fd = ply_detach_directory ("/foo/test-dir-copy-scratch");

  if (fchdir (dir_fd) != 0)
    return false;

  system ("ls");

  return true;
}

int
main (int    argc, 
      char **argv)
{
  if (!test_dir_copy ())
    return 1;

  return 0;
}
/* vim: set ts=4 sw=4 expandtab autoindent cindent cino={.5s,(0: */
