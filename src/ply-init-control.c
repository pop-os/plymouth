/* ply-init-control.c - initcontrol abstraction
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
#include "ply-init-control.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>

#include <initreq.h>

#include "ply-utils.h"

#ifndef PLY_INIT_CONTROL_DEVICE_NAME
#define PLY_INIT_CONTROL_DEVICE_NAME "/dev/initctl"
#endif

struct _ply_init_control
{
  int fd;
};

ply_init_control_t *
ply_init_control_new (void)
{
  ply_init_control_t *control;

  control = calloc (1, sizeof (ply_init_control_t));
  control->fd = -1;
  return control;
}

void
ply_init_control_free (ply_init_control_t *control)
{
  assert (control != NULL);

  if (ply_init_control_is_open (control))
    ply_init_control_close (control);
}

bool
ply_init_control_open (ply_init_control_t *control)
{
  assert (control != NULL);
  assert (!ply_init_control_is_open (control));

  control->fd = open (PLY_INIT_CONTROL_DEVICE_NAME, O_WRONLY);

  if (control->fd < 0)
    return false;

  return true;
}

bool
ply_init_control_is_open (ply_init_control_t *control)
{
  assert (control != NULL);
  return control->fd >= 0;
}

void
ply_init_control_close (ply_init_control_t *control)
{
  assert (control != NULL);
  assert (ply_init_control_is_open (control));

  close (control->fd);
  control->fd = -1;
}

bool
ply_init_control_redirect_messages (ply_init_control_t *control)
{
  struct init_request request;

  assert (control != NULL);
  assert (ply_init_control_is_open (control));

  memset (&request, 0, sizeof (struct init_request));
  request.magic = INIT_MAGIC;
  request.cmd = INIT_CMD_CHANGECONS;

  /* XXX: this is so gross, initreq.h needs to be updated to handle
   * this command better
   */
  strncpy (request.i.bsd.reserved, "/dev/null",
           sizeof (request.i.bsd.reserved));

  if (!ply_write (control->fd, &request, sizeof (request)))
    return false;

  return true;
}

#ifdef PLY_INIT_CONTROL_ENABLE_TEST

#include <math.h>
#include <stdio.h>
#include <sys/time.h>

int
main (int    argc,
      char **argv)
{
  ply_init_control_t *control;
  int exit_code;

  exit_code = 0;

  control = ply_init_control_new ();

  if (!ply_init_control_open (control))
    {
      exit_code = errno;
      perror ("could not open init control");
      return exit_code;
    }

  if (!ply_init_control_redirect_messages (control))
    {
      exit_code = errno;
      perror ("could not redirect init messages");
      return exit_code;
    }

  ply_init_control_close (control);
  ply_init_control_free (control);

  return exit_code;
}

#endif /* PLY_INIT_CONTROL_ENABLE_TEST */

/* vim: set ts=4 sw=4 expandtab autoindent cindent cino={.5s,(0: */
