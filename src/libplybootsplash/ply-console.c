/* ply-console.c - console APIs
 *
 * Copyright (C) 2009 Red Hat, Inc.
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
#include "ply-console.h"

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
#include <termios.h>
#include <unistd.h>
#include <wchar.h>

#include <linux/kd.h>
#include <linux/major.h>
#include <linux/vt.h>

#include "ply-event-loop.h"
#include "ply-list.h"
#include "ply-logger.h"
#include "ply-utils.h"

#ifndef TEXT_PALETTE_SIZE
#define TEXT_PALETTE_SIZE 48
#endif

typedef struct
{
  ply_console_active_vt_changed_handler_t handler;
  void *user_data;
} ply_console_active_vt_changed_closure_t;

struct _ply_console
{
  ply_event_loop_t *loop;

  int fd;
  int active_vt;
  int next_active_vt;

  ply_list_t *vt_change_closures;
  ply_fd_watch_t *fd_watch;

  uint32_t is_open : 1;
  uint32_t is_watching_for_vt_changes : 1;
  uint32_t should_ignore_mode_changes : 1;
};

static bool ply_console_open_device (ply_console_t *console);

ply_console_t *
ply_console_new (void)
{
  ply_console_t *console;

  console = calloc (1, sizeof (ply_console_t));

  console->loop = ply_event_loop_get_default ();
  console->vt_change_closures = ply_list_new ();
  console->fd = -1;

  return console;
}

static void
ply_console_look_up_active_vt (ply_console_t *console)
{
  struct vt_stat console_state = { 0 };

  if (ioctl (console->fd, VT_GETSTATE, &console_state) < 0)
    return;

  console->active_vt = console_state.v_active;
}

void
ply_console_set_mode (ply_console_t     *console,
                      ply_console_mode_t mode)
{

  assert (console != NULL);
  assert (mode == PLY_CONSOLE_MODE_TEXT || mode == PLY_CONSOLE_MODE_GRAPHICS);

  if (console->should_ignore_mode_changes)
    return;

  switch (mode)
    {
      case PLY_CONSOLE_MODE_TEXT:
        if (ioctl (console->fd, KDSETMODE, KD_TEXT) < 0)
          return;
        break;

      case PLY_CONSOLE_MODE_GRAPHICS:
        if (ioctl (console->fd, KDSETMODE, KD_GRAPHICS) < 0)
          return;
        break;
    }
}

void
ply_console_ignore_mode_changes (ply_console_t *console,
                                 bool           should_ignore)
{
  console->should_ignore_mode_changes = should_ignore;
}

static void
on_tty_disconnected (ply_console_t *console)
{
  ply_trace ("console tty disconnected (fd %d)", console->fd);
  console->fd_watch = NULL;
  console->fd = -1;

  ply_trace ("trying to reopen console");
  ply_console_open_device (console);
}

static void
do_active_vt_changed (ply_console_t *console)
{
  ply_list_node_t *node;

  node = ply_list_get_first_node (console->vt_change_closures);
  while (node != NULL)
    {
      ply_console_active_vt_changed_closure_t *closure;
      ply_list_node_t *next_node;

      closure = ply_list_node_get_data (node);
      next_node = ply_list_get_next_node (console->vt_change_closures, node);

      if (closure->handler != NULL)
        closure->handler (closure->user_data, console);

      node = next_node;
    }
}

static void
on_leave_vt (ply_console_t *console)
{
  ioctl (console->fd, VT_RELDISP, 1);

  if (console->next_active_vt > 0)
    {
      ioctl (console->fd, VT_WAITACTIVE, console->next_active_vt);
      console->next_active_vt = 0;
    }

  ply_console_look_up_active_vt (console);
  do_active_vt_changed (console);
}

static void
on_enter_vt (ply_console_t *console)
{
  ioctl (console->fd, VT_RELDISP, VT_ACKACQ);

  ply_console_look_up_active_vt (console);
  do_active_vt_changed (console);
}

static void
ply_console_watch_for_vt_changes (ply_console_t *console)
{
  assert (console != NULL);

  struct vt_mode mode = { 0 };

  if (console->fd < 0)
    return;

  if (console->is_watching_for_vt_changes)
    return;

  mode.mode = VT_PROCESS;
  mode.relsig = SIGUSR1;
  mode.acqsig = SIGUSR2;

  if (ioctl (console->fd, VT_SETMODE, &mode) < 0)
    return;

  ply_event_loop_watch_signal (console->loop,
                               SIGUSR1,
                               (ply_event_handler_t)
                               on_leave_vt, console);

  ply_event_loop_watch_signal (console->loop,
                               SIGUSR2,
                               (ply_event_handler_t)
                               on_enter_vt, console);

  console->is_watching_for_vt_changes = true;
}

static void
ply_console_stop_watching_for_vt_changes (ply_console_t *console)
{
  struct vt_mode mode = { 0 };

  if (!console->is_watching_for_vt_changes)
    return;

  console->is_watching_for_vt_changes = false;

  ply_event_loop_stop_watching_signal (console->loop, SIGUSR1);
  ply_event_loop_stop_watching_signal (console->loop, SIGUSR2);

  mode.mode = VT_AUTO;
  ioctl (console->fd, VT_SETMODE, &mode);
}

static bool
ply_console_open_device (ply_console_t *console)
{
  assert (console != NULL);
  assert (console->fd < 0);
  assert (console->fd_watch == NULL);

  console->fd = open ("/dev/tty0", O_RDWR | O_NOCTTY);

  if (console->fd < 0)
    return false;

  console->fd_watch = ply_event_loop_watch_fd (console->loop, console->fd,
                                                   PLY_EVENT_LOOP_FD_STATUS_NONE,
                                                   (ply_event_handler_t) NULL,
                                                   (ply_event_handler_t) on_tty_disconnected,
                                                   console);

  ply_console_look_up_active_vt (console);

  return true;
}

bool
ply_console_open (ply_console_t *console)
{
  assert (console != NULL);

  if (!ply_console_open_device (console))
    {
      ply_trace ("could not open console: %m");
      return false;
    }

  ply_console_watch_for_vt_changes (console);

  console->is_open = true;

  return true;
}


int
ply_console_get_fd (ply_console_t *console)
{
  return console->fd;
}

bool
ply_console_is_open (ply_console_t *console)
{
  return console->is_open;
}

void
ply_console_close (ply_console_t *console)
{
  console->is_open = false;

  ply_console_stop_watching_for_vt_changes (console);

  if (console->fd_watch != NULL)
    {
      ply_trace ("stop watching tty fd");
      ply_event_loop_stop_watching_fd (console->loop, console->fd_watch);
      console->fd_watch = NULL;
    }

  close (console->fd);
  console->fd = -1;
}

static void
free_vt_change_closures (ply_console_t *console)
{
  ply_list_node_t *node;

  node = ply_list_get_first_node (console->vt_change_closures);
  while (node != NULL)
    {
      ply_console_active_vt_changed_closure_t *closure;
      ply_list_node_t *next_node;

      closure = ply_list_node_get_data (node);
      next_node = ply_list_get_next_node (console->vt_change_closures, node);

      free (closure);
      node = next_node;
    }
  ply_list_free (console->vt_change_closures);
}

void
ply_console_free (ply_console_t *console)
{
  if (console == NULL)
    return;

  ply_console_close (console);

  free_vt_change_closures (console);
  free (console);
}

int
ply_console_get_active_vt (ply_console_t *console)
{
  return console->active_vt;
}

bool
ply_console_set_active_vt (ply_console_t *console,
                           int            vt_number)
{
  assert (console != NULL);

  if (vt_number <= 0)
    return false;

  if (vt_number == console->active_vt)
    return true;

  if (ioctl (console->fd, VT_ACTIVATE, vt_number) < 0)
    return false;

  console->next_active_vt = vt_number;

  return true;
}

void
ply_console_watch_for_active_vt_change (ply_console_t *console,
                                        ply_console_active_vt_changed_handler_t active_vt_changed_handler,
                                        void *user_data)
{
  ply_console_active_vt_changed_closure_t *closure;

  closure = calloc (1, sizeof (*closure));
  closure->handler = active_vt_changed_handler;
  closure->user_data = user_data;

  ply_list_append_data (console->vt_change_closures, closure);
}

void
ply_console_stop_watching_for_active_vt_change (ply_console_t *console,
                                                ply_console_active_vt_changed_handler_t active_vt_changed_handler,
                                                void *user_data)
{
  ply_list_node_t *node;

  node = ply_list_get_first_node (console->vt_change_closures);
  while (node != NULL)
    {
      ply_console_active_vt_changed_closure_t *closure;
      ply_list_node_t *next_node;

      closure = ply_list_node_get_data (node);
      next_node = ply_list_get_next_node (console->vt_change_closures, node);

      if (closure->handler == active_vt_changed_handler &&
          closure->user_data == user_data)
        {
          free (closure);
          ply_list_remove_node (console->vt_change_closures, node);
        }

      node = next_node;
    }
}

/* vim: set ts=4 sw=4 et ai ci cino={.5s,^-2,+.5s,t0,g0,e-2,n-2,p2s,(0,=.5s,:.5s */
