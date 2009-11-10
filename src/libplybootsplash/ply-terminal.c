/* ply-terminal.c - APIs for terminaling text
 *
 * Copyright (C) 2009 Red Hat, Inc.
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
#include "ply-terminal.h"

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
#include <linux/major.h>
#include <linux/vt.h>

#include "ply-buffer.h"
#include "ply-event-loop.h"
#include "ply-list.h"
#include "ply-logger.h"
#include "ply-utils.h"

#ifndef TEXT_PALETTE_SIZE
#define TEXT_PALETTE_SIZE 48
#endif

struct _ply_terminal
{
  ply_event_loop_t *loop;

  struct termios original_term_attributes;

  char *name;
  int   fd;
  int   vt_number;

  ply_fd_watch_t *fd_watch;
  ply_terminal_color_t foreground_color;
  ply_terminal_color_t background_color;

  uint8_t original_color_palette[TEXT_PALETTE_SIZE];
  uint8_t color_palette[TEXT_PALETTE_SIZE];

  int number_of_rows;
  int number_of_columns;

  uint32_t original_term_attributes_saved : 1;
  uint32_t supports_text_color : 1;
  uint32_t is_open : 1;
};

static bool ply_terminal_open_device (ply_terminal_t *terminal);

ply_terminal_t *
ply_terminal_new (const char *device_name)
{
  ply_terminal_t *terminal;

  terminal = calloc (1, sizeof (ply_terminal_t));

  terminal->loop = ply_event_loop_get_default ();
  if (device_name != NULL)
    {
      if (strncmp (device_name, "/dev/", strlen ("/dev/")) == 0)
        terminal->name = strdup (device_name);
      else
        asprintf (&terminal->name, "/dev/%s", device_name);
    }
  terminal->fd = -1;
  terminal->vt_number = -1;

  return terminal;
}

static void
ply_terminal_look_up_color_palette (ply_terminal_t *terminal)
{
  if (ioctl (terminal->fd, GIO_CMAP, terminal->color_palette) < 0)
    terminal->supports_text_color = false;
  else
    terminal->supports_text_color = true;
}

static bool
ply_terminal_change_color_palette (ply_terminal_t *terminal)
{
  if (!terminal->supports_text_color)
    return true;

  if (ioctl (terminal->fd, PIO_CMAP, terminal->color_palette) < 0)
    return false;

  return true;
}

static void
ply_terminal_save_color_palette (ply_terminal_t *terminal)
{
  if (!terminal->supports_text_color)
    return;

  memcpy (terminal->original_color_palette, terminal->color_palette,
          TEXT_PALETTE_SIZE);
}

static void
ply_terminal_restore_color_palette (ply_terminal_t *terminal)
{
  if (!terminal->supports_text_color)
    return;

  memcpy (terminal->color_palette, terminal->original_color_palette,
          TEXT_PALETTE_SIZE);

  ply_terminal_change_color_palette (terminal);
}

void
ply_terminal_reset_colors (ply_terminal_t *terminal)
{
  assert (terminal != NULL);

  ply_terminal_restore_color_palette (terminal);
}

bool
ply_terminal_set_unbuffered_input (ply_terminal_t *terminal)
{
  struct termios term_attributes;

  tcgetattr (terminal->fd, &term_attributes);

  if (!terminal->original_term_attributes_saved)
    {
      terminal->original_term_attributes = term_attributes;
      terminal->original_term_attributes_saved = true;
    }

  cfmakeraw (&term_attributes);

  /* Make \n return go to the beginning of the next line */
  term_attributes.c_oflag |= ONLCR;

  if (tcsetattr (terminal->fd, TCSAFLUSH, &term_attributes) != 0)
    return false;

  return true;
}

bool
ply_terminal_set_buffered_input (ply_terminal_t *terminal)
{
  struct termios term_attributes;

  tcgetattr (terminal->fd, &term_attributes);

  /* If someone already messed with the terminal settings,
   * and they seem good enough, bail
   */
  if (term_attributes.c_lflag & ICANON)
    return true;

  /* If we don't know the original term attributes, or they were originally sucky,
   * then invent some that are probably good enough.
   */
  if (!terminal->original_term_attributes_saved || !(terminal->original_term_attributes.c_lflag & ICANON))
    {
      term_attributes.c_iflag |= IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON;
      term_attributes.c_oflag |= OPOST;
      term_attributes.c_lflag |= ECHO | ECHONL | ICANON | ISIG | IEXTEN;

      if (tcsetattr (terminal->fd, TCSAFLUSH, &term_attributes) != 0)
        return false;

      return true;
    }

  if (tcsetattr (terminal->fd, TCSAFLUSH, &terminal->original_term_attributes) != 0)
    return false;

  return true;
}

void
ply_terminal_write (ply_terminal_t *terminal,
                    const char     *format,
                    ...)
{
  va_list args;
  char *string;

  assert (terminal != NULL);
  assert (format != NULL);

  ply_terminal_set_unbuffered_input (terminal);

  string = NULL;
  va_start (args, format);
  vasprintf (&string, format, args);
  va_end (args);

  write (terminal->fd, string, strlen (string));
  free (string);
}

static void
on_tty_disconnected (ply_terminal_t *terminal)
{
  ply_trace ("tty disconnected (fd %d)", terminal->fd);
  terminal->fd_watch = NULL;
  terminal->fd = -1;

  if (terminal->name != NULL)
    {
      ply_trace ("trying to reopen terminal '%s'", terminal->name);
      ply_terminal_open_device (terminal);
    }
}

static int
get_active_vt (void)
{
  int console_fd;
  struct vt_stat console_state = { 0 };

  console_fd = open ("/dev/tty0", O_RDONLY | O_NOCTTY);

  if (console_fd < 0)
    goto out;

  if (ioctl (console_fd, VT_GETSTATE, &console_state) < 0)
    goto out;

out:
  if (console_fd >= 0)
    close (console_fd);

  return console_state.v_active;
}

static bool
ply_terminal_look_up_geometry (ply_terminal_t *terminal)
{
    struct winsize terminal_size;

    ply_trace ("looking up terminal text geometry");

    if (ioctl (terminal->fd, TIOCGWINSZ, &terminal_size) < 0)
      {
        ply_trace ("could not read terminal text geometry: %m");
        terminal->number_of_columns = 80;
        terminal->number_of_rows = 24;
        return false;
      }

    terminal->number_of_rows = terminal_size.ws_row;
    terminal->number_of_columns = terminal_size.ws_col;

    ply_trace ("terminal is now %dx%d text cells",
               terminal->number_of_columns,
               terminal->number_of_rows);

    return true;
}

static void
ply_terminal_check_for_vt (ply_terminal_t *terminal)
{
  int major_number, minor_number;
  struct stat file_attributes;

  assert (terminal != NULL);
  assert (terminal->fd >= 0);

  if (fstat (terminal->fd, &file_attributes) != 0)
    return;

  major_number = major (file_attributes.st_rdev);
  minor_number = minor (file_attributes.st_rdev);

  if (major_number == TTY_MAJOR)
    terminal->vt_number = minor_number;
  else
    terminal->vt_number = -1;
}

static bool
ply_terminal_open_device (ply_terminal_t *terminal)
{
  assert (terminal != NULL);
  assert (terminal->name != NULL);
  assert (terminal->fd < 0);
  assert (terminal->fd_watch == NULL);

  terminal->fd = open (terminal->name, O_RDWR | O_NOCTTY);

  if (terminal->fd < 0)
    return false;

  terminal->fd_watch = ply_event_loop_watch_fd (terminal->loop, terminal->fd,
                                                   PLY_EVENT_LOOP_FD_STATUS_NONE,
                                                   (ply_event_handler_t) NULL,
                                                   (ply_event_handler_t) on_tty_disconnected,
                                                   terminal);

  ply_terminal_check_for_vt (terminal);

  if (!ply_terminal_set_unbuffered_input (terminal))
    ply_trace ("terminal '%s' will be line buffered", terminal->name);

  return true;
}

bool
ply_terminal_open (ply_terminal_t *terminal)
{
  assert (terminal != NULL);

  if (terminal->name == NULL)
    {
      char tty_name[512] = "";

      terminal->vt_number = get_active_vt ();

      if (readlink ("/proc/self/fd/0", tty_name, sizeof (tty_name) - 1) < 0)
        {
          ply_trace ("could not read tty name of fd 0");
          return false;
        }

      terminal->name = strdup (tty_name);
    }

  ply_trace ("trying to open terminal '%s'", terminal->name);

  if (!ply_terminal_open_device (terminal))
    {
      ply_trace ("could not open %s : %m", terminal->name);
      return false;
    }

  ply_terminal_look_up_geometry (terminal);

  ply_terminal_look_up_color_palette (terminal);
  ply_terminal_save_color_palette (terminal);

  ply_event_loop_watch_signal (terminal->loop,
                               SIGWINCH,
                               (ply_event_handler_t)
                               ply_terminal_look_up_geometry,
                               terminal);

  terminal->is_open = true;

  return true;
}

int
ply_terminal_get_fd (ply_terminal_t *terminal)
{
  return terminal->fd;
}

bool
ply_terminal_is_open (ply_terminal_t *terminal)
{
  return terminal->is_open;
}

void
ply_terminal_close (ply_terminal_t *terminal)
{
  terminal->is_open = false;

  ply_trace ("restoring color palette");
  ply_terminal_restore_color_palette (terminal);

  if (terminal->fd_watch != NULL)
    {
      ply_trace ("stop watching tty fd");
      ply_event_loop_stop_watching_fd (terminal->loop, terminal->fd_watch);
      terminal->fd_watch = NULL;
    }

  if (terminal->loop != NULL)
    {
      ply_trace ("stop watching SIGWINCH signal");
      ply_event_loop_stop_watching_signal (terminal->loop, SIGWINCH);
    }

  ply_trace ("setting buffered input");
  ply_terminal_set_buffered_input (terminal);

  close (terminal->fd);
  terminal->fd = -1;
}

int
ply_terminal_get_number_of_columns (ply_terminal_t *terminal)
{
  return terminal->number_of_columns;
}

int
ply_terminal_get_number_of_rows (ply_terminal_t *terminal)
{
  return terminal->number_of_rows;
}

uint32_t
ply_terminal_get_color_hex_value (ply_terminal_t       *terminal,
                                  ply_terminal_color_t  color)
{
  uint8_t red, green, blue;
  uint32_t hex_value;

  assert (terminal != NULL);
  assert (color <= PLY_TERMINAL_COLOR_WHITE);

  red = (uint8_t) *(terminal->color_palette + 3 * color);
  green = (uint8_t) *(terminal->color_palette + 3 * color + 1);
  blue = (uint8_t) *(terminal->color_palette + 3 * color + 2);

  hex_value = red << 16 | green << 8 | blue;

  return hex_value;
}

void
ply_terminal_set_color_hex_value (ply_terminal_t       *terminal,
                                  ply_terminal_color_t  color,
                                  uint32_t              hex_value)
{
  uint8_t red, green, blue;

  assert (terminal != NULL);
  assert (color <= PLY_TERMINAL_COLOR_WHITE);

  red = (uint8_t) ((hex_value >> 16) & 0xff);
  green = (uint8_t) ((hex_value >> 8) & 0xff);
  blue = (uint8_t) (hex_value & 0xff);

  *(terminal->color_palette + 3 * color) = red;
  *(terminal->color_palette + 3 * color + 1) = green;
  *(terminal->color_palette + 3 * color + 2) = blue;

  ply_terminal_change_color_palette (terminal);
}

bool
ply_terminal_supports_color (ply_terminal_t *terminal)
{
  return terminal->supports_text_color;
}

static void
ply_terminal_detach_from_event_loop (ply_terminal_t *terminal)
{
  assert (terminal != NULL);
  terminal->loop = NULL;
  terminal->fd_watch = NULL;
}

void
ply_terminal_free (ply_terminal_t *terminal)
{
  if (terminal == NULL)
    return;

  free (terminal->name);

  if (terminal->loop != NULL)
    ply_event_loop_stop_watching_for_exit (terminal->loop,
                                           (ply_event_loop_exit_handler_t)
                                           ply_terminal_detach_from_event_loop,
                                           terminal);

  if (terminal->is_open)
    ply_terminal_close (terminal);

  free (terminal);
}

int
ply_terminal_get_vt_number (ply_terminal_t *terminal)
{
  return terminal->vt_number;
}

/* vim: set ts=4 sw=4 et ai ci cino={.5s,^-2,+.5s,t0,g0,e-2,n-2,p2s,(0,=.5s,:.5s */
