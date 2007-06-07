/* fedora-fade-in.c - boot splash plugin
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
#include <values.h>
#include <unistd.h>

#include "ply-boot-splash-plugin.h"
#include "ply-event-loop.h"
#include "ply-frame-buffer.h"
#include "ply-image.h"
#include "ply-utils.h"

#include <linux/kd.h>

#ifndef FRAMES_PER_SECOND
#define FRAMES_PER_SECOND 50
#endif

struct _ply_boot_splash_plugin
{
  ply_event_loop_t *loop;
  ply_frame_buffer_t *frame_buffer;
  ply_image_t *image;

  int console_fd;
  double start_time;
};

ply_boot_splash_plugin_t *
create_plugin (void)
{
  ply_boot_splash_plugin_t *plugin;

  plugin = calloc (1, sizeof (ply_boot_splash_plugin_t));
  plugin->console_fd = -1;
  plugin->start_time = 0.0;

  plugin->frame_buffer = ply_frame_buffer_new (NULL);
  plugin->image = ply_image_new ("/booting.png");

  return plugin;
}

void
destroy_plugin (ply_boot_splash_plugin_t *plugin)
{
  if (plugin == NULL)
    return;

  ply_image_free (plugin->image);
  ply_frame_buffer_free (plugin->frame_buffer);
  free (plugin);
}

static bool
set_graphics_mode (ply_boot_splash_plugin_t *plugin)
{
  assert (plugin != NULL);

  if (ioctl (plugin->console_fd, KDSETMODE, KD_GRAPHICS) < 0)
    return false;

  return true;
}

static void
set_text_mode (ply_boot_splash_plugin_t *plugin)
{
  assert (plugin != NULL);

  ioctl (plugin->console_fd, KDSETMODE, KD_TEXT);
}

static bool
open_console (ply_boot_splash_plugin_t *plugin)
{
  assert (plugin != NULL);

  plugin->console_fd = open ("/dev/tty0", O_RDWR);

  if (plugin->console_fd < 0)
    return false;

  return true;
}

static void
close_console (ply_boot_splash_plugin_t *plugin)
{
  close (plugin->console_fd);
  plugin->console_fd = -1;
}

static void
animate_at_time (ply_boot_splash_plugin_t *plugin,
                 double                    time)
{
  ply_frame_buffer_t *buffer;
  ply_image_t *image;
  ply_frame_buffer_area_t area;
  uint32_t *data;
  long width, height;
  static double last_opacity = 0.0;
  double opacity = 0.0;

  buffer = plugin->frame_buffer;
  image = plugin->image;
  
  data = ply_image_get_data (image);
  width = ply_image_get_width (image);
  height = ply_image_get_height (image);

  ply_frame_buffer_get_size (buffer, &area);
  area.x = (area.width / 2) - (width / 2);
  area.y = (area.height / 2) - (height / 2);
  area.width = width;
  area.height = height;

  opacity = .5 * sin ((time / 4) * (2 * M_PI)) + .8;
  opacity = CLAMP (opacity, 0, 1.0);

  if (fabs (opacity - last_opacity) <= DBL_MIN)
    return;

  last_opacity = opacity;

  ply_frame_buffer_pause_updates (buffer);
  ply_frame_buffer_fill_with_color (buffer, &area, 0.1, 0.1, .7, 1.0);
  ply_frame_buffer_fill_with_argb32_data_at_opacity (buffer, &area, 
                                                     0, 0, width, height, 
                                                     data, opacity);
  ply_frame_buffer_unpause_updates (buffer);
}

static void
on_timeout (ply_boot_splash_plugin_t *plugin)
{
  double sleep_time;
  double now;

  set_graphics_mode (plugin);
  now = ply_get_timestamp ();
  animate_at_time (plugin,
                   now - plugin->start_time);
  sleep_time = 1.0 / FRAMES_PER_SECOND;
  sleep_time = MAX (sleep_time - (ply_get_timestamp () - now),
                    0.010);

  ply_event_loop_watch_for_timeout (plugin->loop, 
                                    sleep_time,
                                    (ply_event_loop_timeout_handler_t)
                                    on_timeout, plugin);
}

static void
start_animation (ply_boot_splash_plugin_t *plugin)
{
  assert (plugin != NULL);
  assert (plugin->loop != NULL);
    
  ply_event_loop_watch_for_timeout (plugin->loop, 
                                    1.0 / FRAMES_PER_SECOND,
                                    (ply_event_loop_timeout_handler_t)
                                    on_timeout, plugin);

  plugin->start_time = ply_get_timestamp ();
  ply_frame_buffer_fill_with_color (plugin->frame_buffer, NULL, 
                                    0.1, 0.1, .7, 1.0);
}

static void
stop_animation (ply_boot_splash_plugin_t *plugin)
{
  assert (plugin != NULL);
  assert (plugin->loop != NULL);
    
  ply_frame_buffer_fill_with_color (plugin->frame_buffer, 
                                    NULL, 0.0, 0.0, 0.0, 1.0);
  set_text_mode (plugin);
}

bool
show_splash_screen (ply_boot_splash_plugin_t *plugin)
{
  assert (plugin != NULL);
  assert (plugin->image != NULL);
  assert (plugin->frame_buffer != NULL);

  if (!ply_image_load (plugin->image))
    return false;

  if (!ply_frame_buffer_open (plugin->frame_buffer))
    return false;

  if (!open_console (plugin))
    return false;

  if (!set_graphics_mode (plugin))
    {
      ply_save_errno ();
      close_console (plugin);
      ply_restore_errno ();
      return false;
    }
  
  start_animation (plugin);

  return true;
}

void
update_status (ply_boot_splash_plugin_t *plugin,
               const char               *status)
{
  assert (plugin != NULL);
}

void
hide_splash_screen (ply_boot_splash_plugin_t *plugin)
{
  assert (plugin != NULL);

  stop_animation (plugin);
  ply_frame_buffer_close (plugin->frame_buffer);
}

static void
detach_from_event_loop (ply_boot_splash_plugin_t *plugin)
{
  plugin->loop = NULL;

  set_text_mode (plugin);
}

void
attach_to_event_loop (ply_boot_splash_plugin_t *plugin,
                      ply_event_loop_t         *loop)
{
  plugin->loop = loop;

  ply_event_loop_watch_for_exit (loop, (ply_event_loop_exit_handler_t) 
                                 detach_from_event_loop,
                                 plugin); 
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
      .attach_to_event_loop = attach_to_event_loop
    };

  return &plugin_interface;
}

/* vim: set ts=4 sw=4 expandtab autoindent cindent cino={.5s,(0: */
