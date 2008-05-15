/* ply-window.h - APIs for putting up a window screen
 *
 * Copyright (C) 2007 Red Hat, Inc.
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
 * Written by: Ray Strode <rstrode@redhat.com>
 */
#include "config.h"
#include "ply-window.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <linux/kd.h>

#include "ply-event-loop.h"
#include "ply-logger.h"
#include "ply-utils.h"

struct _ply_window
{
  ply_event_loop_t *loop;

  char *tty_name;
  int   tty_fd;

  ply_window_mode_t mode;
};


ply_window_t *
ply_window_new (const char *tty_name)
{
  ply_window_t *window;

  assert (tty_name != NULL);

  window = calloc (1, sizeof (ply_window_t));
  window->loop = NULL;
  window->tty_name = strdup (tty_name);
  window->tty_fd = -1;

  return window;
}

bool
ply_window_open (ply_window_t *window)
{
  assert (window != NULL);
  assert (window->tty_name != NULL);
  assert (window->tty_fd < 0);

  window->tty_fd = open (window->tty_name, O_RDWR | O_NOCTTY);

  if (window->tty_fd < 0)
    return false;

  if (!ply_window_set_mode (window, PLY_WINDOW_MODE_TEXT))
    return false;

  return true;
}

void
ply_window_close (ply_window_t *window)
{
  ply_window_set_mode (window, PLY_WINDOW_MODE_TEXT);

  close (window->tty_fd);
  window->tty_fd = -1;
}

bool
ply_window_set_mode (ply_window_t      *window,
                     ply_window_mode_t  mode)
{
  assert (window != NULL);
  assert (mode == PLY_WINDOW_MODE_TEXT || mode == PLY_WINDOW_MODE_GRAPHICS);

  switch (mode)
    {
      case PLY_WINDOW_MODE_TEXT:
        if (ioctl (window->tty_fd, KDSETMODE, KD_TEXT) < 0)
          return false;
        break;

      case PLY_WINDOW_MODE_GRAPHICS:
        if (ioctl (window->tty_fd, KDSETMODE, KD_GRAPHICS) < 0)
          return false;
        break;
    }

  window->mode = mode;
  return true;
}

void
ply_window_free (ply_window_t *window)
{
  if (window == NULL)
    return;

  ply_window_set_mode (window, PLY_WINDOW_MODE_TEXT);
  ply_window_close (window);

  free (window);
}

static void
ply_window_detach_from_event_loop (ply_window_t *window)
{
  assert (window != NULL);
  window->loop = NULL;
}

void
ply_window_attach_to_event_loop (ply_window_t *window,
                                      ply_event_loop_t  *loop)
{
  assert (window != NULL);
  assert (loop != NULL);
  assert (window->loop == NULL);

  window->loop = loop;

  ply_event_loop_watch_for_exit (loop, (ply_event_loop_exit_handler_t)
                                 ply_window_detach_from_event_loop,
                                 window);
}

#ifdef PLY_WINDOW_ENABLE_TEST

#include <stdio.h>

#include "ply-event-loop.h"
#include "ply-window.h"

static void
on_timeout (ply_window_t     *window,
            ply_event_loop_t *loop)
{
  ply_event_loop_exit (loop, 0);
}

int
main (int    argc,
      char **argv)
{
  ply_event_loop_t *loop;
  ply_window_t *window;
  int exit_code;
  const char *tty_name;

  exit_code = 0;

  loop = ply_event_loop_new ();

  if (argc > 1)
    tty_name = argv[1];
  else
    tty_name = "/dev/tty1";

  window = ply_window_new (tty_name);
  ply_window_attach_to_event_loop (window, loop);

  if (!ply_window_open (window))
    {
      ply_save_errno ();
      perror ("could not open window");
      ply_restore_errno ();
      return errno;
    }

  if (!ply_window_set_mode (window, PLY_WINDOW_MODE_GRAPHICS))
    {
      ply_save_errno ();
      perror ("could not set window for graphics mode");
      ply_restore_errno ();
    }

  ply_event_loop_watch_for_timeout (loop,
                                    1.0,
                                   (ply_event_loop_timeout_handler_t)
                                   on_timeout,
                                   window);
  exit_code = ply_event_loop_run (loop);

  ply_window_close (window);
  ply_window_free (window);

  return exit_code;
}

#endif /* PLY_WINDOW_ENABLE_TEST */
/* vim: set ts=4 sw=4 expandtab autoindent cindent cino={.5s,(0: */
