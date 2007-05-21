/* ply-terminal-session.h - api for spawning a program in pseudo-terminal
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
#ifndef PLY_TERMINAL_SESSION_H
#define PLY_TERMINAL_SESSION_H

#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>

typedef struct _ply_terminal_session ply_terminal_session_t;

typedef enum
{
  PLY_TERMINAL_SESSION_FLAGS_NONE = 0x0,
  PLY_TERMINAL_SESSION_FLAGS_RUN_IN_PARENT = 0x1,
  PLY_TERMINAL_SESSION_FLAGS_LOOK_IN_PATH = 0x2
} ply_terminal_session_flags_t;

#ifndef PLY_HIDE_FUNCTION_DECLARATIONS
ply_terminal_session_t *ply_terminal_session_new (const char * const *argv);
                                                  
void ply_terminal_session_free (ply_terminal_session_t *session);
bool ply_terminal_session_run (ply_terminal_session_t *session,
                               ply_terminal_session_flags_t flags);
int ply_terminal_session_get_fd (ply_terminal_session_t *session);
#endif

#endif /* PLY_TERMINAL_SESSION_H */
/* vim: set ts=4 sw=4 expandtab autoindent cindent cino={.5s,(0: */
