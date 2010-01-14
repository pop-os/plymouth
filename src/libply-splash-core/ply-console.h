/* ply-console.h - APIs for consoleing text
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
 * Written By: Ray Strode <rstrode@redhat.com>
 */
#ifndef PLY_CONSOLE_H
#define PLY_CONSOLE_H

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>

typedef struct _ply_console ply_console_t;
typedef void (* ply_console_active_vt_changed_handler_t) (void           *user_data,
                                                          ply_console_t *console);

typedef enum
{
  PLY_CONSOLE_MODE_TEXT,
  PLY_CONSOLE_MODE_GRAPHICS
} ply_console_mode_t;

#ifndef PLY_HIDE_FUNCTION_DECLARATIONS
ply_console_t *ply_console_new (void);

void ply_console_free (ply_console_t *console);

bool ply_console_open (ply_console_t *console);
bool ply_console_is_open (ply_console_t *console);
void ply_console_close (ply_console_t *console);

void ply_console_set_mode (ply_console_t     *console,
                           ply_console_mode_t mode);

void ply_console_ignore_mode_changes (ply_console_t *console,
                                      bool           should_ignore);

int ply_console_get_fd (ply_console_t *console);
int ply_console_get_active_vt (ply_console_t *console);
bool ply_console_set_active_vt (ply_console_t *console,
                                int            vt_number);

void ply_console_watch_for_active_vt_change (ply_console_t *console,
                                             ply_console_active_vt_changed_handler_t active_vt_changed_handler,
                                             void *user_data);
void ply_console_stop_watching_for_active_vt_change (ply_console_t *console,
                                             ply_console_active_vt_changed_handler_t active_vt_changed_handler,
                                             void *user_data);

#endif

#endif /* PLY_CONSOLE_H */
/* vim: set ts=4 sw=4 et ai ci cino={.5s,^-2,+.5s,t0,g0,e-2,n-2,p2s,(0,=.5s,:.5s */
