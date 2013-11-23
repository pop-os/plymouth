/* details.c - boot splash plugin
 *
 * Copyright (C) 2008 Red Hat, Inc.
 * Copyright (C) 2012 Pali Rohár <pali.rohar@gmail.com>
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
#include <wchar.h>
#include <values.h>

#include "ply-boot-splash-plugin.h"
#include "ply-buffer.h"
#include "ply-event-loop.h"
#include "ply-key-file.h"
#include "ply-list.h"
#include "ply-logger.h"
#include "ply-text-display.h"
#include "ply-trigger.h"
#include "ply-utils.h"

#include <linux/kd.h>

#define CLEAR_LINE_SEQUENCE "\033[2K\r"

typedef enum {
   PLY_BOOT_SPLASH_DISPLAY_NORMAL,
   PLY_BOOT_SPLASH_DISPLAY_QUESTION_ENTRY,
   PLY_BOOT_SPLASH_DISPLAY_PASSWORD_ENTRY
} ply_boot_splash_display_type_t;

typedef struct
{
  ply_boot_splash_plugin_t *plugin;
  ply_text_display_t *display;
} view_t;

typedef struct
{
  char *operation_id;
  char *name;
} operation_t;

ply_boot_splash_plugin_interface_t *ply_boot_splash_plugin_get_interface (void);
static void detach_from_event_loop (ply_boot_splash_plugin_t *plugin);

struct _ply_boot_splash_plugin
{
  ply_event_loop_t *loop;
  ply_boot_splash_mode_t mode;
  ply_list_t *views;
  ply_boot_splash_display_type_t state;
  ply_list_t *messages;
  ply_list_t *operations;
};

static view_t *
view_new (ply_boot_splash_plugin_t *plugin,
          ply_text_display_t      *display)
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

static void
free_messages (ply_boot_splash_plugin_t *plugin)
{
  ply_list_node_t *node;

  node = ply_list_get_first_node (plugin->messages);

  while (node != NULL)
    {
      ply_list_node_t *next_node;
      char *message;

      message = ply_list_node_get_data (node);
      next_node = ply_list_get_next_node (plugin->messages, node);

      free (message);
      ply_list_remove_node (plugin->messages, node);

      node = next_node;
    }

  ply_list_free (plugin->messages);
  plugin->messages = NULL;
}

static operation_t *
operation_new (ply_boot_splash_plugin_t *plugin,
               const char               *operation_id,
               const char               *name)
{
  operation_t *operation;
  operation = malloc (sizeof(operation_t));
  operation->operation_id = strdup (operation_id);
  operation->name = strdup (name);
  return operation;
}

static void
operation_free (operation_t *operation)
{
  free (operation->operation_id);
  free (operation->name);
  free (operation);
}

static void
free_operations (ply_boot_splash_plugin_t *plugin)
{
  ply_list_node_t *node;

  node = ply_list_get_first_node (plugin->operations);

  while (node != NULL)
    {
      ply_list_node_t *next_node;
      operation_t     *operation;

      operation = ply_list_node_get_data (node);
      next_node = ply_list_get_next_node (plugin->operations, node);

      operation_free (operation);
      ply_list_remove_node (plugin->operations, node);

      node = next_node;
    }

  ply_list_free (plugin->operations);
  plugin->operations = NULL;
}

static const char *
get_operation_name (ply_boot_splash_plugin_t *plugin, const char *id)
{
  ply_list_node_t *node;

  node = ply_list_get_first_node (plugin->operations);

  while (node != NULL)
    {
      operation_t     *operation;

      operation = ply_list_node_get_data (node);
      if (strcmp(operation->operation_id, id) == 0)
        {
          return operation->name;
        }

      node = ply_list_get_next_node (plugin->operations, node);

    }

  return NULL;
}

static ply_boot_splash_plugin_t *
create_plugin (ply_key_file_t *key_file)
{
  ply_boot_splash_plugin_t *plugin;

  ply_trace ("creating plugin");

  plugin = calloc (1, sizeof (ply_boot_splash_plugin_t));
  plugin->views = ply_list_new ();
  plugin->state = PLY_BOOT_SPLASH_DISPLAY_NORMAL;
  plugin->operations = ply_list_new ();
  plugin->messages = ply_list_new ();
  return plugin;
}

static void
destroy_plugin (ply_boot_splash_plugin_t *plugin)
{
  ply_trace ("destroying plugin");

  if (plugin == NULL)
    return;

  if (plugin->loop != NULL)
    {
      ply_event_loop_stop_watching_for_exit (plugin->loop, (ply_event_loop_exit_handler_t)
                                             detach_from_event_loop,
                                             plugin);
      detach_from_event_loop (plugin);
    }

  free_operations (plugin);
  free_messages (plugin);
  free_views (plugin);

  free (plugin);
}

static void
detach_from_event_loop (ply_boot_splash_plugin_t *plugin)
{
  plugin->loop = NULL;

  ply_trace ("detaching from event loop");
}

static void
view_write (view_t     *view,
            const char *text,
            size_t      number_of_bytes)
{
  ply_terminal_t *terminal;

  terminal = ply_text_display_get_terminal (view->display);
  ply_terminal_write (terminal, "%.*s", (int) number_of_bytes, text);
}

static void
write_on_views (ply_boot_splash_plugin_t *plugin,
                const char               *text,
                size_t                    number_of_bytes)
{
  ply_list_node_t *node;

  if (number_of_bytes == 0)
    return;

  node = ply_list_get_first_node (plugin->views);

  while (node != NULL)
    {
      ply_list_node_t *next_node;
      view_t *view;

      view = ply_list_node_get_data (node);
      next_node = ply_list_get_next_node (plugin->views, node);

      view_write (view, text, number_of_bytes);

      node = next_node;
    }

}

static void
add_text_display (ply_boot_splash_plugin_t *plugin,
                  ply_text_display_t       *display)
{
  view_t *view;
  ply_terminal_t *terminal;

  view = view_new (plugin, display);

  terminal = ply_text_display_get_terminal (view->display);
  if (ply_terminal_open (terminal))
    ply_terminal_activate_vt (terminal);

  ply_list_append_data (plugin->views, view);
}

static void
remove_text_display (ply_boot_splash_plugin_t *plugin,
                     ply_text_display_t       *display)
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
  size_t size;

  assert (plugin != NULL);

  plugin->loop = loop;
  plugin->mode = mode;

  ply_event_loop_watch_for_exit (loop, (ply_event_loop_exit_handler_t)
                                 detach_from_event_loop,
                                 plugin);

  if (boot_buffer)
    {
      size = ply_buffer_get_size (boot_buffer);

      write_on_views (plugin, ply_buffer_get_bytes (boot_buffer), size);
    }

  return true;
}

static void
register_operation (ply_boot_splash_plugin_t *plugin,
                    const char               *operation_id,
                    const char               *name)
{
  operation_t *operation;
  assert (plugin != NULL);
  assert (operation_id != NULL);
  assert (name != NULL);

  ply_trace ("register operation");

  operation = operation_new (plugin, operation_id, name);
  ply_list_append_data (plugin->operations, operation);
}

static void
unregister_operation (ply_boot_splash_plugin_t *plugin,
                      const char               *operation_id)
{
  assert (plugin != NULL);
  assert (operation_id != NULL);

  ply_trace ("unregister operation");

  ply_list_node_t *node;

  node = ply_list_get_first_node (plugin->operations);

  while (node != NULL)
    {
      operation_t *operation;

      operation = ply_list_node_get_data (node);
      if (strcmp(operation->operation_id, operation_id) == 0)
        {
          ply_list_remove_node(plugin->operations, node);
          return;
        }

      node = ply_list_get_next_node (plugin->operations, node);

    }
}

static void
update_status (ply_boot_splash_plugin_t *plugin,
               const char               *status,
               const char               *operation_id)
{
  assert (plugin != NULL);
  assert (status != NULL);

  ply_trace ("status update");

  if (operation_id != NULL)
    {
      const char *operation_name = get_operation_name (plugin, operation_id);
      if (operation_name != NULL)
        {
          char *message;
          asprintf (&message, "%s... [%s]\r\n", operation_name, status);
          if (message != NULL)
            {
              write_on_views (plugin, message, strlen (message));
              free(message);
            }
        }
    }
}

static void
on_boot_output (ply_boot_splash_plugin_t *plugin,
                const char               *output,
                size_t                    size)
{
  ply_trace ("writing '%s' to all views (%d bytes)",
             output, (int) size);
  write_on_views (plugin, output, size);
}

static void
hide_splash_screen (ply_boot_splash_plugin_t *plugin,
                    ply_event_loop_t         *loop)
{
  assert (plugin != NULL);

  ply_trace ("hiding splash screen");

  ply_event_loop_stop_watching_for_exit (plugin->loop,
                                         (ply_event_loop_exit_handler_t)
                                         detach_from_event_loop,
                                         plugin);
  detach_from_event_loop (plugin);
}

static void
display_normal (ply_boot_splash_plugin_t *plugin)
{
  ply_list_node_t *node;

  if (plugin->state != PLY_BOOT_SPLASH_DISPLAY_NORMAL)
    write_on_views (plugin, "\r\n", strlen ("\r\n"));

  plugin->state = PLY_BOOT_SPLASH_DISPLAY_NORMAL;

  node = ply_list_get_first_node (plugin->messages);
  while (node != NULL)
    {
      const char *message;
      ply_list_node_t *next_node;

      message = ply_list_node_get_data (node);
      next_node = ply_list_get_next_node (plugin->messages, node);

      write_on_views (plugin, message, strlen (message));
      write_on_views (plugin, "\r\n", strlen ("\r\n"));

      ply_list_remove_node (plugin->messages, node);
      node = next_node;
    }
}

static void
display_password (ply_boot_splash_plugin_t *plugin,
                  const char               *prompt,
                  int                       bullets)
{
  int i;
  if (plugin->state != PLY_BOOT_SPLASH_DISPLAY_PASSWORD_ENTRY)
    write_on_views (plugin, "\r\n", strlen ("\r\n"));
  else
    write_on_views (plugin,
                    CLEAR_LINE_SEQUENCE,
                    strlen (CLEAR_LINE_SEQUENCE));
  plugin->state = PLY_BOOT_SPLASH_DISPLAY_PASSWORD_ENTRY;

  if (prompt)
    write_on_views (plugin,
                    prompt,
                    strlen (prompt));
  else
    write_on_views (plugin,
                    "Password",
                    strlen ("Password"));

  write_on_views (plugin, ":", strlen (":"));

  for (i = 0; i < bullets; i++)
    write_on_views (plugin, "*", strlen ("*"));
}

static void
display_question (ply_boot_splash_plugin_t *plugin,
                  const char               *prompt,
                  const char               *entry_text)
{
  if (plugin->state != PLY_BOOT_SPLASH_DISPLAY_QUESTION_ENTRY)
    write_on_views (plugin, "\r\n", strlen ("\r\n"));
  else
    write_on_views (plugin,
                    CLEAR_LINE_SEQUENCE,
                    strlen (CLEAR_LINE_SEQUENCE));

  plugin->state = PLY_BOOT_SPLASH_DISPLAY_QUESTION_ENTRY;
  if (prompt)
    write_on_views (plugin, prompt, strlen (prompt));

  write_on_views (plugin, ":", strlen (":"));
  write_on_views (plugin, entry_text, strlen (entry_text));
}

static void
display_message (ply_boot_splash_plugin_t *plugin,
                 const char               *message)
{
  if (plugin->state == PLY_BOOT_SPLASH_DISPLAY_NORMAL)
    {
      write_on_views (plugin, message, strlen (message));
      write_on_views (plugin, "\r\n", strlen ("\r\n"));
    }
  else
    ply_list_append_data (plugin->messages, strdup (message));
}

ply_boot_splash_plugin_interface_t *
ply_boot_splash_plugin_get_interface (void)
{
  static ply_boot_splash_plugin_interface_t plugin_interface =
    {
      .create_plugin = create_plugin,
      .destroy_plugin = destroy_plugin,
      .add_text_display = add_text_display,
      .remove_text_display = remove_text_display,
      .show_splash_screen = show_splash_screen,
      .register_operation = register_operation,
      .unregister_operation = unregister_operation,
      .update_status = update_status,
      .on_boot_output = on_boot_output,
      .hide_splash_screen = hide_splash_screen,
      .display_normal = display_normal,
      .display_password = display_password,
      .display_question = display_question,      
      .display_message = display_message,
    };

  return &plugin_interface;
}

/* vim: set ts=4 sw=4 expandtab autoindent cindent cino={.5s,(0: */
