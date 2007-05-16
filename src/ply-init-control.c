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

#include "ply-terminal.h"
#include "ply-utils.h"

#ifndef PLY_INIT_CONTROL_DEVICE_NAME
#define PLY_INIT_CONTROL_DEVICE_NAME "/dev/initctl"
#endif

struct _ply_init_control
{
  int fd;
  ply_terminal_t *terminal;

  uint32_t is_trapping_messages : 1;
};

static bool ply_init_control_open_terminal (ply_init_control_t *control);
static void ply_init_control_close_terminal (ply_init_control_t *control);
static bool ply_init_control_is_trapping_messages (ply_init_control_t *control);

static bool
ply_init_control_open_terminal (ply_init_control_t *control)
{
  assert (!ply_terminal_is_open (control->terminal));

  if (!ply_terminal_open (control->terminal))
    return false;

  return true;
}

static void
ply_init_control_close_terminal (ply_init_control_t *control)
{
  assert (ply_terminal_is_open (control->terminal));
  ply_terminal_close (control->terminal);
}

static bool
ply_init_control_is_trapping_messages (ply_init_control_t *control)
{
  return control->is_trapping_messages;
}

ply_init_control_t *
ply_init_control_new (void)
{
  ply_init_control_t *control;

  control = calloc (1, sizeof (ply_init_control_t));
  control->fd = -1;
  control->terminal = ply_terminal_new ();
  return control;
}

void
ply_init_control_free (ply_init_control_t *control)
{
  assert (control != NULL);

  if (ply_init_control_is_open (control))
    ply_init_control_close (control);

  ply_init_control_close_terminal (control);
  ply_terminal_free (control->terminal);

  free (control);
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
ply_init_control_change_console (ply_init_control_t *control,
                                 const char         *console_name)
{
  struct init_request request;

  assert (control != NULL);
  assert (ply_init_control_is_open (control));

  memset (&request, 0, sizeof (struct init_request));
  request.magic = INIT_MAGIC;
  request.cmd = INIT_CMD_CHANGECONS;

  /* XXX: this is so gross, initreq.h really needs to be updated 
   * to handle this command better
   */
  if (console_name != NULL)
    strncpy (request.i.bsd.reserved, console_name,
             sizeof (request.i.bsd.reserved));

  if (!ply_write (control->fd, &request, sizeof (request)))
    return false;

  return true;
}

bool
ply_init_control_trap_messages (ply_init_control_t *control)
{
  const char *terminal_name;

  assert (control != NULL);
  assert (ply_init_control_is_open (control));
  assert (!ply_init_control_is_trapping_messages (control));

  ply_init_control_open_terminal (control);
  terminal_name = ply_terminal_get_name (control->terminal);

  control->is_trapping_messages = true;
  return ply_init_control_change_console (control, terminal_name);
}

void
ply_init_control_untrap_messages (ply_init_control_t *control)
{
  assert (control != NULL);
  assert (ply_init_control_is_open (control));
  assert (ply_init_control_is_trapping_messages (control));

  ply_init_control_change_console (control, NULL);
  control->is_trapping_messages = false;
}

int
ply_init_control_get_messages_fd (ply_init_control_t *control)
{
  assert (control != NULL);
  assert (ply_init_control_is_open (control));
  assert (ply_init_control_is_trapping_messages (control));
  return ply_terminal_get_fd (control->terminal);
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
  int fd;
  char buf[64] = "";

  exit_code = 0;

  control = ply_init_control_new ();

  if (!ply_init_control_open (control))
    {
      exit_code = errno;
      perror ("could not open init control");
      return exit_code;
    }

  if (!ply_init_control_trap_messages (control))
    {
      exit_code = errno;
      perror ("could not trap init messages");
      return exit_code;
    }

  fd = ply_init_control_get_messages_fd (control);

  if (read (fd, buf, sizeof (buf) - 1) < 0)
    perror ("couldn't read from messages fd");
  else
    printf ("trapped {%s}\n", buf);

  ply_init_control_untrap_messages (control);
  ply_init_control_close (control);
  ply_init_control_free (control);

  return exit_code;
}

#endif /* PLY_INIT_CONTROL_ENABLE_TEST */

/* vim: set ts=4 sw=4 expandtab autoindent cindent cino={.5s,(0: */
