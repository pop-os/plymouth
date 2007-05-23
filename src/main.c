/* main.c - boot messages monitor
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

#include <stdlib.h>
#include <stdio.h>
#include <sysexits.h>

#include "ply-event-loop.h"
#include "ply-logger.h"
#include "ply-terminal-session.h"
#include "ply-utils.h"

typedef struct
{
  ply_event_loop_t *loop;
  ply_terminal_session_t *session;
} state_t;

static void
on_session_finished (state_t *state)
{
  ply_log ("Session finished...exiting logger\n");
  ply_flush_log ();
  ply_event_loop_exit (state->loop, 0);
} 

static ply_terminal_session_t *
spawn_session (state_t  *state, 
               char    **argv)
{
  ply_terminal_session_t *session;
  ply_terminal_session_flags_t flags;

  flags = 0;
  flags |= PLY_TERMINAL_SESSION_FLAGS_RUN_IN_PARENT;
  flags |= PLY_TERMINAL_SESSION_FLAGS_LOOK_IN_PATH;
  flags |= PLY_TERMINAL_SESSION_FLAGS_REDIRECT_CONSOLE;

  session = ply_terminal_session_new ((const char * const *) argv);
  ply_terminal_session_attach_to_event_loop (session, state->loop);

  if (!ply_terminal_session_run (session, flags,
                                 (ply_terminal_session_done_handler_t)
                                 on_session_finished, state))
    {
      ply_save_errno ();
      ply_terminal_session_free (session);
      ply_restore_errno ();
      return NULL;
    }

  return session;
}

int
main (int    argc,
      char **argv)
{
  state_t state;
  int exit_code;

  if (argc <= 1)
    {
      ply_error ("%s other-command [other-command-args]", argv[0]);
      return EX_USAGE;
    }

  state.loop = ply_event_loop_new ();
  state.session = spawn_session (&state, argv + 1);

  if (state.session == NULL)
    {
      ply_error ("could not run '%s': %m", argv[1]);
      return EX_UNAVAILABLE;
    }

  ply_terminal_session_start_logging (state.session);
  exit_code = ply_event_loop_run (state.loop);
  ply_terminal_session_stop_logging (state.session);

  ply_terminal_session_free (state.session);
  ply_event_loop_free (state.loop);

  return exit_code;
}
/* vim: set ts=4 sw=4 expandtab autoindent cindent cino={.5s,(0: */
