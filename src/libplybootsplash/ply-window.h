/* ply-window.h - APIs for putting up a splash screen
 *
 * Copyright (C) 2008 Red Hat, Inc.
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
 * Written By: Ray Strode <rstrode@redhat.com>
 */
#ifndef PLY_WINDOW_H
#define PLY_WINDOW_H

#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>

#include "ply-buffer.h"
#include "ply-event-loop.h"
#include "ply-frame-buffer.h"

typedef struct _ply_window ply_window_t;

typedef void (* ply_window_keyboard_input_handler_t) (void *user_data,
                                                      const char *keyboard_input,
                                                      size_t      character_size);

typedef void (* ply_window_backspace_handler_t) (void *user_data);

typedef void (* ply_window_escape_handler_t) (void *user_data);
typedef void (* ply_window_enter_handler_t) (void *user_data,
                                             const char *line);

typedef enum
{
  PLY_WINDOW_MODE_TEXT,
  PLY_WINDOW_MODE_GRAPHICS
} ply_window_mode_t;

#ifndef PLY_HIDE_FUNCTION_DECLARATIONS
ply_window_t *ply_window_new (int vt_number);
void ply_window_free (ply_window_t *window);

void ply_window_set_keyboard_input_handler (ply_window_t *window,
                                            ply_window_keyboard_input_handler_t input_handler,
                                            void         *user_data);
void ply_window_set_backspace_handler (ply_window_t *window,
                                       ply_window_backspace_handler_t backspace_handler,
                                       void         *user_data);
void ply_window_set_escape_handler (ply_window_t *window,
                                    ply_window_escape_handler_t escape_handler,
                                    void         *user_data);
void ply_window_set_enter_handler (ply_window_t *window,
                                   ply_window_enter_handler_t enter_handler,
                                   void         *user_data);

bool ply_window_open (ply_window_t *window);
bool ply_window_take_console (ply_window_t *window);
bool ply_window_give_console (ply_window_t *window,
                              int           vt_number);
void ply_window_close (ply_window_t *window);
bool ply_window_set_mode (ply_window_t      *window,
                          ply_window_mode_t  mode);
int  ply_window_get_number_of_text_rows (ply_window_t *window);
int  ply_window_get_number_of_text_columns (ply_window_t *window);
void ply_window_set_text_cursor_position (ply_window_t *window,
                                          int           column,
                                          int           row);

void ply_window_attach_to_event_loop (ply_window_t     *window,
                                      ply_event_loop_t *loop);
ply_frame_buffer_t *ply_window_get_frame_buffer (ply_window_t *window);

#endif

#endif /* PLY_WINDOW_H */
/* vim: set ts=4 sw=4 expandtab autoindent cindent cino={.5s,(0: */
