/* ply-event-loop.h - small epoll based event loop
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
#ifndef PLY_EVENT_LOOP_H
#define PLY_EVENT_LOOP_H

#include <stdbool.h>

typedef struct _ply_event_loop ply_event_loop_t;

typedef void (* ply_event_handler_t) (void *user_data,
                                      int   source);

#ifndef PLY_HIDE_FUNCTION_DECLARATIONS
ply_event_loop_t *ply_event_loop_new (void);
void ply_event_loop_free (ply_event_loop_t *loop);
bool ply_event_loop_watch_fd (ply_event_loop_t *loop,
                              int               fd,
                              ply_event_handler_t new_data_handler,
                              ply_event_handler_t disconnected_handler,
                              void          *user_data);
void ply_event_loop_stop_watching_fd (ply_event_loop_t *loop, 
		                      int               fd);
bool ply_event_loop_watch_signal (ply_event_loop_t     *loop,
                                  int                   signal_number,
                                  ply_event_handler_t   signal_handler,
                                  void                  *user_data);
void ply_event_loop_stop_watching_signal (ply_event_loop_t *loop,
                                          int               signal_number);

int ply_event_loop_run (ply_event_loop_t *loop);
void ply_event_loop_exit (ply_event_loop_t *loop,
                          int               exit_code);
#endif

#endif
