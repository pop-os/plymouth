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

#include <dirent.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/types.h>
#include <sysexits.h>
#include <unistd.h>

#include "ply-answer.h"
#include "ply-boot-server.h"
#include "ply-boot-splash.h"
#include "ply-event-loop.h"
#include "ply-logger.h"
#include "ply-terminal-session.h"
#include "ply-utils.h"

#ifndef PLY_WORKING_DIRECTORY
#define PLY_WORKING_DIRECTORY "/var/run/plymouth"
#endif

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
  int original_root_dir_fd;

  char kernel_command_line[PLY_MAX_COMMAND_LINE_SIZE];
} state_t;

static ply_boot_splash_t *start_boot_splash (state_t    *state,
                                             const char *module_path);

static void
on_session_start (state_t *state)
{
  ply_trace ("changing to original root fs");

  if (fchdir (state->original_root_dir_fd) < 0)
    {
      ply_trace ("Could not change to original root directory "
                 "to start session: %m");
      return;
    }
}

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
on_ask_for_password (state_t      *state,
                     ply_answer_t *answer)
{
  if (state->boot_splash != NULL)
    {
      ply_answer_with_string (answer, "");
      return;
    }

  ply_boot_splash_ask_for_password (state->boot_splash, answer);
}

static void
on_system_initialized (state_t *state)
{

  ply_trace ("system now initialized, ready to mount root filesystem");
  mknod ("/dev/root", 0600 | S_IFBLK, makedev (253, 0));
  mount("/dev/root", "/sysroot", "ext3", 0, NULL);
  ply_terminal_session_open_log (state->session,
                                 "/sysroot/var/log/bootmessages.log");
}

static void
on_show_splash (state_t *state)
{
  ply_trace ("Showing splash screen");
  state->boot_splash = start_boot_splash (state,
                                          PLYMOUTH_PLUGIN_PATH "spinfinity.so");

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
on_quit (state_t *state)
{
  ply_trace ("time to quit, closing log");
  ply_terminal_session_close_log (state->session);
  ply_trace ("hiding splash");
  if (state->boot_splash != NULL)
    ply_boot_splash_hide (state->boot_splash);
  ply_trace ("exiting event loop");
  ply_event_loop_exit (state->loop, 0);

  ply_trace ("switching root dir");
  fchdir (state->original_root_dir_fd);
  chroot (".");
  ply_trace ("unmounting temporary filesystem mounts");
  ply_unmount_filesystem (PLY_WORKING_DIRECTORY "/sysroot");
  ply_unmount_filesystem (PLY_WORKING_DIRECTORY "/proc");
  ply_unmount_filesystem (PLY_WORKING_DIRECTORY "/dev/pts");
  ply_unmount_filesystem (PLY_WORKING_DIRECTORY);
}

static ply_boot_server_t *
start_boot_server (state_t *state)
{
  ply_boot_server_t *server;

  server = ply_boot_server_new ((ply_boot_server_update_handler_t) on_update,
                                (ply_boot_server_ask_for_password_handler_t) on_ask_for_password,
                                (ply_boot_server_show_splash_handler_t) on_show_splash,
                                (ply_boot_server_system_initialized_handler_t) on_system_initialized,
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
  ply_boot_splash_hide (state->boot_splash);
  ply_boot_splash_free (state->boot_splash);

  state->boot_splash = start_boot_splash (state, PLYMOUTH_PLUGIN_PATH "details.so");
}

static ply_window_t *
create_window (state_t    *state,
               const char *tty)
{
  ply_window_t *window;

  ply_trace ("creating window for %s", tty);
  window = ply_window_new (tty);

  ply_trace ("attaching window to event loop");
  ply_window_attach_to_event_loop (window, state->loop);

  ply_trace ("opening window");
  if (!ply_window_open (window))
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
spawn_session (state_t  *state,
               char    **argv)
{
  ply_terminal_session_t *session;
  ply_terminal_session_flags_t flags;

  flags = 0;
  flags |= PLY_TERMINAL_SESSION_FLAGS_RUN_IN_PARENT;
  flags |= PLY_TERMINAL_SESSION_FLAGS_LOOK_IN_PATH;
  flags |= PLY_TERMINAL_SESSION_FLAGS_REDIRECT_CONSOLE;
  flags |= PLY_TERMINAL_SESSION_FLAGS_CHANGE_ROOT_TO_CURRENT_DIRECTORY;

  ply_trace ("opening terminal session for '%s'", argv[0]);
  session = ply_terminal_session_new ((const char * const *) argv);
  ply_trace ("attaching terminal session to event loop");
  ply_terminal_session_attach_to_event_loop (session, state->loop);

  ply_trace ("running '%s'", argv[0]);
  if (!ply_terminal_session_run (session, flags,
                                 (ply_terminal_session_begin_handler_t)
                                 on_session_start,
                                 (ply_terminal_session_output_handler_t)
                                 on_session_output,
                                 (ply_terminal_session_done_handler_t)
                                 on_session_finished, state))
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
create_working_directory (state_t *state)
{
  ply_trace ("creating working directory '%s'",
             PLY_WORKING_DIRECTORY);
  if (!ply_create_detachable_directory (PLY_WORKING_DIRECTORY))
    return false;

  ply_trace ("changing to working directory");
  if (chdir (PLY_WORKING_DIRECTORY) < 0)
    return false;

  ply_trace ("creating proc subdirectory");
  if (!ply_create_directory ("proc"))
    return false;

  ply_trace ("creating dev subdirectory");
  if (!ply_create_directory ("dev"))
    return false;

  ply_trace ("creating dev/pts subdirectory");
  if (!ply_create_directory ("dev/pts"))
    return false;

  ply_trace ("creating usr/share/plymouth subdirectory");
  if (!ply_create_directory ("usr/share/plymouth"))
    return false;

  ply_trace ("creating " PLYMOUTH_PLUGIN_PATH " subdirectory");
  if (!ply_create_directory (PLYMOUTH_PLUGIN_PATH + 1))
    return false;

  ply_trace ("creating sysroot subdirectory");
  if (!ply_create_directory ("sysroot"))
    return false;

  return true;
}

static bool
change_to_working_directory (state_t *state)
{
  ply_trace ("changing to working directory");

  state->original_root_dir_fd = open ("/", O_RDONLY);

  if (state->original_root_dir_fd < 0)
    return false;

  if (chdir (PLY_WORKING_DIRECTORY) < 0)
    return false;

  if (chroot (".") < 0)
    return false;

  ply_trace ("now successfully in working directory");
  return true;
}

static bool
mount_proc_filesystem (state_t *state)
{
  ply_trace ("mounting proc filesystem");
  if (mount ("none", PLY_WORKING_DIRECTORY "/proc", "proc", 0, NULL) < 0)
    return false;

  open (PLY_WORKING_DIRECTORY "/proc/.", O_RDWR);

  ply_trace ("mounted proc filesystem");
  return true;
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


static bool
create_device_nodes (state_t *state)
{
  ply_trace ("creating device nodes");

  if (mknod ("./dev/root", 0600 | S_IFBLK, makedev (253, 0)) < 0)
    return false;

  if (mknod ("./dev/null", 0600 | S_IFCHR, makedev (1, 3)) < 0)
    return false;

  if (mknod ("./dev/console", 0600 | S_IFCHR, makedev (5, 1)) < 0)
    return false;

  if (mknod ("./dev/tty", 0600 | S_IFCHR, makedev (5, 0)) < 0)
    return false;

  if (mknod ("./dev/tty0", 0600 | S_IFCHR, makedev (4, 0)) < 0)
    return false;

  if (mknod ("./dev/tty1", 0600 | S_IFCHR, makedev (4, 1)) < 0)
    return false;

  if (mknod ("./dev/ptmx", 0600 | S_IFCHR, makedev (5, 2)) < 0)
    return false;

  if (mknod ("./dev/fb", 0600 | S_IFCHR, makedev (29, 0)) < 0)
    return false;

  ply_trace ("created device nodes");
  return true;
}

static bool
mount_devpts_filesystem (state_t *state)
{
  ply_trace ("mounting devpts filesystem");
  if (mount ("none", PLY_WORKING_DIRECTORY "/dev/pts", "devpts", 0,
             "gid=5,mode=620") < 0)
    return false;

  open (PLY_WORKING_DIRECTORY "/dev/pts/.", O_RDWR);

  ply_trace ("mounted devpts filesystem");
  return true;
}

static bool
copy_data_files (state_t *state)
{
  char *logo_dir, *p;

  ply_trace ("copying data files");
  if (!ply_copy_directory ("/usr/share/plymouth",
                           "usr/share/plymouth"))
    return false;
  ply_trace ("copied data files");

  ply_trace ("copying plugins");
  if (!ply_copy_directory (PLYMOUTH_PLUGIN_PATH,
                           PLYMOUTH_PLUGIN_PATH + 1))
    return false;

  ply_trace ("copying logo");
  logo_dir = strdup (PLYMOUTH_LOGO_FILE);
  p = strrchr (logo_dir, '/');

  if (p != NULL)
    *p = '\0';

  if (!ply_create_directory (logo_dir + 1))
    {
      free (logo_dir);
      return false;
    }
  free (logo_dir);

  if (!ply_copy_file (PLYMOUTH_LOGO_FILE,
                      PLYMOUTH_LOGO_FILE + 1))
    return false;
  ply_trace ("copied plugins files");

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

static bool
set_console_io_to_vt1 (state_t *state)
{
  int fd;

  fd = open ("/dev/tty1", O_RDWR | O_APPEND);

  if (fd < 0)
    return false;

  dup2 (fd, STDIN_FILENO);
  dup2 (fd, STDOUT_FILENO);
  dup2 (fd, STDERR_FILENO);

  return true;
}

static bool
plymouth_should_be_running (state_t *state)
{
  ply_trace ("checking if plymouth should be running");

  if ((strstr (state->kernel_command_line, " single ") != NULL)
    || (strstr (state->kernel_command_line, "single ") != NULL)
    || (strstr (state->kernel_command_line, " single") != NULL))
    {
      ply_trace ("kernel command line has option 'single'");
      return false;
    }

  if ((strstr (state->kernel_command_line, " 1 ") != NULL)
    || (strstr (state->kernel_command_line, "1 ") != NULL)
    || (strstr (state->kernel_command_line, " 1") != NULL))
    {
      ply_trace ("kernel command line has option '1'");
      return false;
    }

  return true;
}

static bool
initialize_environment (state_t *state)
{
  ply_trace ("initializing minimal work environment");
  if (!create_working_directory (state))
    return false;

  if (!create_device_nodes (state))
    return false;

  if (!copy_data_files (state))
    return false;

  if (!mount_proc_filesystem (state))
    return false;

  if (!get_kernel_command_line (state))
    return false;

  check_verbosity (state);

  if (!plymouth_should_be_running (state))
    return false;

  if (!mount_devpts_filesystem (state))
    return false;

  if (!change_to_working_directory (state))
    return false;

  if (!set_console_io_to_vt1 (state))
    return false;

  ply_trace ("initialized minimal work environment");
  return true;
}

int
main (int    argc,
      char **argv)
{
  state_t state = { 0 };
  int exit_code;

  if (argc <= 1)
    {
      ply_error ("%s other-command [other-command-args]", argv[0]);
      return EX_USAGE;
    }

  state.loop = ply_event_loop_new ();

  /* before do anything we need to make sure we have a working
   * environment.  /proc needs to be mounted and certain devices need
   * to be accessible (like the framebuffer device, pseudoterminal
   * devices, etc)
   */
  if (!initialize_environment (&state))
    {
      ply_error ("could not setup basic operating environment: %m");
      ply_list_directory (PLY_WORKING_DIRECTORY);
      return EX_OSERR;
    }

  state.boot_buffer = ply_buffer_new ();

  state.session = spawn_session (&state, argv + 1);

  if (state.session == NULL)
    {
      ply_error ("could not run '%s': %m", argv[1]);
      return EX_UNAVAILABLE;
    }

  state.boot_server = start_boot_server (&state);

  if (state.boot_server == NULL)
    {
      ply_error ("could not log bootup: %m");
      return EX_UNAVAILABLE;
    }

  state.window = create_window (&state, "/dev/tty1");

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

  return exit_code;
}
/* vim: set ts=4 sw=4 expandtab autoindent cindent cino={.5s,(0: */
