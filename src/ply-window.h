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

#include "ply-event-loop.h"

typedef struct _ply_window ply_window_t;

typedef enum
{
  PLY_WINDOW_MODE_TEXT,
  PLY_WINDOW_MODE_GRAPHICS
} ply_window_mode_t;

#ifndef PLY_HIDE_FUNCTION_DECLARATIONS
ply_window_t *ply_window_new (const char *tty_name);
void ply_window_free (ply_window_t *window);

bool ply_window_open (ply_window_t *window);
void ply_window_close (ply_window_t *window);
bool ply_window_set_mode (ply_window_t      *window,
                          ply_window_mode_t  mode);

void ply_window_attach_to_event_loop (ply_window_t     *window,
                                      ply_event_loop_t *loop);

#endif

#endif /* PLY_WINDOW_H */
/* vim: set ts=4 sw=4 expandtab autoindent cindent cino={.5s,(0: */
