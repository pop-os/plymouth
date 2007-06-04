/* ply-boot-server.h - APIs for talking to the boot status daemon
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
#ifndef PLY_BOOT_SERVER_H
#define PLY_BOOT_SERVER_H

#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>

#include "ply-boot-protocol.h"
#include "ply-event-loop.h"

typedef struct _ply_boot_server ply_boot_server_t;

#ifndef PLY_HIDE_FUNCTION_DECLARATIONS
ply_boot_server_t *ply_boot_server_new (void);
                                                  
void ply_boot_server_free (ply_boot_server_t *server);
bool ply_boot_server_listen (ply_boot_server_t *server);
void ply_boot_server_stop_listening (ply_boot_server_t *server);
void ply_boot_server_attach_to_event_loop (ply_boot_server_t *server,
                                           ply_event_loop_t  *loop);

#endif

#endif /* PLY_BOOT_SERVER_H */
/* vim: set ts=4 sw=4 expandtab autoindent cindent cino={.5s,(0: */
