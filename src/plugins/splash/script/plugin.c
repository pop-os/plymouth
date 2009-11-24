/* plugin.c - boot script plugin
 *
 * Copyright (C) 2007, 2008 Red Hat, Inc.
 *               2008, 2009 Charlie Brej <cbrej@cs.man.ac.uk>
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
 * Written by: Charlie Brej <cbrej@cs.man.ac.uk>
 *             Ray Strode <rstrode@redhat.com>
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
#include "ply-entry.h"
#include "ply-event-loop.h"
#include "ply-key-file.h"
#include "ply-list.h"
#include "ply-logger.h"
#include "ply-image.h"
#include "ply-pixel-display.h"
#include "ply-trigger.h"
#include "ply-utils.h"

#include "script.h"
#include "script-parse.h"
#include "script-object.h"
#include "script-execute.h"
#include "script-lib-image.h"
#include "script-lib-sprite.h"
#include "script-lib-plymouth.h"
#include "script-lib-math.h"
#include "script-lib-string.h"

#include <linux/kd.h>

#ifndef FRAMES_PER_SECOND
#define FRAMES_PER_SECOND 50
#endif

typedef struct
{
  ply_boot_splash_plugin_t *plugin;
  ply_pixel_display_t *display;

  script_state_t                *script_state;
  script_lib_sprite_data_t      *script_sprite_lib;
  script_lib_image_data_t       *script_image_lib;
  script_lib_plymouth_data_t    *script_plymouth_lib;
  script_lib_math_data_t        *script_math_lib;
  script_lib_string_data_t      *script_string_lib;
} view_t;

struct _ply_boot_splash_plugin
{
  ply_event_loop_t      *loop;
  ply_boot_splash_mode_t mode;
  ply_list_t            *views;

  char *script_filename;
  char *image_dir;

  script_op_t                   *script_main_op;

  uint32_t is_animating : 1;
};

static void detach_from_event_loop (ply_boot_splash_plugin_t *plugin);
static void stop_animation (ply_boot_splash_plugin_t *plugin);
ply_boot_splash_plugin_interface_t *ply_boot_splash_plugin_get_interface (void);

static view_t *
view_new (ply_boot_splash_plugin_t *plugin,
          ply_pixel_display_t      *display)
{
  view_t *view;

  view = calloc (1, sizeof (view_t));
  view->plugin = plugin;
  view->display = display;

  return view;
}

static void
view_free (view_t *view)
{
  free (view);
}

static void
pause_views (ply_boot_splash_plugin_t *plugin)
{
  ply_list_node_t *node;

  node = ply_list_get_first_node (plugin->views);
  while (node != NULL)
    {
      ply_list_node_t *next_node;
      view_t *view;

      view = ply_list_node_get_data (node);
      next_node = ply_list_get_next_node (plugin->views, node);

      ply_pixel_display_pause_updates (view->display);

      node = next_node;
    }
}

static void
unpause_views (ply_boot_splash_plugin_t *plugin)
{
  ply_list_node_t *node;

  node = ply_list_get_first_node (plugin->views);
  while (node != NULL)
    {
      ply_list_node_t *next_node;
      view_t *view;

      view = ply_list_node_get_data (node);
      next_node = ply_list_get_next_node (plugin->views, node);

      ply_pixel_display_unpause_updates (view->display);

      node = next_node;
    }
}

static void
free_views (ply_boot_splash_plugin_t *plugin)
{
  ply_list_node_t *node;

  node = ply_list_get_first_node (plugin->views);

  while (node != NULL)
    {
      ply_list_node_t *next_node;
      view_t *view;

      view = ply_list_node_get_data (node);
      next_node = ply_list_get_next_node (plugin->views, node);

      view_free (view);
      ply_list_remove_node (plugin->views, node);

      node = next_node;
    }

  ply_list_free (plugin->views);
  plugin->views = NULL;
}

static ply_boot_splash_plugin_t *
create_plugin (ply_key_file_t *key_file)
{
  ply_boot_splash_plugin_t *plugin;
  plugin = calloc (1, sizeof (ply_boot_splash_plugin_t));
  plugin->image_dir = ply_key_file_get_value (key_file, "script", "ImageDir");
  plugin->script_filename = ply_key_file_get_value (key_file,
                                                    "script",
                                                    "ScriptFile");
  plugin->views = ply_list_new ();
  return plugin;
}

static void
destroy_plugin (ply_boot_splash_plugin_t *plugin)
{
  if (plugin == NULL)
    return;

  if (plugin->loop != NULL)
    {
      stop_animation (plugin);
      ply_event_loop_stop_watching_for_exit (plugin->loop,
                                             (ply_event_loop_exit_handler_t)
                                             detach_from_event_loop,
                                             plugin);
      detach_from_event_loop (plugin);
    }

  free_views (plugin);
  free (plugin->script_filename);
  free (plugin->image_dir);
  free (plugin);
}

static void
on_timeout (view_t *view)
{
  ply_boot_splash_plugin_t *plugin;
  double sleep_time;

  plugin = view->plugin;

  script_lib_plymouth_on_refresh (view->script_state,
                                  view->script_plymouth_lib);
  script_lib_sprite_refresh (view->script_sprite_lib);

  sleep_time = 1.0 / FRAMES_PER_SECOND;
  ply_event_loop_watch_for_timeout (plugin->loop,
                                    sleep_time,
                                    (ply_event_loop_timeout_handler_t)
                                    on_timeout, view);
}

static void
on_boot_progress (ply_boot_splash_plugin_t *plugin,
                  double                    duration,
                  double                    percent_done)
{
  ply_list_node_t *node;

  node = ply_list_get_first_node (plugin->views);
  while (node != NULL)
    {
      ply_list_node_t *next_node;
      view_t *view;

      view = ply_list_node_get_data (node);
      next_node = ply_list_get_next_node (plugin->views, node);

      script_lib_plymouth_on_boot_progress (view->script_state,
                                            view->script_plymouth_lib,
                                            duration,
                                            percent_done);
      node = next_node;
    }
}

static bool
view_start_animation (view_t *view)
{
  ply_boot_splash_plugin_t *plugin;

  assert (view != NULL);

  plugin = view->plugin;

  view->script_state = script_state_new (view);
  view->script_image_lib = script_lib_image_setup (view->script_state,
                                                   plugin->image_dir);
  view->script_sprite_lib = script_lib_sprite_setup (view->script_state,
                                                     view->display);
  view->script_plymouth_lib = script_lib_plymouth_setup (view->script_state,
                                                         plugin->mode);
  view->script_math_lib = script_lib_math_setup (view->script_state);
  view->script_string_lib = script_lib_string_setup (view->script_state);

  ply_trace ("executing script file");
  script_return_t ret = script_execute (view->script_state,
                                        plugin->script_main_op);
  script_obj_unref (ret.object);
  on_timeout (view);

  return true;
}

static bool
start_animation (ply_boot_splash_plugin_t *plugin)
{
  ply_list_node_t *node;

  assert (plugin != NULL);
  assert (plugin->loop != NULL);

  if (plugin->is_animating)
    return true;

  ply_trace ("parsing script file");
  plugin->script_main_op = script_parse_file (plugin->script_filename);
  node = ply_list_get_first_node (plugin->views);
  while (node != NULL)
    {
      ply_list_node_t *next_node;
      view_t *view;

      view = ply_list_node_get_data (node);
      next_node = ply_list_get_next_node (plugin->views, node);

      view_start_animation (view);

      node = next_node;
    }

  plugin->is_animating = true;
  return true;
}

static void
view_stop_animation (view_t *view)
{
  ply_boot_splash_plugin_t *plugin;

  plugin = view->plugin;
  script_lib_plymouth_on_quit (view->script_state,
                                  view->script_plymouth_lib);
  script_lib_sprite_refresh (view->script_sprite_lib);

  if (plugin->loop != NULL)
    ply_event_loop_stop_watching_for_timeout (plugin->loop,
                                              (ply_event_loop_timeout_handler_t)
                                              on_timeout, view);

  script_state_destroy (view->script_state);
  script_lib_sprite_destroy (view->script_sprite_lib);
  script_lib_image_destroy (view->script_image_lib);
  script_lib_plymouth_destroy (view->script_plymouth_lib);
  script_lib_math_destroy (view->script_math_lib);
  script_lib_string_destroy (view->script_string_lib);
}

static void
stop_animation (ply_boot_splash_plugin_t *plugin)
{
  ply_list_node_t *node;

  assert (plugin != NULL);
  assert (plugin->loop != NULL);

  if (!plugin->is_animating)
    return;
  plugin->is_animating = false;

  node = ply_list_get_first_node (plugin->views);
  while (node != NULL)
    {
      ply_list_node_t *next_node;
      view_t *view;

      view = ply_list_node_get_data (node);
      next_node = ply_list_get_next_node (plugin->views, node);

      view_stop_animation (view);

      node = next_node;
    }

  script_parse_op_free (plugin->script_main_op);
}

static void
on_interrupt (ply_boot_splash_plugin_t *plugin)
{
  ply_event_loop_exit (plugin->loop, 1);
  stop_animation (plugin);
}

static void
detach_from_event_loop (ply_boot_splash_plugin_t *plugin)
{
  plugin->loop = NULL;
}

static void
on_keyboard_input (ply_boot_splash_plugin_t *plugin,
                   const char               *keyboard_input,
                   size_t                    character_size)
{
  char keyboard_string[character_size + 1];
  ply_list_node_t *node;

  memcpy (keyboard_string, keyboard_input, character_size);
  keyboard_string[character_size] = '\0';

  /* FIXME: Not sure what to do here.  We don't want to feed
   * the input once per monitor, I don't think, so we just call
   * it on the first available monitor.
   *
   * I'm not even sure it's useful for scripts to be able to access
   * this, but if it is we probably need to encode view awareness
   * into the script api somehow.
   */
  node = ply_list_get_first_node (plugin->views);

  if (node != NULL)
    {
      view_t *view;
      view = (view_t *) ply_list_node_get_data (node);

      script_lib_plymouth_on_keyboard_input (view->script_state,
                                             view->script_plymouth_lib,
                                             keyboard_string);
    }
}

static void
on_draw (view_t                   *view,
         ply_pixel_buffer_t       *pixel_buffer,
         int                       x,
         int                       y,
         int                       width,
         int                       height)
{
  script_lib_sprite_draw_area (view->script_sprite_lib,
                               pixel_buffer,
                               x, y, width, height);
}

static void
set_keyboard (ply_boot_splash_plugin_t *plugin,
              ply_keyboard_t           *keyboard)
{

  ply_keyboard_add_input_handler (keyboard,
                                  (ply_keyboard_input_handler_t)
                                  on_keyboard_input, plugin);
}

static void
unset_keyboard (ply_boot_splash_plugin_t *plugin,
                ply_keyboard_t           *keyboard)
{
  ply_keyboard_remove_input_handler (keyboard,
                                     (ply_keyboard_input_handler_t)
                                     on_keyboard_input);
}

static void
add_pixel_display (ply_boot_splash_plugin_t *plugin,
                   ply_pixel_display_t      *display)
{
  view_t *view;

  view = view_new (plugin, display);

  ply_pixel_display_set_draw_handler (view->display,
                                      (ply_pixel_display_draw_handler_t)
                                      on_draw, view);

  ply_list_append_data (plugin->views, view);
}

static void
remove_pixel_display (ply_boot_splash_plugin_t *plugin,
                      ply_pixel_display_t      *display)
{
  ply_list_node_t *node;

  node = ply_list_get_first_node (plugin->views);
  while (node != NULL)
    {
      view_t *view;
      ply_list_node_t *next_node;

      view = ply_list_node_get_data (node);
      next_node = ply_list_get_next_node (plugin->views, node);

      if (view->display == display)
        {

          ply_pixel_display_set_draw_handler (view->display, NULL, NULL);
          view_free (view);
          ply_list_remove_node (plugin->views, node);
          return;
        }

      node = next_node;
    }
}

static bool
show_splash_screen (ply_boot_splash_plugin_t *plugin,
                    ply_event_loop_t         *loop,
                    ply_buffer_t             *boot_buffer,
                    ply_boot_splash_mode_t    mode)
{
  assert (plugin != NULL);

  plugin->loop = loop;
  plugin->mode = mode;

  ply_event_loop_watch_for_exit (loop, (ply_event_loop_exit_handler_t)
                                 detach_from_event_loop,
                                 plugin);

  ply_event_loop_watch_signal (plugin->loop,
                               SIGINT,
                               (ply_event_handler_t)
                               on_interrupt, plugin);

  ply_trace ("starting boot animation");
  return start_animation (plugin);
}

static void
update_status (ply_boot_splash_plugin_t *plugin,
               const char               *status)
{
  assert (plugin != NULL);

  ply_list_node_t *node;

  node = ply_list_get_first_node (plugin->views);
  while (node != NULL)
    {
      ply_list_node_t *next_node;
      view_t *view;

      view = ply_list_node_get_data (node);
      next_node = ply_list_get_next_node (plugin->views, node);

      script_lib_plymouth_on_update_status (view->script_state,
                                            view->script_plymouth_lib,
                                            status);
      node = next_node;
    }
}

static void
hide_splash_screen (ply_boot_splash_plugin_t *plugin,
                    ply_event_loop_t         *loop)
{
  assert (plugin != NULL);

  if (plugin->loop != NULL)
    {
      stop_animation (plugin);

      ply_event_loop_stop_watching_for_exit (plugin->loop,
                                             (ply_event_loop_exit_handler_t)
                                             detach_from_event_loop,
                                             plugin);
      detach_from_event_loop (plugin);
    }
}

static void
on_root_mounted (ply_boot_splash_plugin_t *plugin)
{

  ply_list_node_t *node;

  node = ply_list_get_first_node (plugin->views);
  while (node != NULL)
    {
      ply_list_node_t *next_node;
      view_t *view;

      view = ply_list_node_get_data (node);
      next_node = ply_list_get_next_node (plugin->views, node);

      script_lib_plymouth_on_root_mounted (view->script_state,
                                           view->script_plymouth_lib);
      node = next_node;
    }
}

static void
become_idle (ply_boot_splash_plugin_t *plugin,
             ply_trigger_t            *idle_trigger)
{
  ply_trigger_pull (idle_trigger, NULL);
}

static void
display_normal (ply_boot_splash_plugin_t *plugin)
{
  ply_list_node_t *node;

  pause_views (plugin);
  node = ply_list_get_first_node (plugin->views);
  while (node != NULL)
    {
      ply_list_node_t *next_node;
      view_t *view;

      view = ply_list_node_get_data (node);
      next_node = ply_list_get_next_node (plugin->views, node);

      script_lib_plymouth_on_display_normal (view->script_state,
                                             view->script_plymouth_lib);


      node = next_node;
    }
  unpause_views (plugin);
}

static void
display_password (ply_boot_splash_plugin_t *plugin,
                  const char               *prompt,
                  int                       bullets)
{
  ply_list_node_t *node;

  pause_views (plugin);
  node = ply_list_get_first_node (plugin->views);
  while (node != NULL)
    {
      ply_list_node_t *next_node;
      view_t *view;

      view = ply_list_node_get_data (node);
      next_node = ply_list_get_next_node (plugin->views, node);

      script_lib_plymouth_on_display_password (view->script_state,
                                               view->script_plymouth_lib,
                                               prompt,
                                               bullets);

      node = next_node;
    }
  unpause_views (plugin);

}

static void
display_question (ply_boot_splash_plugin_t *plugin,
                  const char               *prompt,
                  const char               *entry_text)
{
  ply_list_node_t *node;

  pause_views (plugin);
  node = ply_list_get_first_node (plugin->views);
  while (node != NULL)
    {
      ply_list_node_t *next_node;
      view_t *view;

      view = ply_list_node_get_data (node);
      next_node = ply_list_get_next_node (plugin->views, node);

      script_lib_plymouth_on_display_question (view->script_state,
                                               view->script_plymouth_lib,
                                               prompt,
                                               entry_text);

      node = next_node;
    }
  unpause_views (plugin);
}

static void
display_message (ply_boot_splash_plugin_t *plugin,
                 const char               *message)
{
  ply_list_node_t *node;

  pause_views (plugin);
  node = ply_list_get_first_node (plugin->views);
  while (node != NULL)
    {
      ply_list_node_t *next_node;
      view_t *view;

      view = ply_list_node_get_data (node);
      next_node = ply_list_get_next_node (plugin->views, node);

      script_lib_plymouth_on_message (view->script_state,
                                      view->script_plymouth_lib,
                                      message);

      node = next_node;
    }
  unpause_views (plugin);
}

ply_boot_splash_plugin_interface_t *
ply_boot_splash_plugin_get_interface (void)
{
  static ply_boot_splash_plugin_interface_t plugin_interface =
  {
    .create_plugin = create_plugin,
    .destroy_plugin = destroy_plugin,
    .set_keyboard = set_keyboard,
    .unset_keyboard = unset_keyboard,
    .add_pixel_display = add_pixel_display,
    .remove_pixel_display = remove_pixel_display,
    .show_splash_screen = show_splash_screen,
    .update_status = update_status,
    .on_boot_progress = on_boot_progress,
    .hide_splash_screen = hide_splash_screen,
    .on_root_mounted = on_root_mounted,
    .become_idle = become_idle,
    .display_normal = display_normal,
    .display_password = display_password,
    .display_question = display_question,
    .display_message = display_message,
  };

  return &plugin_interface;
}

/* vim: set ts=4 sw=4 expandtab autoindent cindent cino={.5s,(0: */
