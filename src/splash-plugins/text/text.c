/* text.c - boot splash plugin
 *
 * Copyright (C) 2008 Red Hat, Inc.
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

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>
#include <values.h>

#include "ply-boot-splash-plugin.h"
#include "ply-event-loop.h"
#include "ply-list.h"
#include "ply-logger.h"
#include "ply-frame-buffer.h"
#include "ply-image.h"
#include "ply-utils.h"
#include "ply-window.h"

#include <linux/kd.h>

struct _ply_boot_splash_plugin
{
  ply_event_loop_t *loop;

  int console_fd;
};

ply_boot_splash_plugin_t *
create_plugin (void)
{
  ply_boot_splash_plugin_t *plugin;

  ply_trace ("creating plugin");

  plugin = calloc (1, sizeof (ply_boot_splash_plugin_t));
  plugin->console_fd = -1;

  return plugin;
}

void
destroy_plugin (ply_boot_splash_plugin_t *plugin)
{
  ply_trace ("destroying plugin");

  if (plugin == NULL)
    return;

  free (plugin);
}

static bool
open_console (ply_boot_splash_plugin_t *plugin)
{
  assert (plugin != NULL);

  plugin->console_fd = open ("/dev/tty1", O_RDWR | O_APPEND | O_NOCTTY);

  if (plugin->console_fd < 0)
    return false;

  return true;
}

bool
show_splash_screen (ply_boot_splash_plugin_t *plugin,
                    ply_window_t             *window,
                    ply_buffer_t             *boot_buffer)
{
  assert (plugin != NULL);

  ply_trace ("opening console");
  if (!open_console (plugin))
    return false;

  return true;
}

void
update_status (ply_boot_splash_plugin_t *plugin,
               const char               *status)
{
  assert (plugin != NULL);

  ply_trace ("status update");
  write (plugin->console_fd, ".", 1);
}

static void
detach_from_event_loop (ply_boot_splash_plugin_t *plugin)
{
  plugin->loop = NULL;

  ply_trace ("detaching from event loop");
}

void
hide_splash_screen (ply_boot_splash_plugin_t *plugin,
                    ply_window_t             *window)
{
  assert (plugin != NULL);

  ply_trace ("hiding splash screen");

  if (plugin->loop != NULL)
    {
      ply_event_loop_stop_watching_for_exit (plugin->loop,
                                             (ply_event_loop_exit_handler_t)
                                             detach_from_event_loop,
                                             plugin);
      detach_from_event_loop (plugin);
    }
}

void
attach_to_event_loop (ply_boot_splash_plugin_t *plugin,
                      ply_event_loop_t         *loop)
{
  plugin->loop = loop;

  ply_trace ("attaching to event loop");
  ply_event_loop_watch_for_exit (loop, (ply_event_loop_exit_handler_t)
                                 detach_from_event_loop,
                                 plugin);
}

char *
ask_for_password (ply_boot_splash_plugin_t *plugin)
{
  char           answer[1024];
  struct termios initial_term_attributes;
  struct termios noecho_term_attributes;

  tcgetattr (STDIN_FILENO, &initial_term_attributes);
  noecho_term_attributes = initial_term_attributes;
  noecho_term_attributes.c_lflag &= ~ECHO;

  printf ("Password: ");

  if (tcsetattr (STDIN_FILENO, TCSAFLUSH, &noecho_term_attributes) != 0) {
    fprintf (stderr, "Could not set terminal attributes\n");
    return NULL;
  }

  fgets (answer, sizeof (answer), stdin);
  answer[strlen (answer) - 1] = '\0';

  tcsetattr (STDIN_FILENO, TCSANOW, &initial_term_attributes);

  printf ("\n");

  return strdup (answer);
}

void
on_keyboard_input (ply_boot_splash_plugin_t *plugin,
                   const char               *keyboard_input)
{
}

ply_boot_splash_plugin_interface_t *
ply_boot_splash_plugin_get_interface (void)
{
  static ply_boot_splash_plugin_interface_t plugin_interface =
    {
      .create_plugin = create_plugin,
      .destroy_plugin = destroy_plugin,
      .show_splash_screen = show_splash_screen,
      .update_status = update_status,
      .hide_splash_screen = hide_splash_screen,
      .attach_to_event_loop = attach_to_event_loop,
      .ask_for_password = ask_for_password,
      .on_keyboard_input = on_keyboard_input
    };

  return &plugin_interface;
}

/* vim: set ts=4 sw=4 expandtab autoindent cindent cino={.5s,(0: */
