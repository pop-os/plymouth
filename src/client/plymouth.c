/* plymouth.c - updates boot status
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
#include <stdio.h>

#include "ply-boot-client.h"
#include "ply-command-parser.h"
#include "ply-event-loop.h"
#include "ply-logger.h"
#include "ply-utils.h"

typedef struct
{
  ply_event_loop_t     *loop;
  ply_boot_client_t    *client;
  ply_command_parser_t *command_parser;
} state_t;

static void
on_answer (ply_event_loop_t *loop,
           const char       *answer)
{
  write (STDOUT_FILENO, answer, strlen (answer));
  ply_event_loop_exit (loop, 0);
}

static void
on_success (state_t *state)
{
  ply_event_loop_exit (state->loop, 0);
}

static void
on_failure (state_t *state)
{
  ply_event_loop_exit (state->loop, 1);
}

static void
on_disconnect (state_t *state)
{
  ply_error ("error: unexpectedly disconnected from boot status daemon");
  ply_event_loop_exit (state->loop, 2);
}

int
main (int    argc,
      char **argv)
{
  state_t state = { 0 };
  bool should_help, should_quit, should_ping, should_sysinit, should_ask_for_password, should_show_splash, should_hide_splash;
  char *status, *chroot_dir;
  int exit_code;

  exit_code = 0;

  state.loop = ply_event_loop_new ();
  state.client = ply_boot_client_new ();
  state.command_parser = ply_command_parser_new ("plymouth", "Boot splash control client");

  ply_command_parser_add_options (state.command_parser,
                                  "help", "This help message", PLY_COMMAND_OPTION_TYPE_FLAG,
                                  "newroot", "Tell boot daemon that new root filesystem is mounted", PLY_COMMAND_OPTION_TYPE_STRING,
                                  "quit", "Tell boot daemon to quit", PLY_COMMAND_OPTION_TYPE_FLAG,
                                  "ping", "Check of boot daemon is running", PLY_COMMAND_OPTION_TYPE_FLAG,
                                  "sysinit", "Tell boot daemon root filesystem is mounted read-write", PLY_COMMAND_OPTION_TYPE_FLAG,
                                  "show-splash", "Show splash screen", PLY_COMMAND_OPTION_TYPE_FLAG,
                                  "hide-splash", "Hide splash screen", PLY_COMMAND_OPTION_TYPE_FLAG,
                                  "ask-for-password", "Ask user for password", PLY_COMMAND_OPTION_TYPE_FLAG,
                                  "update", "Tell boot daemon an update about boot progress", PLY_COMMAND_OPTION_TYPE_STRING,
                                  NULL);

  if (!ply_command_parser_parse_arguments (state.command_parser, state.loop, argv, argc))
    {
      char *help_string;

      help_string = ply_command_parser_get_help_string (state.command_parser);

      ply_error ("%s", help_string);

      free (help_string);
      return 1;
    }

  ply_command_parser_get_options (state.command_parser,
                                  "help", &should_help,
                                  "newroot", &chroot_dir,
                                  "quit", &should_quit,
                                  "ping", &should_ping,
                                  "sysinit", &should_sysinit,
                                  "show-splash", &should_show_splash,
                                  "hide-splash", &should_hide_splash,
                                  "ask-for-password", &should_ask_for_password,
                                  "update", &status,
                                  NULL);

  if (should_help || argc < 2)
    {
      char *help_string;

      help_string = ply_command_parser_get_help_string (state.command_parser);

      if (argc < 2)
        fprintf (stderr, "%s", help_string);
      else
        printf ("%s", help_string);

      free (help_string);
      return 0;
    }

  if (!ply_boot_client_connect (state.client,
                                (ply_boot_client_disconnect_handler_t)
                                on_disconnect, &state))
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

  ply_boot_client_attach_to_event_loop (state.client, state.loop);

  if (should_show_splash)
    ply_boot_client_tell_daemon_to_show_splash (state.client,
                                               (ply_boot_client_response_handler_t)
                                               on_success,
                                               (ply_boot_client_response_handler_t)
                                               on_failure, &state);
  else if (should_hide_splash)
    ply_boot_client_tell_daemon_to_hide_splash (state.client,
                                               (ply_boot_client_response_handler_t)
                                               on_success,
                                               (ply_boot_client_response_handler_t)
                                               on_failure, &state);
  else if (should_quit)
    ply_boot_client_tell_daemon_to_quit (state.client,
                                         (ply_boot_client_response_handler_t)
                                         on_success,
                                         (ply_boot_client_response_handler_t)
                                         on_failure, &state);
  else if (should_ping)
    ply_boot_client_ping_daemon (state.client,
                                 (ply_boot_client_response_handler_t)
                                 on_success, 
                                 (ply_boot_client_response_handler_t)
                                 on_failure, &state);
  else if (status != NULL)
    ply_boot_client_update_daemon (state.client, status,
                                   (ply_boot_client_response_handler_t)
                                   on_success, 
                                   (ply_boot_client_response_handler_t)
                                   on_failure, &state);
  else if (should_ask_for_password)
    ply_boot_client_ask_daemon_for_password (state.client,
                                   (ply_boot_client_answer_handler_t)
                                   on_answer,
                                   (ply_boot_client_response_handler_t)
                                   on_failure, &state);
  else if (should_sysinit)
    ply_boot_client_tell_daemon_system_is_initialized (state.client,
                                   (ply_boot_client_response_handler_t)
                                   on_success, 
                                   (ply_boot_client_response_handler_t)
                                   on_failure, &state);
  else if (chroot_dir)
    ply_boot_client_tell_daemon_to_change_root (state.client, chroot_dir,
                                   (ply_boot_client_response_handler_t)
                                   on_success,
                                   (ply_boot_client_response_handler_t)
                                   on_failure, &state);
  else
    return 1;

  exit_code = ply_event_loop_run (state.loop);

  ply_boot_client_free (state.client);

  return exit_code;
}
/* vim: set ts=4 sw=4 expandtab autoindent cindent cino={.5s,(0: */
