/* ply-terminal-session.c - api for spawning a program in pseudo-terminal
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
#include "ply-terminal-session.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "ply-event-loop.h"
#include "ply-logger.h"
#include "ply-terminal.h"
#include "ply-utils.h"

struct _ply_terminal_session
{
  ply_terminal_t *terminal;
  ply_logger_t *logger;
  ply_event_loop_t *loop;
  char **argv;

  ply_terminal_session_done_handler_t  done_handler;
  void                                *done_handler_user_data;

  uint32_t is_running : 1;
};

static bool ply_terminal_session_open_console (ply_terminal_session_t *session);
static bool ply_terminal_session_execute (ply_terminal_session_t *session,
                                          bool                    look_in_path);

static bool
ply_terminal_session_open_console (ply_terminal_session_t *session)
{
  int fd;
  const char *terminal_name;

  terminal_name = ply_terminal_get_device_name (session->terminal);

  fd = open (terminal_name, O_RDONLY); 

  if (fd < 0)
    return false;

  assert (fd == STDIN_FILENO);
  assert (ttyname (fd) != NULL);
  assert (strcmp (ttyname (fd), terminal_name) == 0);

  fd = open (terminal_name, O_WRONLY); 

  if (fd < 0)
    return false;

  assert (fd == STDOUT_FILENO);
  assert (ttyname (fd) != NULL);
  assert (strcmp (ttyname (fd), terminal_name) == 0);

  fd = open (terminal_name, O_WRONLY); 

  if (fd < 0)
    return false;

  assert (fd == STDERR_FILENO);
  assert (ttyname (fd) != NULL);
  assert (strcmp (ttyname (fd), terminal_name) == 0);

  return true;
}

static bool
ply_terminal_session_execute (ply_terminal_session_t *session,
                              bool                    look_in_path)
{
  ply_close_all_fds ();

  if (!ply_terminal_session_open_console (session))
    return false;

  if (look_in_path)
    execvp (session->argv[0], session->argv);
  else
    execv (session->argv[0], session->argv);

  return false;
}

ply_terminal_session_t *
ply_terminal_session_new (const char * const *argv)
                          
{
  ply_terminal_session_t *session;

  assert (argv != NULL);
  assert (argv[0] != NULL);

  session = calloc (1, sizeof (ply_terminal_session_t));
  session->argv = ply_copy_string_array (argv);
  session->terminal = ply_terminal_new ();
  session->logger = ply_logger_new ();
  session->is_running = false;

  return session;
}

void
ply_terminal_session_free (ply_terminal_session_t *session)
{
  if (session == NULL)
    return;

  ply_terminal_session_stop_logging (session);
  ply_logger_free (session->logger);

  ply_free_string_array (session->argv);
  ply_terminal_free (session->terminal);
  free (session);
}

static void
ply_terminal_session_detach_from_event_loop (ply_terminal_session_t *session)
{
  assert (session != NULL);
  session->loop = NULL;
}

void 
ply_terminal_session_attach_to_event_loop (ply_terminal_session_t *session,
                                           ply_event_loop_t       *loop)
{
  assert (session != NULL);
  assert (loop != NULL);
  assert (session->loop == NULL);

  session->loop = loop;

  ply_event_loop_watch_for_exit (loop, (ply_event_loop_exit_handler_t) 
                                 ply_terminal_session_detach_from_event_loop,
                                 session); 
}

bool 
ply_terminal_session_run (ply_terminal_session_t              *session,
                          ply_terminal_session_flags_t         flags,
                          ply_terminal_session_done_handler_t  done_handler,
                          void                                *user_data)
{
  int pid;
  bool run_in_parent, look_in_path;

  assert (session != NULL);
  assert (session->loop != NULL);
  assert (!session->is_running);
  assert (session->done_handler == NULL);

  run_in_parent = (flags & PLY_TERMINAL_SESSION_FLAGS_RUN_IN_PARENT) != 0;
  look_in_path = (flags & PLY_TERMINAL_SESSION_FLAGS_LOOK_IN_PATH) != 0;

  if (!ply_terminal_create_device (session->terminal))
    return false;

  pid = fork ();

  if (pid < 0)
    return false;

  if (((pid == 0) && run_in_parent) ||
      ((pid != 0) && !run_in_parent))
    {
      session->is_running = true;
      session->done_handler = done_handler;
      session->done_handler_user_data = user_data;

      return true;
    }

  ply_terminal_session_execute (session, look_in_path);

  _exit (errno);
  return false;
}

int
ply_terminal_session_get_fd (ply_terminal_session_t *session)
{
  assert (session != NULL);

  return ply_terminal_get_fd (session->terminal);
}

static void
ply_terminal_session_on_new_data (ply_terminal_session_t *session,
                                  int                     session_fd)
{
  char buffer[4096];
  ssize_t bytes_read;

  assert (session != NULL);
  assert (session_fd >= 0);

  bytes_read = read (session_fd, buffer, sizeof (buffer));

  if (bytes_read > 0)
    {
      int i;
      for (i = 0; i < bytes_read; i++)
        buffer[i] = (char) toupper ((uint8_t) buffer[i]);
      ply_logger_inject_bytes (session->logger, buffer, bytes_read);
    }

  ply_logger_flush (session->logger);
}

static void
ply_terminal_session_on_hangup (ply_terminal_session_t *session)
{
  assert (session != NULL);

  ply_logger_flush (session->logger);

  session->is_running = false;

  if (session->done_handler != NULL)
    session->done_handler (session->done_handler_user_data, session);
}

void 
ply_terminal_session_start_logging (ply_terminal_session_t *session)
{
  int session_fd;

  assert (session != NULL);
  assert (session->logger != NULL);

  if (!ply_logger_is_logging (session->logger))
    ply_logger_toggle_logging (session->logger);

  session_fd = ply_terminal_session_get_fd (session);

  assert (session_fd >= 0);

  ply_event_loop_watch_fd (session->loop, session_fd,
                           (ply_event_handler_t)
                           ply_terminal_session_on_new_data, 
                           (ply_event_handler_t)
                           ply_terminal_session_on_hangup, session);

  ply_logger_set_output_fd (session->logger, open ("/dev/tty1", O_WRONLY));
  ply_logger_flush (session->logger);
}

void
ply_terminal_session_stop_logging (ply_terminal_session_t *session)
{
  assert (session != NULL);
  assert (session->logger != NULL);

  if (ply_logger_is_logging (session->logger))
    ply_logger_toggle_logging (session->logger);
}

#ifdef PLY_TERMINAL_SESSION_ENABLE_TEST

#include <stdio.h>

#include "ply-event-loop.h"
#include "ply-terminal-session.h"

static void
on_finished (ply_event_loop_t *loop)
{
}

int
main (int    argc,
      char **argv)
{
  ply_event_loop_t *loop;
  ply_terminal_session_t *session;
  int exit_code;
  ply_terminal_session_flags_t flags;

  exit_code = 0;

  loop = ply_event_loop_new ();

  session = ply_terminal_session_new ((const char * const *) (argv + 1));

  flags = PLY_TERMINAL_SESSION_FLAGS_RUN_IN_PARENT;
  flags |= PLY_TERMINAL_SESSION_FLAGS_LOOK_IN_PATH;

  ply_terminal_session_attach_to_event_loop (session, loop);

  if (!ply_terminal_session_run (session, flags, 
                                 (ply_terminal_session_done_handler_t)
                                 on_finished, loop))
    {
      perror ("could not start terminal session");
      return errno;
    }

  ply_terminal_session_start_logging (session);
  exit_code = ply_event_loop_run (loop);
  ply_terminal_session_stop_logging (session);

  ply_terminal_session_free (session);

  return exit_code;
}

#endif /* PLY_TERMINAL_SESSION_ENABLE_TEST */
/* vim: set ts=4 sw=4 expandtab autoindent cindent cino={.5s,(0: */
