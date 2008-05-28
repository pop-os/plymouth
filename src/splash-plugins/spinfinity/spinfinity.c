/* spinfinity.c - boot splash plugin
 *
 * Copyright (C) 2007, 2008 Red Hat, Inc.
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
#include <wchar.h>

#include "ply-boot-splash-plugin.h"
#include "ply-buffer.h"
#include "ply-event-loop.h"
#include "ply-list.h"
#include "ply-logger.h"
#include "ply-frame-buffer.h"
#include "ply-image.h"
#include "ply-utils.h"
#include "ply-window.h"

#include "throbber.h"

#include <linux/kd.h>

#ifndef FRAMES_PER_SECOND
#define FRAMES_PER_SECOND 30
#endif

typedef struct
{
  int number_of_bullets;
  ply_frame_buffer_area_t area;
} entry_t;

struct _ply_boot_splash_plugin
{
  ply_event_loop_t *loop;
  ply_frame_buffer_t *frame_buffer;
  ply_frame_buffer_area_t box_area, lock_area;
  ply_image_t *logo_image;
  ply_image_t *bullet_image;
  ply_image_t *lock_image;
  ply_image_t *entry_image;
  ply_image_t *box_image;
  ply_window_t *window;

  entry_t *entry;
  throbber_t *throbber;

  ply_boot_splash_password_answer_handler_t password_answer_handler;
  void *password_answer_data;
};

static void detach_from_event_loop (ply_boot_splash_plugin_t *plugin);
static void draw_password_entry (ply_boot_splash_plugin_t *plugin);

ply_boot_splash_plugin_t *
create_plugin (void)
{
  ply_boot_splash_plugin_t *plugin;

  srand ((int) ply_get_timestamp ());
  plugin = calloc (1, sizeof (ply_boot_splash_plugin_t));

  plugin->frame_buffer = ply_frame_buffer_new (NULL);
  plugin->logo_image = ply_image_new (PLYMOUTH_IMAGE_DIR "fedora-logo.png");
  plugin->lock_image = ply_image_new (PLYMOUTH_IMAGE_DIR "lock.png");
  plugin->bullet_image = ply_image_new (PLYMOUTH_IMAGE_DIR "bullet.png");
  plugin->entry_image = ply_image_new (PLYMOUTH_IMAGE_DIR "entry.png");
  plugin->box_image = ply_image_new (PLYMOUTH_IMAGE_DIR "box.png");

  plugin->throbber = throbber_new ("throbber-");

  return plugin;
}

static entry_t *
entry_new (int x,
           int y,
           int width,
           int height)
{

  entry_t *entry;

  entry = calloc (1, sizeof (entry_t));
  entry->area.x = x;
  entry->area.y = y;
  entry->area.width = width;
  entry->area.height = height;

  return entry;
}

static void
entry_free (entry_t *entry)
{
  free (entry);
}

void
destroy_plugin (ply_boot_splash_plugin_t *plugin)
{
  if (plugin == NULL)
    return;

  ply_image_free (plugin->logo_image);
  ply_image_free (plugin->bullet_image);
  ply_image_free (plugin->entry_image);
  ply_image_free (plugin->box_image);
  ply_image_free (plugin->lock_image);
  ply_frame_buffer_free (plugin->frame_buffer);
  throbber_free (plugin->throbber);
  free (plugin);
}

static void
draw_logo (ply_boot_splash_plugin_t *plugin)
{
  ply_frame_buffer_area_t logo_area;
  uint32_t *logo_data;
  long width, height;

  width = ply_image_get_width (plugin->logo_image);
  height = ply_image_get_height (plugin->logo_image);
  logo_data = ply_image_get_data (plugin->logo_image);
  ply_frame_buffer_get_size (plugin->frame_buffer, &logo_area);
  logo_area.x = (logo_area.width / 2) - (width / 2);
  logo_area.y = (logo_area.height / 2) - (height / 2);
  logo_area.width = width;
  logo_area.height = height;

  ply_frame_buffer_pause_updates (plugin->frame_buffer);
  ply_frame_buffer_fill_with_color (plugin->frame_buffer, &logo_area,
                                    0.0, 0.43, .71, 1.0);
  ply_frame_buffer_fill_with_argb32_data (plugin->frame_buffer, 
                                          &logo_area, 0, 0,
                                          logo_data);
  ply_frame_buffer_unpause_updates (plugin->frame_buffer);
}

static void
start_animation (ply_boot_splash_plugin_t *plugin)
{

  long width, height;
  ply_frame_buffer_area_t area;
  assert (plugin != NULL);
  assert (plugin->loop != NULL);

  ply_frame_buffer_fill_with_color (plugin->frame_buffer, NULL,
                                    0.0, 0.43, .71, 1.0);

  draw_logo (plugin);

  ply_frame_buffer_get_size (plugin->frame_buffer, &area);

  width = throbber_get_width (plugin->throbber);
  height = throbber_get_height (plugin->throbber);
  throbber_start (plugin->throbber,
                  plugin->loop,
                  plugin->frame_buffer,
                  area.width / 2.0 - width / 2.0,
                  area.width / 2.0 - height / 2.0);
}

static void
stop_animation (ply_boot_splash_plugin_t *plugin)
{
  int i;

  assert (plugin != NULL);
  assert (plugin->loop != NULL);

  throbber_stop (plugin->throbber);

  for (i = 0; i < 10; i++)
    {
      ply_frame_buffer_fill_with_color (plugin->frame_buffer, NULL,
                                        0.0, 0.43, .71, .1 + .1 * i);
    }

  ply_frame_buffer_fill_with_color (plugin->frame_buffer, NULL,
                                    0.0, 0.43, .71, 1.0);

  for (i = 0; i < 20; i++)
    {
      ply_frame_buffer_fill_with_color (plugin->frame_buffer, NULL,
                                        0.0, 0.0, 0.0, .05 + .05 * i);
    }

  ply_frame_buffer_fill_with_color (plugin->frame_buffer, NULL,
                                    0.0, 0.0, 0.0, 1.0);
}

static void
on_interrupt (ply_boot_splash_plugin_t *plugin)
{
  ply_event_loop_exit (plugin->loop, 1);
  stop_animation (plugin);
  ply_window_set_mode (plugin->window, PLY_WINDOW_MODE_TEXT);
}

static void
detach_from_event_loop (ply_boot_splash_plugin_t *plugin)
{
  plugin->loop = NULL;

  ply_window_set_mode (plugin->window, PLY_WINDOW_MODE_TEXT);
}

bool
show_splash_screen (ply_boot_splash_plugin_t *plugin,
                    ply_event_loop_t         *loop,
                    ply_window_t             *window,
                    ply_buffer_t             *boot_buffer)
{
  assert (plugin != NULL);
  assert (plugin->logo_image != NULL);
  assert (plugin->frame_buffer != NULL);

  plugin->loop = loop;
  ply_event_loop_watch_for_exit (loop, (ply_event_loop_exit_handler_t)
                                 detach_from_event_loop,
                                 plugin);

  ply_trace ("loading logo image");
  if (!ply_image_load (plugin->logo_image))
    return false;

  ply_trace ("loading lock image");
  if (!ply_image_load (plugin->lock_image))
    return false;

  ply_trace ("loading bullet image");
  if (!ply_image_load (plugin->bullet_image))
    return false;

  ply_trace ("loading entry image");
  if (!ply_image_load (plugin->entry_image))
    return false;

  ply_trace ("loading box image");
  if (!ply_image_load (plugin->box_image))
    return false;

  ply_trace ("opening frame buffer");
  if (!ply_frame_buffer_open (plugin->frame_buffer))
    {
      ply_event_loop_stop_watching_for_exit (plugin->loop, (ply_event_loop_exit_handler_t)
                                             detach_from_event_loop,
                                             plugin);
      return false;
    }

  plugin->window = window;

  if (!ply_window_set_mode (plugin->window, PLY_WINDOW_MODE_GRAPHICS))
    return false;

  ply_event_loop_watch_signal (plugin->loop,
                               SIGINT,
                               (ply_event_handler_t) 
                               on_interrupt, plugin);
  
  ply_trace ("starting boot animation");
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
hide_splash_screen (ply_boot_splash_plugin_t *plugin,
                    ply_event_loop_t         *loop,
                    ply_window_t             *window)
{
  assert (plugin != NULL);

  if (plugin->loop != NULL)
    {
      stop_animation (plugin);

      ply_event_loop_stop_watching_for_exit (plugin->loop, (ply_event_loop_exit_handler_t)
                                             detach_from_event_loop,
                                             plugin);
      detach_from_event_loop (plugin);
    }

  ply_frame_buffer_close (plugin->frame_buffer);
  ply_window_set_mode (plugin->window, PLY_WINDOW_MODE_TEXT);
}
static void
draw_password_entry (ply_boot_splash_plugin_t *plugin)
{
  ply_frame_buffer_area_t bullet_area;
  uint32_t *box_data, *lock_data, *entry_data, *bullet_data;
  int i;

  ply_frame_buffer_pause_updates (plugin->frame_buffer);

  entry_data = ply_image_get_data (plugin->entry_image);
  lock_data = ply_image_get_data (plugin->lock_image);
  box_data = ply_image_get_data (plugin->box_image);

  ply_frame_buffer_fill_with_color (plugin->frame_buffer, &plugin->box_area,
                                    0.0, 0.43, .71, 1.0);

  ply_frame_buffer_fill_with_argb32_data (plugin->frame_buffer,
                                          &plugin->box_area, 0, 0,
                                          box_data);

  ply_frame_buffer_fill_with_argb32_data (plugin->frame_buffer,
                                          &plugin->entry->area, 0, 0,
                                          entry_data);

  ply_frame_buffer_fill_with_argb32_data (plugin->frame_buffer,
                                          &plugin->lock_area, 0, 0,
                                          lock_data);

  bullet_data = ply_image_get_data (plugin->bullet_image);
  bullet_area.width = ply_image_get_width (plugin->bullet_image);
  bullet_area.height = ply_image_get_height (plugin->bullet_image);

  for (i = 0; i < plugin->entry->number_of_bullets; i++)
    {
      bullet_area.x = plugin->entry->area.x + (i + 1) * bullet_area.width;
      bullet_area.y = plugin->entry->area.y + plugin->entry->area.height / 2.0 - bullet_area.height / 2.0;

      ply_frame_buffer_fill_with_argb32_data (plugin->frame_buffer,
                                              &bullet_area, 0, 0,
                                              bullet_data);
    }
  ply_frame_buffer_unpause_updates (plugin->frame_buffer);
}

static void
show_password_entry (ply_boot_splash_plugin_t *plugin)
{
  ply_frame_buffer_area_t area;
  int x, y;
  int entry_width, entry_height;

  assert (plugin != NULL);

  ply_frame_buffer_get_size (plugin->frame_buffer, &area);
  plugin->box_area.width = ply_image_get_width (plugin->box_image);
  plugin->box_area.height = ply_image_get_height (plugin->box_image);
  plugin->box_area.x = area.width / 2.0 - plugin->box_area.width / 2.0;
  plugin->box_area.y = area.height / 2.0 - plugin->box_area.height / 2.0;

  plugin->lock_area.width = ply_image_get_width (plugin->lock_image);
  plugin->lock_area.height = ply_image_get_height (plugin->lock_image);

  entry_width = ply_image_get_width (plugin->entry_image);
  entry_height = ply_image_get_height (plugin->entry_image);

  x = area.width / 2.0 - (plugin->lock_area.width + entry_width) / 2.0 + plugin->lock_area.width;
  y = area.height / 2.0 - entry_height / 2.0;

  plugin->entry = entry_new (x, y, entry_width, entry_height);

  plugin->lock_area.x = area.width / 2.0 - (plugin->lock_area.width + entry_width) / 2.0;
  plugin->lock_area.y = area.height / 2.0 - plugin->lock_area.height / 2.0;

  ply_frame_buffer_fill_with_color (plugin->frame_buffer, NULL,
                                    0.0, 0.43, .71, 1.0);
  draw_password_entry (plugin);
}

void
ask_for_password (ply_boot_splash_plugin_t *plugin,
                  ply_boot_splash_password_answer_handler_t answer_handler,
                  void *answer_data)
{
  plugin->password_answer_handler = answer_handler;
  plugin->password_answer_data = answer_data;

  stop_animation (plugin);
  show_password_entry (plugin);
}

void
on_keyboard_input (ply_boot_splash_plugin_t *plugin,
                   const char               *keyboard_input,
                   size_t                    character_size)
{
  if (plugin->password_answer_handler == NULL)
    return;

  plugin->entry->number_of_bullets++;
  draw_password_entry (plugin);
}

void
on_backspace (ply_boot_splash_plugin_t *plugin)
{
  plugin->entry->number_of_bullets--;
  draw_password_entry (plugin);
}

void
on_enter (ply_boot_splash_plugin_t *plugin,
          const char               *text)
{
  plugin->password_answer_handler (plugin->password_answer_data,
                                   text);
  plugin->entry->number_of_bullets = 0;
  entry_free (plugin->entry);
  plugin->entry = NULL;
  start_animation (plugin);
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
      .ask_for_password = ask_for_password,
      .on_keyboard_input = on_keyboard_input,
      .on_backspace = on_backspace,
      .on_enter = on_enter
    };

  return &plugin_interface;
}

/* vim: set ts=4 sw=4 expandtab autoindent cindent cino={.5s,(0: */
