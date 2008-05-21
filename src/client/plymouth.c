/* rhgb-client.c - updates boot status
 *
 * Copyright (C) 2007 Red Hat, Inc
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License,
 * or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this; see the file COPYING.  If not, write to the Free
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Written by: Ray Strode <rstrode@redhat.com>
 */
#include "config.h"

#include <errno.h>
#include <stdlib.h>

#include "ply-boot-client.h"
#include "ply-event-loop.h"
#include "ply-logger.h"
#include "ply-utils.h"

static void
on_answer (ply_event_loop_t *loop,
           const char       *answer)
{
  write (STDOUT_FILENO, answer, strlen (answer));
  ply_event_loop_exit (loop, 0);
}

static void
on_success (ply_event_loop_t *loop)
{
  ply_event_loop_exit (loop, 0);
}

static void
on_failure (ply_event_loop_t *loop)
{
  ply_event_loop_exit (loop, 1);
}

static void
on_disconnect (ply_event_loop_t *loop)
{
  ply_error ("error: unexpectedly disconnected from boot status daemon");
  ply_event_loop_exit (loop, 2);
}

void
print_usage (void)
{
  ply_log ("plymouth [--ping] [--update=STATUS] [--show-splash] [--details] [--sysinit] [--quit]");
  ply_flush_log ();
}

int
main (int    argc,
      char **argv)
{
  ply_event_loop_t *loop;
  ply_boot_client_t *client;
  bool should_quit, should_ping, should_update, should_sysinit, should_ask_for_password, should_show_splash;
  char *status;
  int exit_code;
  int i;

  exit_code = 0;

  if (argc <= 1)
    {
      print_usage ();
      return 1;
    }

  loop = ply_event_loop_new ();
  client = ply_boot_client_new ();

  should_ping = false;
  should_update = false;
  should_quit = false;
  should_ask_for_password = false;
  should_show_splash = false;
  status = NULL;
  for (i = 1; i < argc; i++)
    {
      if (strstr (argv[i], "--help") != NULL)
        {
          print_usage ();
          return 1;
        }
      if (strstr (argv[i], "--quit") != NULL)
        should_quit = true;
      else if (strstr (argv[i], "--ping") != NULL)
        should_ping = true;
      else if (strstr (argv[i], "--sysinit") != NULL)
        should_sysinit = true;
      else if (strstr (argv[i], "--show-splash") != NULL)
        should_show_splash = true;
      else if (strstr (argv[i], "--ask-for-password") != NULL)
        should_ask_for_password = true;
      else if (strstr (argv[i], "--update") != NULL)
        {
          const char *update_argument;
          should_update = true;
          update_argument = strstr (argv[i], "--update") + strlen ("--update");

          if (update_argument[0] == '\0')
            {
              print_usage ();
              return 1;
            }
          status = strdup (update_argument + 1);
        }
    }

  if (!ply_boot_client_connect (client, 
                                (ply_boot_client_disconnect_handler_t)
                                on_disconnect, loop))
    {
      if (should_ping)
         return 1;

#if 0
      ply_save_errno ();

      if (errno == ECONNREFUSED)
        ply_error ("error: boot status daemon not running "
                   "(use --ping to check ahead of time)");
      else
        ply_error ("could not connect to boot status daemon: %m");
      ply_restore_errno ();
#endif
      return errno;
    }

  ply_boot_client_attach_to_event_loop (client, loop);

  if (should_show_splash)
    ply_boot_client_tell_daemon_to_show_splash (client,
                                               (ply_boot_client_response_handler_t)
                                               on_success,
                                               (ply_boot_client_response_handler_t)
                                               on_failure, loop);
  else if (should_quit)
    ply_boot_client_tell_daemon_to_quit (client,
                                         (ply_boot_client_response_handler_t)
                                         on_success,
                                         (ply_boot_client_response_handler_t)
                                         on_failure, loop);
  else if (should_ping)
    ply_boot_client_ping_daemon (client, 
                                 (ply_boot_client_response_handler_t)
                                 on_success, 
                                 (ply_boot_client_response_handler_t)
                                 on_failure, loop);
  else if (should_update)
    ply_boot_client_update_daemon (client, status,
                                   (ply_boot_client_response_handler_t)
                                   on_success, 
                                   (ply_boot_client_response_handler_t)
                                   on_failure, loop);
  else if (should_ask_for_password)
    ply_boot_client_ask_daemon_for_password (client,
                                   (ply_boot_client_answer_handler_t)
                                   on_answer,
                                   (ply_boot_client_response_handler_t)
                                   on_failure, loop);
  else if (should_sysinit)
    ply_boot_client_tell_daemon_system_is_initialized (client,
                                   (ply_boot_client_response_handler_t)
                                   on_success, 
                                   (ply_boot_client_response_handler_t)
                                   on_failure, loop);
  else
    return 1;

  exit_code = ply_event_loop_run (loop);

  ply_boot_client_free (client);

  return exit_code;
}
/* vim: set ts=4 sw=4 expandtab autoindent cindent cino={.5s,(0: */
