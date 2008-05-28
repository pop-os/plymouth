/* throbber.h - simple throbber animation
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
#ifndef THROBBER_H
#define THROBBER_H

#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>

#include "ply-event-loop.h"
#include "ply-frame-buffer.h"

typedef struct _throbber throbber_t;

#ifndef PLY_HIDE_FUNCTION_DECLARATIONS
throbber_t *throbber_new (const char *image_dir,
                          const char *frames_prefix);
void throbber_free (throbber_t *throbber);

bool throbber_start (throbber_t         *throbber,
                     ply_event_loop_t   *loop,
                     ply_frame_buffer_t *frame_buffer,
                     long                x,
                     long                y);
void throbber_stop (throbber_t *throbber);

long throbber_get_width (throbber_t *throbber);
long throbber_get_height (throbber_t *throbber);
#endif

#endif /* THROBBER_H */
/* vim: set ts=4 sw=4 expandtab autoindent cindent cino={.5s,(0: */
