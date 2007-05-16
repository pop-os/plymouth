/* ply-init-control.h - framecontrol abstraction
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
 * Written By: Ray Strode <rstrode@redhat.com>
 */
#ifndef PLY_INIT_CONTROL_H
#define PLY_INIT_CONTROL_H

#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>

typedef struct _ply_init_control ply_init_control_t;

#ifndef PLY_HIDE_FUNCTION_DECLARATIONS
ply_init_control_t *ply_init_control_new (void);
void ply_init_control_free (ply_init_control_t *init_control);
bool ply_init_control_is_open (ply_init_control_t *control);
bool ply_init_control_open (ply_init_control_t *init_control);
void ply_init_control_close (ply_init_control_t *init_control);
bool ply_init_control_trap_messages (ply_init_control_t *init_control);
void ply_init_control_untrap_messages (ply_init_control_t *init_control);
int ply_init_control_get_messages_fd (ply_init_control_t *init_control);
#endif

#endif /* PLY_INIT_CONTROL_H */
/* vim: set ts=4 sw=4 expandtab autoindent cindent cino={.5s,(0: */
