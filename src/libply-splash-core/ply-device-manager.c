/* ply-device-manager.c - device manager
 *
 * Copyright (C) 2013 Red Hat, Inc.
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
 */
#include "config.h"
#include "ply-device-manager.h"

#include <assert.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "ply-logger.h"
#include "ply-event-loop.h"
#include "ply-hashtable.h"
#include "ply-list.h"
#include "ply-utils.h"

struct _ply_device_manager
{
  ply_device_manager_flags_t  flags;
  ply_event_loop_t           *loop;
  ply_hashtable_t            *terminals;
  ply_terminal_t             *local_console_terminal;
  ply_list_t                 *seats;

  ply_seat_added_handler_t    seat_added_handler;
  ply_seat_removed_handler_t  seat_removed_handler;
  void                       *seat_event_handler_data;
};

static void
detach_from_event_loop (ply_device_manager_t *manager)
{
  assert (manager != NULL);

  manager->loop = NULL;
}

static void
attach_to_event_loop (ply_device_manager_t *manager,
                      ply_event_loop_t     *loop)
{
  assert (manager != NULL);
  assert (loop != NULL);
  assert (manager->loop == NULL);

  manager->loop = loop;

  ply_event_loop_watch_for_exit (loop, (ply_event_loop_exit_handler_t)
                                 detach_from_event_loop,
                                 manager);
}

static void
free_seats (ply_device_manager_t *manager)
{
  ply_list_node_t *node;

  ply_trace ("removing seats");
  node = ply_list_get_first_node (manager->seats);
  while (node != NULL)
    {
      ply_seat_t *seat;
      ply_list_node_t *next_node;

      seat = ply_list_node_get_data (node);
      next_node = ply_list_get_next_node (manager->seats, node);

      if (manager->seat_removed_handler != NULL)
        manager->seat_removed_handler (manager->seat_event_handler_data, seat);

      ply_seat_free (seat);
      ply_list_remove_node (manager->seats, node);

      node = next_node;
    }
}

static void
free_terminal (char                 *device,
               ply_terminal_t       *terminal,
               ply_device_manager_t *manager)
{
  ply_hashtable_remove (manager->terminals, device);

  ply_terminal_close (terminal);
  ply_terminal_free (terminal);
}

static void
free_terminals (ply_device_manager_t *manager)
{
  ply_hashtable_foreach (manager->terminals,
                         (ply_hashtable_foreach_func_t *)
                         free_terminal,
                         manager);
}

static ply_terminal_t *
get_terminal (ply_device_manager_t *manager,
              const char           *device_name)
{
  char *full_name = NULL;
  ply_terminal_t *terminal;

  if (strncmp (device_name, "/dev/", strlen ("/dev/")) == 0)
    full_name = strdup (device_name);
  else
    asprintf (&full_name, "/dev/%s", device_name);

  if (strcmp (full_name, "/dev/tty0") == 0 ||
      strcmp (full_name, "/dev/tty") == 0)
    {
      terminal = manager->local_console_terminal;
      goto done;
    }

  terminal = ply_hashtable_lookup (manager->terminals, full_name);

  if (terminal == NULL)
    {
      terminal = ply_terminal_new (full_name);

      ply_hashtable_insert (manager->terminals,
                            (void *) ply_terminal_get_name (terminal),
                            terminal);
    }

done:
  free (full_name);
  return terminal;
}

ply_device_manager_t *
ply_device_manager_new (const char                 *default_tty,
                        ply_device_manager_flags_t  flags)
{
  ply_device_manager_t *manager;

  manager = calloc (1, sizeof (ply_device_manager_t));
  manager->loop = NULL;
  manager->terminals = ply_hashtable_new (ply_hashtable_string_hash, ply_hashtable_string_compare);
  manager->local_console_terminal = ply_terminal_new (default_tty);
  ply_hashtable_insert (manager->terminals,
                        (void *) ply_terminal_get_name (manager->local_console_terminal),
                        manager->local_console_terminal);
  manager->seats = ply_list_new ();
  manager->flags = flags;

  attach_to_event_loop (manager, ply_event_loop_get_default ());

  return manager;
}

void
ply_device_manager_free (ply_device_manager_t *manager)
{
  ply_trace ("freeing device manager");

  if (manager == NULL)
    return;

  free_seats (manager);
  ply_list_free (manager->seats);

  free_terminals (manager);
  ply_hashtable_free (manager->terminals);

  free (manager);
}

static int
add_consoles_from_file (ply_device_manager_t *manager,
                        const char           *path)
{
  int fd;
  char contents[512] = "";
  ssize_t contents_length;
  int num_consoles;
  const char *remaining_file_contents;

  ply_trace ("opening %s", path);
  fd = open (path, O_RDONLY);

  if (fd < 0)
    {
      ply_trace ("couldn't open it: %m");
      return 0;
    }

  ply_trace ("reading file");
  contents_length = read (fd, contents, sizeof (contents) - 1);

  if (contents_length <= 0)
    {
      ply_trace ("couldn't read it: %m");
      close (fd);
      return 0;
    }
  close (fd);

  remaining_file_contents = contents;
  num_consoles = 0;

  while (remaining_file_contents < contents + contents_length)
    {
      char *console;
      size_t console_length;
      const char *console_device;
      ply_terminal_t *terminal;

      /* Advance past any leading whitespace */
      remaining_file_contents += strspn (remaining_file_contents, " \n\t\v");

      if (*remaining_file_contents == '\0')
        {
          /* There's nothing left after the whitespace, we're done */
          break;
        }

      /* Find trailing whitespace and NUL terminate.  If strcspn
       * doesn't find whitespace, it gives us the length of the string
       * until the next NUL byte, which we'll just overwrite with
       * another NUL byte anyway. */
      console_length = strcspn (remaining_file_contents, " \n\t\v");
      console = strndup (remaining_file_contents, console_length);

      terminal = get_terminal (manager, console);
      console_device = ply_terminal_get_name (terminal);

      free (console);

      ply_trace ("console %s found!", console_device);
      num_consoles++;

      /* Move past the parsed console string, and the whitespace we
       * may have found above.  If we found a NUL above and not whitespace,
       * then we're going to jump past the end of the buffer and the loop
       * will terminate
       */
      remaining_file_contents += console_length + 1;
    }

  return num_consoles;
}

static void
create_seat_for_terminal_and_renderer_type (ply_device_manager_t *manager,
                                            const char           *device_path,
                                            ply_terminal_t       *terminal,
                                            ply_renderer_type_t   renderer_type)
{
  ply_seat_t *seat;

  ply_trace ("creating seat for %s (renderer type: %u) (terminal: %s)",
             device_path? : "", renderer_type, terminal? ply_terminal_get_name (terminal): "none");
  seat = ply_seat_new (terminal);

  if (!ply_seat_open (seat, renderer_type, device_path))
    {
      ply_trace ("could not create seat");
      ply_seat_free (seat);
      return;
    }

  ply_list_append_data (manager->seats, seat);

  if (manager->seat_added_handler != NULL)
    manager->seat_added_handler (manager->seat_event_handler_data, seat);
}

static void
create_seat_for_terminal (const char           *device_path,
                          ply_terminal_t       *terminal,
                          ply_device_manager_t *manager)
{
  create_seat_for_terminal_and_renderer_type (manager,
                                              device_path,
                                              terminal,
                                              PLY_RENDERER_TYPE_NONE);
}
static void
create_seats_from_terminals (ply_device_manager_t *manager)
{
  int num_consoles;

  ply_trace ("checking for consoles");

  if (manager->flags & PLY_DEVICE_MANAGER_FLAGS_IGNORE_SERIAL_CONSOLES)
    {
      num_consoles = 0;
      ply_trace ("ignoring all consoles but default console because explicitly told to.");
    }
  else
    {
      num_consoles = add_consoles_from_file (manager, "/sys/class/tty/console/active");

      if (num_consoles == 0)
        ply_trace ("ignoring all consoles but default console because /sys/class/tty/console/active could not be read");
    }

  if (num_consoles > 1)
    {
      ply_hashtable_foreach (manager->terminals,
                             (ply_hashtable_foreach_func_t *)
                             create_seat_for_terminal,
                             manager);
    }
  else
    {
      create_seat_for_terminal_and_renderer_type (manager,
                                                  ply_terminal_get_name (manager->local_console_terminal),
                                                  manager->local_console_terminal,
                                                  PLY_RENDERER_TYPE_AUTO);
    }
}

void
ply_device_manager_watch_seats (ply_device_manager_t       *manager,
                                ply_seat_added_handler_t    seat_added_handler,
                                ply_seat_removed_handler_t  seat_removed_handler,
                                void                       *data)
{
  manager->seat_added_handler = seat_added_handler;
  manager->seat_removed_handler = seat_removed_handler;
  manager->seat_event_handler_data = data;

  create_seats_from_terminals (manager);
}

bool
ply_device_manager_has_open_seats (ply_device_manager_t *manager)
{
  ply_list_node_t *node;

  node = ply_list_get_first_node (manager->seats);
  while (node != NULL)
    {
      ply_seat_t *seat;
      ply_list_node_t *next_node;

      seat = ply_list_node_get_data (node);
      next_node = ply_list_get_next_node (manager->seats, node);

      if (ply_seat_is_open (seat))
        return true;

      node = next_node;
    }

  return false;
}

ply_list_t *
ply_device_manager_get_seats (ply_device_manager_t *manager)
{
  return manager->seats;
}

ply_terminal_t *
ply_device_manager_get_default_terminal (ply_device_manager_t *manager)
{
  return manager->local_console_terminal;
}

void
ply_device_manager_activate_renderers (ply_device_manager_t *manager)
{
  ply_list_node_t *node;

  ply_trace ("activating renderers");
  node = ply_list_get_first_node (manager->seats);
  while (node != NULL)
    {
      ply_seat_t *seat;
      ply_list_node_t *next_node;

      seat = ply_list_node_get_data (node);
      next_node = ply_list_get_next_node (manager->seats, node);

      ply_seat_activate_renderer (seat);

      node = next_node;
    }
}

void
ply_device_manager_deactivate_renderers (ply_device_manager_t *manager)
{
  ply_list_node_t *node;

  ply_trace ("deactivating renderers");
  node = ply_list_get_first_node (manager->seats);
  while (node != NULL)
    {
      ply_seat_t *seat;
      ply_list_node_t *next_node;

      seat = ply_list_node_get_data (node);
      next_node = ply_list_get_next_node (manager->seats, node);

      ply_seat_deactivate_renderer (seat);

      node = next_node;
    }
}

void
ply_device_manager_activate_keyboards (ply_device_manager_t *manager)
{
  ply_list_node_t *node;

  ply_trace ("activating keyboards");
  node = ply_list_get_first_node (manager->seats);
  while (node != NULL)
    {
      ply_seat_t *seat;
      ply_list_node_t *next_node;

      seat = ply_list_node_get_data (node);
      next_node = ply_list_get_next_node (manager->seats, node);

      ply_seat_activate_keyboard (seat);

      node = next_node;
    }
}

void
ply_device_manager_deactivate_keyboards (ply_device_manager_t *manager)
{
  ply_list_node_t *node;

  ply_trace ("deactivating keyboards");
  node = ply_list_get_first_node (manager->seats);
  while (node != NULL)
    {
      ply_seat_t *seat;
      ply_list_node_t *next_node;

      seat = ply_list_node_get_data (node);
      next_node = ply_list_get_next_node (manager->seats, node);

      ply_seat_deactivate_keyboard (seat);

      node = next_node;
    }
}
