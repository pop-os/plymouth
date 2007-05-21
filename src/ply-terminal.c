/* ply-terminal.c - psuedoterminal abstraction
 *
 * Copyright (C) 2006, 2007 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
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
 * Written by: Kristian Høgsberg <krh@redhat.com>
 *             Ray Strode <rstrode@redhat.com>
 */
#include "config.h"
#include "ply-terminal.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "ply-utils.h"

struct _ply_terminal
{
  char  *name;
  int fd;
};

ply_terminal_t *
ply_terminal_new (void)
{
  ply_terminal_t *terminal;

  terminal = calloc (1, sizeof (ply_terminal_t));
  terminal->fd = -1;

  return terminal;
}

void
ply_terminal_free (ply_terminal_t *terminal)
{
  assert (terminal != NULL);

  ply_terminal_destroy_device (terminal);
  free (terminal);
}

bool
ply_terminal_create_device (ply_terminal_t *terminal)
{
  int saved_errno;

  assert (terminal != NULL);
  assert (!ply_terminal_has_device (terminal));

#if 0
  terminal->fd = posix_openpt (O_RDWR | O_NOCTTY);
#endif
  terminal->fd = open ("/dev/ptmx", O_RDWR | O_NOCTTY);

  if (terminal->fd < 0)
    return false;

  if (grantpt (terminal->fd) < 0)
    {
      saved_errno = errno;
      ply_terminal_destroy_device (terminal);
      errno = saved_errno;
      return false;
    }

  if (unlockpt (terminal->fd) < 0)
    {
      saved_errno = errno;
      ply_terminal_destroy_device (terminal);
      errno = saved_errno;
      return false;
    }

  terminal->name = strdup (ptsname (terminal->fd));

  return true;
}

bool
ply_terminal_has_device (ply_terminal_t *terminal)
{
  assert (terminal != NULL);

  return terminal->fd >= 0;
}

void
ply_terminal_destroy_device (ply_terminal_t *terminal)
{
  assert (terminal != NULL);

  free (terminal->name);
  terminal->name = NULL;

  close (terminal->fd);
  terminal->fd = -1;
}

int 
ply_terminal_get_fd (ply_terminal_t *terminal)
{
  assert (terminal != NULL);

  return terminal->fd;
}

const char *
ply_terminal_get_device_name (ply_terminal_t *terminal)
{
  assert (terminal != NULL);
  assert (ply_terminal_has_device (terminal));

  assert (terminal->name != NULL);
  return terminal->name;
}

#ifdef PLY_TERMINAL_ENABLE_TEST

#include <stdio.h>

int
main (int    argc,
      char **argv)
{
  ply_terminal_t *terminal;
  const char *name;
  uint8_t byte;
  int exit_code;

  exit_code = 0;

  terminal = ply_terminal_new ();

  if (!ply_terminal_create_device (terminal))
    {
      exit_code = errno;
      perror ("could not open new terminal");
      return exit_code;
    }

  name = ply_terminal_get_device_name (terminal);
  printf ("terminal name is '%s'\n", name);

  while (read (ply_terminal_get_fd (terminal), 
               &byte, sizeof (byte)) == 1)
    printf ("%c", byte);

  ply_terminal_free (terminal);

  return exit_code;
}

#endif /* PLY_TERMINAL_ENABLE_TEST */
/* vim: set ts=4 sw=4 expandtab autoindent cindent cino={.5s,(0: */
