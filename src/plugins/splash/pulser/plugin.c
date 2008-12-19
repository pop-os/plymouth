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
#include <wchar.h>

#include "ply-trigger.h"
#include "ply-boot-splash-plugin.h"
#include "ply-buffer.h"
#include "ply-event-loop.h"
#include "ply-list.h"
#include "ply-logger.h"
#include "ply-frame-buffer.h"
#include "ply-image.h"
#include "ply-text-pulser.h"
#include "ply-utils.h"
#include "ply-window.h"

#include <linux/kd.h>

#define CLEAR_LINE_SEQUENCE "\033[2K\r\n"
#define BACKSPACE "\b\033[0K"

struct _ply_boot_splash_plugin
{
  ply_event_loop_t *loop;

  ply_window_t *window;

  ply_text_pulser_t *pulser;

  uint32_t keyboard_input_is_hidden : 1;
  uint32_t is_animating : 1;
};
void hide_splash_screen (ply_boot_splash_plugin_t *plugin,
                         ply_event_loop_t         *loop);

ply_boot_splash_plugin_t *
create_plugin (void)
{
  ply_boot_splash_plugin_t *plugin;

  ply_trace ("creating plugin");

  plugin = calloc (1, sizeof (ply_boot_splash_plugin_t));
  plugin->pulser = ply_text_pulser_new ();

  return plugin;
}

static void
detach_from_event_loop (ply_boot_splash_plugin_t *plugin)
{
  plugin->loop = NULL;

  ply_trace ("detaching from event loop");
}

void
destroy_plugin (ply_boot_splash_plugin_t *plugin)
{
  ply_trace ("destroying plugin");

  if (plugin == NULL)
    return;

  /* It doesn't ever make sense to keep this plugin on screen
   * after exit
   */
  hide_splash_screen (plugin, plugin->loop);

  ply_text_pulser_free (plugin->pulser);

  free (plugin);
}

static void
start_animation (ply_boot_splash_plugin_t *plugin)
{

  int window_width, window_height;
  int width, height;
  assert (plugin != NULL);
  assert (plugin->loop != NULL);

  if (plugin->is_animating)
     return;

  ply_window_set_color_hex_value (plugin->window,
                                  PLY_WINDOW_COLOR_BROWN,
                                  PLYMOUTH_BACKGROUND_END_COLOR);
  ply_window_set_color_hex_value (plugin->window,
                                  PLY_WINDOW_COLOR_BLUE,
                                  PLYMOUTH_BACKGROUND_START_COLOR);
  ply_window_set_color_hex_value (plugin->window,
                                  PLY_WINDOW_COLOR_GREEN,
                                  PLYMOUTH_BACKGROUND_COLOR);

  ply_window_set_background_color (plugin->window, PLY_WINDOW_COLOR_BLUE);
  ply_window_clear_screen (plugin->window);
  ply_window_hide_text_cursor (plugin->window);

  window_width = ply_window_get_number_of_text_columns (plugin->window);
  window_height = ply_window_get_number_of_text_rows (plugin->window);
  width = ply_text_pulser_get_number_of_columns (plugin->pulser);
  height = ply_text_pulser_get_number_of_rows (plugin->pulser);
  ply_text_pulser_start (plugin->pulser,
                         plugin->loop,
                         plugin->window,
                         window_width / 2.0 - width / 2.0,
                         window_height / 2.0 - height / 2.0);

  plugin->is_animating = true;
}

static void
stop_animation (ply_boot_splash_plugin_t *plugin)
{
  assert (plugin != NULL);
  assert (plugin->loop != NULL);

  if (!plugin->is_animating)
     return;

  plugin->is_animating = false;

  ply_text_pulser_stop (plugin->pulser);
}

void
on_keyboard_input (ply_boot_splash_plugin_t *plugin,
                   const char               *keyboard_input,
                   size_t                    character_size)
{
}

void
on_backspace (ply_boot_splash_plugin_t *plugin)
{
}

void
on_enter (ply_boot_splash_plugin_t *plugin,
          const char               *line)
{
}

void
on_draw (ply_boot_splash_plugin_t *plugin,
         int                       x,
         int                       y,
         int                       width,
         int                       height)
{
  ply_window_set_background_color (plugin->window, PLY_WINDOW_COLOR_BLUE);
  ply_window_clear_screen (plugin->window);
}

void
on_erase (ply_boot_splash_plugin_t *plugin,
          int                       x,
          int                       y,
          int                       width,
          int                       height)
{
  ply_window_set_background_color (plugin->window, PLY_WINDOW_COLOR_BLUE);
  ply_window_clear_screen (plugin->window);
}

void
add_window (ply_boot_splash_plugin_t *plugin,
            ply_window_t             *window)
{
  plugin->window = window;
}

void
remove_window (ply_boot_splash_plugin_t *plugin,
               ply_window_t             *window)
{
  plugin->window = NULL;
}

bool
show_splash_screen (ply_boot_splash_plugin_t *plugin,
                    ply_event_loop_t         *loop,
                    ply_buffer_t             *boot_buffer)
{
  assert (plugin != NULL);

  ply_show_new_kernel_messages (false);

  ply_window_add_keyboard_input_handler (plugin->window,
                                         (ply_window_keyboard_input_handler_t)
                                         on_keyboard_input, plugin);
  ply_window_add_backspace_handler (plugin->window,
                                    (ply_window_backspace_handler_t)
                                    on_backspace, plugin);
  ply_window_add_enter_handler (plugin->window,
                                (ply_window_enter_handler_t)
                                on_enter, plugin);
  ply_window_set_draw_handler (plugin->window,
                               (ply_window_draw_handler_t)
                                on_draw, plugin);
  ply_window_set_erase_handler (plugin->window,
                                (ply_window_erase_handler_t)
                                on_erase, plugin);

  plugin->loop = loop;
  ply_event_loop_watch_for_exit (loop, (ply_event_loop_exit_handler_t)
                                 detach_from_event_loop,
                                 plugin);

  start_animation (plugin);

  return true;
}

void
update_status (ply_boot_splash_plugin_t *plugin,
               const char               *status)
{
  assert (plugin != NULL);

  ply_trace ("status update");
}

void
hide_splash_screen (ply_boot_splash_plugin_t *plugin,
                    ply_event_loop_t         *loop)
{
  assert (plugin != NULL);

  ply_trace ("hiding splash screen");

  if (plugin->loop != NULL)
    {
      stop_animation (plugin);

      ply_event_loop_stop_watching_for_exit (plugin->loop,
                                             (ply_event_loop_exit_handler_t)
                                             detach_from_event_loop,
                                             plugin);
      detach_from_event_loop (plugin);
    }

  if (plugin->window != NULL)
    {
      ply_window_remove_keyboard_input_handler (plugin->window, (ply_window_keyboard_input_handler_t) on_keyboard_input);
      ply_window_remove_backspace_handler (plugin->window, (ply_window_backspace_handler_t) on_backspace);
      ply_window_remove_enter_handler (plugin->window, (ply_window_enter_handler_t) on_enter);
      ply_window_set_draw_handler (plugin->window, NULL, NULL);
      ply_window_set_erase_handler (plugin->window, NULL, NULL);

      ply_window_set_background_color (plugin->window, PLY_WINDOW_COLOR_DEFAULT);
      ply_window_clear_screen (plugin->window);
      ply_window_show_text_cursor (plugin->window);
      ply_window_reset_colors (plugin->window);
    }

  ply_show_new_kernel_messages (true);
}

void display_normal (ply_boot_splash_plugin_t *plugin)
{
  start_animation(plugin);
}

void
display_password (ply_boot_splash_plugin_t *plugin,
                  const char               *prompt,
                  int                       bullets)
{
      int window_width, window_height;
      int i;
      stop_animation (plugin);
      ply_window_set_background_color (plugin->window, PLY_WINDOW_COLOR_DEFAULT);
      ply_window_clear_screen (plugin->window);

      window_width = ply_window_get_number_of_text_columns (plugin->window);
      window_height = ply_window_get_number_of_text_rows (plugin->window);
      
      if (!prompt)
        prompt = "Password";
      
      ply_window_set_text_cursor_position (plugin->window, 0, window_height / 2);
      
      for (i=0; i < window_width; i++)
        {
          write (STDOUT_FILENO, " ", strlen (" "));
        }
      ply_window_set_text_cursor_position (plugin->window,
                                           window_width / 2 - (strlen (prompt)),
                                           window_height / 2);
      write (STDOUT_FILENO, prompt, strlen (prompt));
      write (STDOUT_FILENO, ":", strlen (":"));
      
      for (i=0; i < bullets; i++)
        {
          write (STDOUT_FILENO, "•", strlen ("•"));
        }
      ply_window_show_text_cursor (plugin->window);
}

void
display_question (ply_boot_splash_plugin_t *plugin,
                  const char               *prompt,
                  const char               *entry_text)
{
      int window_width, window_height;
      int i;
      stop_animation (plugin);
      ply_window_set_background_color (plugin->window, PLY_WINDOW_COLOR_DEFAULT);
      ply_window_clear_screen (plugin->window);

      window_width = ply_window_get_number_of_text_columns (plugin->window);
      window_height = ply_window_get_number_of_text_rows (plugin->window);
      
      if (!prompt)
        prompt = "";
      
      ply_window_set_text_cursor_position (plugin->window,
                                           0, window_height / 2);
      
      for (i=0; i < window_width; i++)
        {
          write (STDOUT_FILENO, " ", strlen (" "));
        }
      ply_window_set_text_cursor_position (plugin->window,
                                           window_width / 2 - (strlen (prompt)),
                                           window_height / 2);
      write (STDOUT_FILENO, prompt, strlen (prompt));
      write (STDOUT_FILENO, ":", strlen (":"));
      
      write (STDOUT_FILENO, entry_text, strlen (entry_text));
      ply_window_show_text_cursor (plugin->window);
}


ply_boot_splash_plugin_interface_t *
ply_boot_splash_plugin_get_interface (void)
{
  static ply_boot_splash_plugin_interface_t plugin_interface =
    {
      .create_plugin = create_plugin,
      .destroy_plugin = destroy_plugin,
      .add_window = add_window,
      .remove_window = remove_window,
      .show_splash_screen = show_splash_screen,
      .update_status = update_status,
      .hide_splash_screen = hide_splash_screen,
      .display_normal = display_normal,
      .display_password = display_password,
      .display_question = display_question,      
    };

  return &plugin_interface;
}

/* vim: set ts=4 sw=4 expandtab autoindent cindent cino={.5s,(0: */
