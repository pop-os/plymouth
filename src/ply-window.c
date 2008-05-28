/* ply-window.h - APIs for putting up a window screen
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
#include "ply-window.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>
#include <wchar.h>

#include <linux/kd.h>

#include "ply-buffer.h"
#include "ply-event-loop.h"
#include "ply-logger.h"
#include "ply-utils.h"

#define KEY_CTRL_T ('\100' ^'T')
#define KEY_CTRL_V ('\100' ^'V')
#define KEY_ESCAPE ('\100' ^'[')
#define KEY_RETURN '\r'
#define KEY_BACKSPACE '\177'

struct _ply_window
{
  ply_event_loop_t *loop;
  ply_buffer_t     *keyboard_input_buffer;
  ply_buffer_t     *line_buffer;

  char *tty_name;
  int   tty_fd;

  ply_fd_watch_t *tty_fd_watch;
  ply_window_mode_t mode;

  uint32_t should_force_text_mode : 1;

  ply_window_keyboard_input_handler_t keyboard_input_handler;
  void *keyboard_input_handler_user_data;

  ply_window_backspace_handler_t backspace_handler;
  void *backspace_handler_user_data;

  ply_window_escape_handler_t escape_handler;
  void *escape_handler_user_data;

  ply_window_enter_handler_t enter_handler;
  void *enter_handler_user_data;
};

ply_window_t *
ply_window_new (const char *tty_name)
{
  ply_window_t *window;

  assert (tty_name != NULL);

  window = calloc (1, sizeof (ply_window_t));
  window->keyboard_input_buffer = ply_buffer_new ();
  window->line_buffer = ply_buffer_new ();
  window->loop = NULL;
  window->tty_name = strdup (tty_name);
  window->tty_fd = -1;

  return window;
}

static void
process_keyboard_input (ply_window_t *window,
                        const char   *keyboard_input,
                        size_t        character_size)
{
  wchar_t key;

  if (mbrtowc (&key, keyboard_input, 1, NULL) > 0)
    {
      switch (key)
        {
          case KEY_CTRL_V:
            ply_trace ("toggle verbose mode!");
            ply_toggle_tracing ();
            ply_trace ("verbose mode toggled!");
          return;

          case KEY_CTRL_T:
            ply_trace ("toggle text mode!");
            window->should_force_text_mode = !window->should_force_text_mode;
            ply_window_set_mode (window, window->mode);
            ply_trace ("text mode toggled!");
          return;

          case KEY_ESCAPE:
            ply_trace ("escape key!");
            if (window->escape_handler != NULL)
              window->escape_handler (window->escape_handler_user_data);
            ply_trace ("end escape key handler");
          return;

          case KEY_BACKSPACE:
            {
            ssize_t bytes_to_remove;
            ssize_t previous_character_size;
            const char *bytes;
            size_t size;
            ply_trace ("backspace key!");

            bytes = ply_buffer_get_bytes (window->line_buffer);
            size = ply_buffer_get_size (window->line_buffer);

            bytes_to_remove = MB_CUR_MAX;
            while ((previous_character_size = mbrlen (bytes + size - bytes_to_remove, bytes_to_remove, NULL)) < bytes_to_remove &&
                   previous_character_size > 0)
              bytes_to_remove -= previous_character_size;

            if (bytes_to_remove <= size)
              {
                ply_buffer_remove_bytes_at_end (window->line_buffer, bytes_to_remove);

                if (window->backspace_handler != NULL)
                  window->backspace_handler (window->backspace_handler_user_data);
              }
            }
          return;

          case KEY_RETURN:
            ply_trace ("return key!");

            if (window->enter_handler != NULL)
              window->enter_handler (window->enter_handler_user_data,
                                     ply_buffer_get_bytes (window->line_buffer));

            ply_buffer_clear (window->line_buffer);
          return;

          default:
            ply_buffer_append_bytes (window->line_buffer,
                                     keyboard_input, character_size);
          break;
        }
    }

  if (window->keyboard_input_handler != NULL)
    window->keyboard_input_handler (window->keyboard_input_handler_user_data,
                                    keyboard_input, character_size);
}

static void
check_buffer_for_key_events (ply_window_t *window)
{
  const char *bytes;
  size_t size, i;

  bytes = ply_buffer_get_bytes (window->keyboard_input_buffer);
  size = ply_buffer_get_size (window->keyboard_input_buffer);

  i = 0;
  while (i < size)
    {
      ssize_t character_size;
      char *keyboard_input;

      character_size = (ssize_t) mbrlen (bytes + i, size - i, NULL);

      if (character_size < 0)
        break;

      keyboard_input = strndup (bytes + i, character_size);

      process_keyboard_input (window, keyboard_input, character_size);

      free (keyboard_input);

      i += character_size;
    }

  if (i > 0)
    ply_buffer_remove_bytes (window->keyboard_input_buffer, i);
}

static void
on_key_event (ply_window_t *window)
{
  ply_buffer_append_from_fd (window->keyboard_input_buffer, window->tty_fd);

  check_buffer_for_key_events (window);
}

bool
ply_window_set_unbuffered_input (ply_window_t *window)
{
  struct termios term_attributes;

  tcgetattr (window->tty_fd, &term_attributes);
  cfmakeraw (&term_attributes);

  if (tcsetattr (window->tty_fd, TCSAFLUSH, &term_attributes) != 0)
    return false;

  return true;
}

bool
ply_window_open (ply_window_t *window)
{
  assert (window != NULL);
  assert (window->tty_name != NULL);
  assert (window->tty_fd < 0);

  window->tty_fd = open (window->tty_name, O_RDWR | O_NOCTTY);

  if (window->tty_fd < 0)
    return false;

  if (!ply_window_set_unbuffered_input (window))
    return false;

  if (!ply_window_set_mode (window, PLY_WINDOW_MODE_TEXT))
    return false;

  if (window->loop != NULL)
    window->tty_fd_watch = ply_event_loop_watch_fd (window->loop, window->tty_fd,
                                                    PLY_EVENT_LOOP_FD_STATUS_HAS_DATA,
                                                    (ply_event_handler_t) on_key_event,
                                                    NULL, window);

  return true;
}

void
ply_window_close (ply_window_t *window)
{
  ply_window_set_mode (window, PLY_WINDOW_MODE_TEXT);

  if (window->tty_fd_watch != NULL)
    {
      ply_event_loop_stop_watching_fd (window->loop, window->tty_fd_watch);
      window->tty_fd_watch = NULL;
    }

  close (window->tty_fd);
  window->tty_fd = -1;
}

bool
ply_window_set_mode (ply_window_t      *window,
                     ply_window_mode_t  mode)
{
  assert (window != NULL);
  assert (mode == PLY_WINDOW_MODE_TEXT || mode == PLY_WINDOW_MODE_GRAPHICS);

  switch (mode)
    {
      case PLY_WINDOW_MODE_TEXT:
        if (ioctl (window->tty_fd, KDSETMODE, KD_TEXT) < 0)
          return false;
        break;

      case PLY_WINDOW_MODE_GRAPHICS:
        if (ioctl (window->tty_fd, KDSETMODE,
                   window->should_force_text_mode? KD_TEXT : KD_GRAPHICS) < 0)
          return false;
        break;
    }
  ply_window_set_unbuffered_input (window);

  window->mode = mode;
  return true;
}

static void
ply_window_detach_from_event_loop (ply_window_t *window)
{
  assert (window != NULL);
  window->loop = NULL;
  window->tty_fd_watch = NULL;
}

void
ply_window_free (ply_window_t *window)
{
  if (window == NULL)
    return;

  if (window->loop != NULL)
    ply_event_loop_stop_watching_for_exit (window->loop,
                                           (ply_event_loop_exit_handler_t)
                                           ply_window_detach_from_event_loop,
                                           window);

  ply_window_set_mode (window, PLY_WINDOW_MODE_TEXT);
  ply_window_close (window);

  ply_buffer_free (window->keyboard_input_buffer);
  ply_buffer_free (window->line_buffer);

  free (window);
}

void
ply_window_set_keyboard_input_handler (ply_window_t *window,
                                       ply_window_keyboard_input_handler_t input_handler,
                                       void       *user_data)
{
  assert (window != NULL);

  window->keyboard_input_handler = input_handler;
  window->keyboard_input_handler_user_data = user_data;
}

void
ply_window_set_backspace_handler (ply_window_t *window,
                                  ply_window_backspace_handler_t backspace_handler,
                                  void         *user_data)
{
  assert (window != NULL);

  window->backspace_handler = backspace_handler;
  window->backspace_handler_user_data = user_data;
}

void
ply_window_set_escape_handler (ply_window_t *window,
                               ply_window_escape_handler_t escape_handler,
                               void       *user_data)
{
  assert (window != NULL);

  window->escape_handler = escape_handler;
  window->escape_handler_user_data = user_data;
}

void
ply_window_set_enter_handler (ply_window_t *window,
                              ply_window_enter_handler_t enter_handler,
                              void         *user_data)
{
  assert (window != NULL);

  window->enter_handler = enter_handler;
  window->enter_handler_user_data = user_data;
}

void
ply_window_attach_to_event_loop (ply_window_t     *window,
                                 ply_event_loop_t *loop)
{
  assert (window != NULL);
  assert (loop != NULL);
  assert (window->loop == NULL);

  window->loop = loop;

  if (window->tty_fd >= 0)
    window->tty_fd_watch = ply_event_loop_watch_fd (window->loop, window->tty_fd,
                                                    PLY_EVENT_LOOP_FD_STATUS_HAS_DATA,
                                                    (ply_event_handler_t) on_key_event,
                                                    NULL, window);

  ply_event_loop_watch_for_exit (loop, (ply_event_loop_exit_handler_t)
                                 ply_window_detach_from_event_loop,
                                 window);
}

#ifdef PLY_WINDOW_ENABLE_TEST

#include <stdio.h>

#include "ply-event-loop.h"
#include "ply-window.h"

static void
on_timeout (ply_window_t     *window,
            ply_event_loop_t *loop)
{
  ply_event_loop_exit (loop, 0);
}

static void
on_keypress (ply_window_t *window,
             const char   *keyboard_input)
{
  printf ("key '%c' (0x%x) was pressed\n",
          keyboard_input[0], (unsigned int) keyboard_input[0]);
}

int
main (int    argc,
      char **argv)
{
  ply_event_loop_t *loop;
  ply_window_t *window;
  int exit_code;
  const char *tty_name;

  exit_code = 0;

  loop = ply_event_loop_new ();

  if (argc > 1)
    tty_name = argv[1];
  else
    tty_name = "/dev/tty1";

  window = ply_window_new (tty_name);
  ply_window_attach_to_event_loop (window, loop);
  ply_window_set_keyboard_input_handler (window,
                                         (ply_window_keyboard_input_handler_t)
                                         on_keypress, window);

  if (!ply_window_open (window))
    {
      ply_save_errno ();
      perror ("could not open window");
      ply_restore_errno ();
      return errno;
    }

  if (!ply_window_set_mode (window, PLY_WINDOW_MODE_TEXT))
    {
      ply_save_errno ();
      perror ("could not set window for graphics mode");
      ply_restore_errno ();
    }

  ply_event_loop_watch_for_timeout (loop,
                                    15.0,
                                   (ply_event_loop_timeout_handler_t)
                                   on_timeout,
                                   window);
  exit_code = ply_event_loop_run (loop);

  ply_window_close (window);
  ply_window_free (window);

  return exit_code;
}

#endif /* PLY_WINDOW_ENABLE_TEST */
/* vim: set ts=4 sw=4 expandtab autoindent cindent cino={.5s,(0: */
