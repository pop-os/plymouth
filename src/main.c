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

#include <sys/stat.h>
#include <sys/types.h>
#include <limits.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <sysexits.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <linux/kd.h>

#include "ply-answer.h"
#include "ply-boot-server.h"
#include "ply-boot-splash.h"
#include "ply-event-loop.h"
#include "ply-logger.h"
#include "ply-terminal-session.h"
#include "ply-utils.h"

#ifndef PLY_MAX_COMMAND_LINE_SIZE
#define PLY_MAX_COMMAND_LINE_SIZE 512
#endif

typedef struct
{
  ply_event_loop_t *loop;
  ply_boot_server_t *boot_server;
  ply_window_t *window;
  ply_boot_splash_t *boot_splash;
  ply_terminal_session_t *session;
  ply_buffer_t *boot_buffer;
  long ptmx;

  char kernel_command_line[PLY_MAX_COMMAND_LINE_SIZE];
  uint32_t showing_details : 1;
  uint32_t system_initialized : 1;

  char *console;

  int number_of_errors;
} state_t;

static ply_boot_splash_t *start_boot_splash (state_t    *state,
                                             const char *module_path);

static ply_window_t *create_window (state_t *state, int vt_number);

static void
on_session_output (state_t    *state,
                   const char *output,
                   size_t      size)
{
  ply_buffer_append_bytes (state->boot_buffer, output, size);

  if (state->boot_splash != NULL)
    ply_boot_splash_update_output (state->boot_splash,
                                   output, size);
}

static void
on_session_finished (state_t *state)
{
  ply_log ("\nSession finished...exiting logger\n");
  ply_flush_log ();
  ply_free_log ();
  ply_event_loop_exit (state->loop, 1);
}

static void
on_update (state_t     *state,
           const char  *status)
{
  ply_trace ("updating status to '%s'", status);
  if (state->boot_splash != NULL)
    ply_boot_splash_update_status (state->boot_splash,
                                   status);
}

static void
show_detailed_splash (state_t *state)
{
  ply_trace ("Showing detailed splash screen");
  state->boot_splash = start_boot_splash (state,
                                          PLYMOUTH_PLUGIN_PATH "details.so");

  if (state->boot_splash == NULL)
    {
      ply_trace ("Could not start detailed splash screen, exiting");
      exit (1);
    }
}

static void
show_default_splash (state_t *state)
{
  ply_trace ("Showing splash screen");
  state->boot_splash = start_boot_splash (state,
                                          PLYMOUTH_PLUGIN_PATH "default.so");

  if (state->boot_splash == NULL)
    {
      ply_trace ("Could not start graphical splash screen,"
                 "showing text splash screen");
      state->boot_splash = start_boot_splash (state,
                                              PLYMOUTH_PLUGIN_PATH "text.so");
    }

  if (state->boot_splash == NULL)
    ply_error ("could not start boot splash: %m");
}

static void
on_ask_for_password (state_t      *state,
                     const char   *prompt,
                     ply_answer_t *answer)
{
  if (state->boot_splash == NULL)
    {
      show_detailed_splash (state);
      if (state->boot_splash == NULL)
        ply_answer_with_string (answer, "");
      return;
    }

  ply_boot_splash_ask_for_password (state->boot_splash,
                                    prompt, answer);
}

static void
on_newroot (state_t    *state,
             const char *root_dir)
{
  ply_trace ("new root mounted at \"%s\", switching to it", root_dir);
  chdir(root_dir);
  chroot(".");
  chdir("/");

  ply_boot_splash_root_mounted (state->boot_splash);
}

static void
spool_error (state_t *state)
{
  ply_trace ("spooling error for viewer");

  unlink (PLYMOUTH_SPOOL_DIRECTORY "/boot.log");

  ply_create_file_link (PLYMOUTH_LOG_DIRECTORY "/boot.log",
                        PLYMOUTH_SPOOL_DIRECTORY "/boot.log");
}

static void
on_system_initialized (state_t *state)
{
  ply_trace ("system now initialized, opening boot.log");
  state->system_initialized = true;
  ply_terminal_session_open_log (state->session,
                                 PLYMOUTH_LOG_DIRECTORY "/boot.log");

  if (state->number_of_errors > 0)
    spool_error (state);
}

static void
on_error (state_t *state)
{
  ply_trace ("encountered error during boot up");

  if (state->system_initialized && state->number_of_errors == 0)
    spool_error (state);
  else
    ply_trace ("not spooling because number of errors %d", state->number_of_errors);

  state->number_of_errors++;
}

static bool
plymouth_should_show_default_splash (state_t *state)
{
  ply_trace ("checking if plymouth should be running");

  const char const *strings[] = {
      " single ", " single", "^single ",
      " 1 ", " 1", "^1 ",
      " init=", "^init=",
      NULL
  };
  int i;

  if (state->console != NULL)
    return false;

  if (state->window == NULL)
    return false;

  for (i = 0; strings[i] != NULL; i++)
    {
      int cmp;
      if (strings[i][0] == '^')
          cmp = strncmp(state->kernel_command_line, strings[i]+1,
                        strlen(strings[i]+1)) == 0;
      else
          cmp = strstr (state->kernel_command_line, strings[i]) != NULL;

      if (cmp)
        {
          ply_trace ("kernel command line has option \"%s\"", strings[i]);
          return false;
        }
    }

  return strstr (state->kernel_command_line, "rhgb") != NULL;
}

static void
on_show_splash (state_t *state)
{

  if (state->window == NULL)
    {
      state->window = create_window (state, 1);

      if (state->window != NULL && state->console == NULL)
        ply_window_take_console (state->window);
    }

  if (plymouth_should_show_default_splash (state))
    show_default_splash (state);
  else
    show_detailed_splash (state);
}

static void
on_hide_splash (state_t *state)
{

  ply_trace ("hiding boot splash");
  if (state->boot_splash != NULL)
    {
      ply_boot_splash_hide (state->boot_splash);
      ply_boot_splash_free (state->boot_splash);
      state->boot_splash = NULL;
    }

  ply_trace ("closing splash window");
  if (state->window != NULL)
    {
      ply_window_close (state->window);
      ply_window_free (state->window);
      state->window = NULL;
    }

  if (state->session != NULL)
    {
      ply_trace ("unredirecting console");
      int fd;

      fd = open ("/dev/console", O_RDWR | O_NOCTTY);
      if (fd >= 0)
          ioctl (fd, TIOCCONS);

      close (fd);
    }
}

static void
on_quit (state_t *state,
         bool     retain_splash)
{
  ply_trace ("time to quit, closing boot.log");
  if (state->session != NULL)
    ply_terminal_session_close_log (state->session);
  ply_trace ("unloading splash");
  if (state->boot_splash != NULL)
    {
      if (!retain_splash)
        ply_boot_splash_hide (state->boot_splash);
      ply_boot_splash_free (state->boot_splash);
      state->boot_splash = NULL;
    }
  ply_trace ("exiting event loop");
  ply_event_loop_exit (state->loop, 0);
}

static ply_boot_server_t *
start_boot_server (state_t *state)
{
  ply_boot_server_t *server;

  server = ply_boot_server_new ((ply_boot_server_update_handler_t) on_update,
                                (ply_boot_server_ask_for_password_handler_t) on_ask_for_password,
                                (ply_boot_server_show_splash_handler_t) on_show_splash,
                                (ply_boot_server_hide_splash_handler_t) on_hide_splash,
                                (ply_boot_server_newroot_handler_t) on_newroot,
                                (ply_boot_server_system_initialized_handler_t) on_system_initialized,
                                (ply_boot_server_error_handler_t) on_error,
                                (ply_boot_server_quit_handler_t) on_quit,
                                state);

  if (!ply_boot_server_listen (server))
    {
      ply_save_errno ();
      ply_boot_server_free (server);
      ply_restore_errno ();
      return NULL;
    }

  ply_boot_server_attach_to_event_loop (server, state->loop);

  return server;
}

static void
on_escape_pressed (state_t *state)
{
  if (state->boot_splash != NULL)
    {
      ply_boot_splash_hide (state->boot_splash);
      ply_boot_splash_free (state->boot_splash);
      state->boot_splash = NULL;
    }

  if (!state->showing_details)
    {
      show_detailed_splash (state);
      state->showing_details = true;
    }
  else
    {
      show_default_splash (state);
      state->showing_details = false;
    }
}

static ply_window_t *
create_window (state_t    *state,
               int         vt_number)
{
  ply_window_t *window;

  ply_trace ("creating window on vt %d", vt_number);
  window = ply_window_new (vt_number);

  ply_trace ("attaching window to event loop");
  ply_window_attach_to_event_loop (window, state->loop);

  ply_trace ("opening window");
  if (state->console == NULL && !ply_window_open (window))
    {
      ply_save_errno ();
      ply_trace ("could not open window: %m");
      ply_window_free (window);
      ply_restore_errno ();
      return NULL;
    }

  ply_trace ("listening for escape key");
  ply_window_set_escape_handler (window, (ply_window_escape_handler_t)
                                 on_escape_pressed, state);

  return window;
}

static ply_boot_splash_t *
start_boot_splash (state_t    *state,
                   const char *module_path)
{
  ply_boot_splash_t *splash;

  ply_trace ("Loading boot splash plugin '%s'",
             module_path);
  splash = ply_boot_splash_new (module_path, state->window, state->boot_buffer);

  ply_trace ("attaching plugin to event loop");
  ply_boot_splash_attach_to_event_loop (splash, state->loop);

  ply_trace ("showing plugin");
  if (!ply_boot_splash_show (splash))
    {
      ply_save_errno ();
      ply_boot_splash_free (splash);
      ply_restore_errno ();
      return NULL;
    }

  return splash;
}

static ply_terminal_session_t *
attach_to_running_session (state_t *state)
{
  ply_terminal_session_t *session;
  ply_terminal_session_flags_t flags;

  flags = 0;
  flags |= PLY_TERMINAL_SESSION_FLAGS_REDIRECT_CONSOLE;

  ply_trace ("creating terminal session for current terminal");
  session = ply_terminal_session_new (NULL);
  ply_trace ("attaching terminal session to event loop");
  ply_terminal_session_attach_to_event_loop (session, state->loop);

  if (!ply_terminal_session_attach (session, flags,
                                 (ply_terminal_session_output_handler_t)
                                 on_session_output,
                                 (ply_terminal_session_done_handler_t)
                                 on_session_finished,
                                 state->ptmx,
                                 state))
    {
      ply_save_errno ();
      ply_terminal_session_free (session);
      ply_buffer_free (state->boot_buffer);
      state->boot_buffer = NULL;
      ply_restore_errno ();
      return NULL;
    }

  return session;
}

static bool
get_kernel_command_line (state_t *state)
{
  int fd;

  ply_trace ("opening /proc/cmdline");
  fd = open ("proc/cmdline", O_RDONLY);

  if (fd < 0)
    {
      ply_trace ("couldn't open it: %m");
      return false;
    }

  ply_trace ("reading kernel command line");
  if (read (fd, state->kernel_command_line, sizeof (state->kernel_command_line)) < 0)
    {
      ply_trace ("couldn't read it: %m");
      return false;
    }

  ply_trace ("Kernel command line is: '%s'", state->kernel_command_line);
  return true;
}

static void
check_verbosity (state_t *state)
{
  ply_trace ("checking if tracing should be enabled");

  if ((strstr (state->kernel_command_line, " plymouth:debug ") != NULL)
     || (strstr (state->kernel_command_line, "plymouth:debug ") != NULL)
     || (strstr (state->kernel_command_line, " plymouth:debug") != NULL))
    {
      ply_trace ("tracing should be enabled!");
      if (!ply_is_tracing ())
        ply_toggle_tracing ();
    }
  else
    ply_trace ("tracing shouldn't be enabled!");
}

static void
check_for_serial_console (state_t *state)
{
  char *console_key;
  char *remaining_command_line;

  ply_trace ("checking if splash screen should be disabled");

  remaining_command_line = state->kernel_command_line;
  while ((console_key = strstr (remaining_command_line, " console=")) != NULL)
    {
      char *end;
      ply_trace ("serial console found!");

      free (state->console);
      state->console = strdup (console_key + strlen (" console="));

      remaining_command_line = console_key + strlen (" console=");

      end = strpbrk (state->console, " \n\t\v,");

      if (end != NULL)
        {
          *end = '\0';
          remaining_command_line += end - state->console;
        }
    }
}

static bool
redirect_standard_io_to_device (char *device)
{
  int fd;
  char *file;

  ply_trace ("redirecting stdio to %s", device);

  if (strncmp (device, "/dev/", strlen ("/dev/")) == 0)
    file = strdup (device);
  else
    asprintf (&file, "/dev/%s", device);

  fd = open (file, O_RDWR | O_APPEND);

  free (file);

  if (fd < 0)
    return false;

  dup2 (fd, STDIN_FILENO);
  dup2 (fd, STDOUT_FILENO);
  dup2 (fd, STDERR_FILENO);

  return true;
}

static bool
initialize_environment (state_t *state)
{
  ply_trace ("initializing minimal work environment");

  if (!get_kernel_command_line (state))
    return false;

  check_verbosity (state);
  check_for_serial_console (state);

  if (state->console != NULL)
    redirect_standard_io_to_device (state->console);
  else
    redirect_standard_io_to_device ("tty1");

  ply_trace ("initialized minimal work environment");
  return true;
}

static void
on_crash (int signum)
{
    int fd;

    fd = open ("/dev/tty1", O_RDWR | O_NOCTTY);

    ioctl (fd, KDSETMODE, KD_TEXT);

    close (fd);

    signal (signum, SIG_DFL);
    raise(signum);
}

int
main (int    argc,
      char **argv)
{
  state_t state = {
      .ptmx = -1,
  };
  int exit_code;
  bool attach_to_session = false;
  ply_daemon_handle_t *daemon_handle;

  if (argc >= 2 && !strcmp(argv[1], "--attach-to-session"))
      attach_to_session = true;

  if (attach_to_session && argc == 3)
    {
      state.ptmx = strtol(argv[2], NULL, 0);
      if ((state.ptmx == LONG_MIN || state.ptmx == LONG_MAX) && errno != 0)
        {
          ply_error ("%s: could not parse ptmx string \"%s\": %m", argv[0], argv[2]);
          return EX_OSERR;
        }
    }

  if ((attach_to_session && argc != 3) || (attach_to_session && state.ptmx == -1))
    {
      ply_error ("%s [--attach-to-session <pty_master_fd>]", argv[0]);
      return EX_USAGE;
    }

  if (geteuid () != 0)
    {
      ply_error ("plymouthd must be run as root user");
      return EX_OSERR;
    }

  chdir ("/");

  daemon_handle = ply_create_daemon ();

  if (daemon_handle == NULL)
    {
      ply_error ("cannot daemonize: %m");
      return EX_UNAVAILABLE;
    }

  signal (SIGABRT, on_crash);
  signal (SIGSEGV, on_crash);

  state.loop = ply_event_loop_new ();

  /* before do anything we need to make sure we have a working
   * environment.
   */
  if (!initialize_environment (&state))
    {
      if (errno == 0)
        {
          ply_detach_daemon (daemon_handle, 0);
          return 0;
        }

      ply_error ("could not setup basic operating environment: %m");
      ply_detach_daemon (daemon_handle, EX_OSERR);
      return EX_OSERR;
    }

  state.boot_buffer = ply_buffer_new ();

  if (attach_to_session)
    {
      state.session = attach_to_running_session (&state);

      if (state.session == NULL)
        {
          ply_error ("could not create session: %m");
          ply_detach_daemon (daemon_handle, EX_UNAVAILABLE);
          return EX_UNAVAILABLE;
        }
    }

  state.boot_server = start_boot_server (&state);

  if (state.boot_server == NULL)
    {
      ply_error ("could not log bootup: %m");
      ply_detach_daemon (daemon_handle, EX_UNAVAILABLE);
      return EX_UNAVAILABLE;
    }

  if (!ply_detach_daemon (daemon_handle, 0))
    {
      ply_error ("could not tell parent to exit: %m");
      return EX_UNAVAILABLE;
    }

  ply_trace ("entering event loop");
  exit_code = ply_event_loop_run (state.loop);
  ply_trace ("exited event loop");

  ply_boot_splash_free (state.boot_splash);
  state.boot_splash = NULL;

  ply_window_free (state.window);
  state.window = NULL;

  ply_boot_server_free (state.boot_server);
  state.boot_server = NULL;

  ply_trace ("freeing terminal session");
  ply_terminal_session_free (state.session);

  ply_buffer_free (state.boot_buffer);

  ply_trace ("freeing event loop");
  ply_event_loop_free (state.loop);

  ply_trace ("exiting with code %d", exit_code);

  ply_free_error_log();

  return exit_code;
}
/* vim: set sts=4 ts=4 sw=4 expandtab autoindent cindent cino={.5s,(0: */
