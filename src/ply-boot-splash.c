/* ply-boot-splash.h - APIs for putting up a splash screen
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
#include "ply-boot-splash.h"

#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <wchar.h>

#include "ply-boot-splash-plugin.h"
#include "ply-event-loop.h"
#include "ply-list.h"
#include "ply-logger.h"
#include "ply-utils.h"

struct _ply_boot_splash
{
  ply_event_loop_t *loop;
  ply_module_handle_t *module_handle;
  const ply_boot_splash_plugin_interface_t *plugin_interface;
  ply_boot_splash_plugin_t *plugin;
  ply_window_t *window;
  ply_buffer_t *boot_buffer;

  char *module_name;
  char *status;

  uint32_t is_shown : 1;
};

typedef const ply_boot_splash_plugin_interface_t *
        (* get_plugin_interface_function_t) (void);

ply_boot_splash_t *
ply_boot_splash_new (const char   *module_name,
                     ply_window_t *window,
                     ply_buffer_t *boot_buffer)
{
  ply_boot_splash_t *splash;

  assert (module_name != NULL);

  splash = calloc (1, sizeof (ply_boot_splash_t));
  splash->loop = NULL;
  splash->module_name = strdup (module_name);
  splash->module_handle = NULL;
  splash->is_shown = false;

  splash->window = window;
  splash->boot_buffer = boot_buffer;

  return splash;
}

static bool
ply_boot_splash_load_plugin (ply_boot_splash_t *splash)
{
  assert (splash != NULL);
  assert (splash->module_name != NULL);

  get_plugin_interface_function_t get_boot_splash_plugin_interface;

  splash->module_handle = ply_open_module (splash->module_name);

  if (splash->module_handle == NULL)
    return false;

  get_boot_splash_plugin_interface = (get_plugin_interface_function_t)
      ply_module_look_up_function (splash->module_handle,
                                   "ply_boot_splash_plugin_get_interface");

  if (get_boot_splash_plugin_interface == NULL)
    {
      ply_save_errno ();
      ply_close_module (splash->module_handle);
      splash->module_handle = NULL;
      ply_restore_errno ();
      return false;
    }

  splash->plugin_interface = get_boot_splash_plugin_interface ();

  if (splash->plugin_interface == NULL)
    {
      ply_save_errno ();
      ply_close_module (splash->module_handle);
      splash->module_handle = NULL;
      ply_restore_errno ();
      return false;
    }

  splash->plugin = splash->plugin_interface->create_plugin ();

  assert (splash->plugin != NULL);

  return true;
}

static void
ply_boot_splash_unload_plugin (ply_boot_splash_t *splash)
{
  assert (splash != NULL);
  assert (splash->plugin != NULL);
  assert (splash->plugin_interface != NULL);
  assert (splash->module_handle != NULL);

  splash->plugin_interface->destroy_plugin (splash->plugin);
  splash->plugin = NULL;

  ply_close_module (splash->module_handle);
  splash->plugin_interface = NULL;
  splash->module_handle = NULL;
}

void
ply_boot_splash_free (ply_boot_splash_t *splash)
{
  if (splash == NULL)
    return;

  if (splash->module_handle != NULL)
    ply_boot_splash_unload_plugin (splash);

  free (splash->module_name);
  free (splash);
}

bool
ply_boot_splash_show (ply_boot_splash_t *splash)
{
  assert (splash != NULL);
  assert (splash->module_name != NULL);
  assert (splash->loop != NULL);

  ply_trace ("trying to load %s", splash->module_name);
  if (!ply_boot_splash_load_plugin (splash))
    {
      ply_save_errno ();
      ply_trace ("can't load plugin: %m");
      ply_restore_errno ();
      return false;
    }

  assert (splash->plugin_interface != NULL);
  assert (splash->plugin != NULL);
  assert (splash->plugin_interface->show_splash_screen != NULL);

  splash->plugin_interface->add_window (splash->plugin, splash->window);

  ply_trace ("showing splash screen\n");
  if (!splash->plugin_interface->show_splash_screen (splash->plugin,
                                                     splash->loop,
                                                     splash->boot_buffer))
    {

      ply_save_errno ();
      ply_trace ("can't show splash: %m");
      ply_restore_errno ();
      return false;
    }

  splash->is_shown = true;
  return true;
}

void
ply_boot_splash_update_status (ply_boot_splash_t *splash,
                               const char        *status)
{
  assert (splash != NULL);
  assert (status != NULL);
  assert (splash->plugin_interface != NULL);
  assert (splash->plugin != NULL);
  assert (splash->plugin_interface->update_status != NULL);
  assert (splash->is_shown);

  splash->plugin_interface->update_status (splash->plugin, status);
}

void
ply_boot_splash_update_output (ply_boot_splash_t *splash,
                               const char        *output,
                               size_t             size)
{
  assert (splash != NULL);
  assert (output != NULL);

  if (splash->plugin_interface->on_boot_output != NULL)
    splash->plugin_interface->on_boot_output (splash->plugin, output, size);
}

void
ply_boot_splash_root_mounted (ply_boot_splash_t *splash)
{
  assert (splash != NULL);

  if (splash->plugin_interface->on_root_mounted != NULL)
    splash->plugin_interface->on_root_mounted (splash->plugin);
}

void
ply_boot_splash_ask_for_password (ply_boot_splash_t *splash,
                                  const char        *prompt,
                                  ply_answer_t      *answer)
{

  assert (splash != NULL);
  assert (splash->plugin_interface != NULL);
  assert (splash->plugin != NULL);
  assert (splash->is_shown);

  if (splash->plugin_interface->ask_for_password == NULL)
    {
      ply_answer_unknown (answer);
      return;
    }

  splash->plugin_interface->ask_for_password (splash->plugin,
                                              prompt,
                                              answer);
}

static void
ply_boot_splash_detach_from_event_loop (ply_boot_splash_t *splash)
{
  assert (splash != NULL);
  splash->loop = NULL;
}

void
ply_boot_splash_hide (ply_boot_splash_t *splash)
{
  assert (splash != NULL);
  assert (splash->plugin_interface != NULL);
  assert (splash->plugin != NULL);
  assert (splash->plugin_interface->hide_splash_screen != NULL);

  splash->plugin_interface->hide_splash_screen (splash->plugin,
                                                splash->loop);

  splash->plugin_interface->remove_window (splash->plugin, splash->window);

  splash->is_shown = false;

  if (splash->loop != NULL)
    {
      ply_event_loop_stop_watching_for_exit (splash->loop, (ply_event_loop_exit_handler_t)
                                             ply_boot_splash_detach_from_event_loop,
                                             splash);
    }
}

void
ply_boot_splash_attach_to_event_loop (ply_boot_splash_t *splash,
                                      ply_event_loop_t  *loop)
{
  assert (splash != NULL);
  assert (loop != NULL);
  assert (splash->loop == NULL);

  splash->loop = loop;

  ply_event_loop_watch_for_exit (loop, (ply_event_loop_exit_handler_t) 
                                 ply_boot_splash_detach_from_event_loop,
                                 splash); 
}

#ifdef PLY_BOOT_SPLASH_ENABLE_TEST

#include <stdio.h>

#include "ply-event-loop.h"
#include "ply-boot-splash.h"

typedef struct test_state test_state_t;
struct test_state {
  ply_event_loop_t *loop;
  ply_boot_splash_t *splash;
  ply_window_t *window;
  ply_buffer_t *buffer;
};

static void
on_timeout (ply_boot_splash_t *splash)
{
  ply_boot_splash_update_status (splash, "foo");
  ply_event_loop_watch_for_timeout (splash->loop, 
                                    5.0,
                                   (ply_event_loop_timeout_handler_t)
                                   on_timeout,
                                   splash);
}

static void
on_quit (test_state_t *state)
{
    ply_boot_splash_hide (state->splash);
    ply_event_loop_exit (state->loop, 0);
}

int
main (int    argc,
      char **argv)
{
  int exit_code;
  test_state_t state;
  const char *module_name;

  exit_code = 0;

  state.loop = ply_event_loop_new ();

  if (argc > 1)
    module_name = argv[1];
  else
    module_name = "../splash-plugins/fade-in/.libs/fade-in.so";

  state.window = ply_window_new (argc > 2? atoi (argv[2]) : 0);

  if (!ply_window_open (state.window))
    {
      perror ("could not open terminal");
      return errno;
    }

  if (!ply_window_take_console (state.window))
    {
      perror ("could not switch console to window vt");
      return errno;
    }

  ply_window_attach_to_event_loop (state.window, state.loop);
  ply_window_set_escape_handler (state.window,
                                 (ply_window_escape_handler_t) on_quit, &state);

  state.buffer = ply_buffer_new ();
  state.splash = ply_boot_splash_new (module_name, state.window, state.buffer);
  ply_boot_splash_attach_to_event_loop (state.splash, state.loop);

  if (!ply_boot_splash_show (state.splash))
    {
      perror ("could not show splash screen");
      return errno;
    }

  ply_event_loop_watch_for_timeout (state.loop, 
                                    1.0,
                                   (ply_event_loop_timeout_handler_t)
                                   on_timeout,
                                   state.splash);
  exit_code = ply_event_loop_run (state.loop);
  ply_window_free (state.window);
  ply_boot_splash_free (state.splash);
  ply_buffer_free (state.buffer);

  return exit_code;
}

#endif /* PLY_BOOT_SPLASH_ENABLE_TEST */
/* vim: set ts=4 sw=4 expandtab autoindent cindent cino={.5s,(0: */
