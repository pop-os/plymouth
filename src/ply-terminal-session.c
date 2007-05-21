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

#include "ply-terminal.h"
#include "ply-utils.h"

struct _ply_terminal_session
{
  ply_terminal_t *terminal;
  char **argv;
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

  session = calloc (1, sizeof (ply_terminal_session_t));
  session->argv = ply_copy_string_array (argv);
  session->terminal = ply_terminal_new ();

  return session;
}

void
ply_terminal_session_free (ply_terminal_session_t *session)
{
  if (session == NULL)
    return;

  ply_free_string_array (session->argv);
  ply_terminal_free (session->terminal);
  free (session);
}

bool 
ply_terminal_session_run (ply_terminal_session_t *session,
                          ply_terminal_session_flags_t flags)
{
  int pid;
  bool run_in_parent, look_in_path;
  
  assert (session != NULL);

  run_in_parent = (flags & PLY_TERMINAL_SESSION_FLAGS_RUN_IN_PARENT) != 0;
  look_in_path = (flags & PLY_TERMINAL_SESSION_FLAGS_LOOK_IN_PATH) != 0;

  if (!ply_terminal_create_device (session->terminal))
    return false;

  pid = fork ();

  if (pid < 0)
    return false;

  if (((pid == 0) && run_in_parent) ||
      ((pid != 0) && !run_in_parent))
    return true;

  ply_terminal_session_execute (session, look_in_path);

  _exit (errno);
}

int
ply_terminal_session_get_fd (ply_terminal_session_t *session)
{
  assert (session != NULL);

  return ply_terminal_get_fd (session->terminal);
}

#ifdef PLY_TERMINAL_SESSION_ENABLE_TEST

#include <stdio.h>

int
main (int    argc,
      char **argv)
{
  ply_terminal_session_t *session;
  uint8_t byte;
  int exit_code;
  ply_terminal_session_flags_t flags;

  exit_code = 0;

  session = ply_terminal_session_new ((const char * const *) (argv + 1));

  flags = PLY_TERMINAL_SESSION_FLAGS_RUN_IN_PARENT;
  flags |= PLY_TERMINAL_SESSION_FLAGS_LOOK_IN_PATH;

  if (!ply_terminal_session_run (session, flags))
    {
      perror ("could not start terminal session");
      return errno;
    }

  while (read (ply_terminal_session_get_fd (session), 
               &byte, sizeof (byte)) == 1)
    printf ("%c", byte);

  ply_terminal_session_free (session);

  return exit_code;
}

#endif /* PLY_TERMINAL_SESSION_ENABLE_TEST */
/* vim: set ts=4 sw=4 expandtab autoindent cindent cino={.5s,(0: */
