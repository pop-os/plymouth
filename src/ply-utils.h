/* ply-utils.h - random useful functions and macros
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
#ifndef PLY_UTILS_H
#define PLY_UTILS_H

#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>

#ifndef MIN
#define MIN(a,b) ((a) <= (b)? (a) : (b))
#endif

#ifndef MAX
#define MAX(a,b) ((a) >= (b)? (a) : (b))
#endif

#ifndef CLAMP
#define CLAMP(a,b,c) (MIN (MAX ((a), (b)), (c)))
#endif

#ifndef PLY_HIDE_FUNCTION_DECLARATIONS
bool ply_open_unidirectional_pipe (int *sender_fd,
                                   int *receiver_fd);
bool ply_write (int         fd,
                const void *buffer,
                size_t      number_of_bytes); 
bool ply_read (int     fd,
               void   *buffer,
               size_t  number_of_bytes); 
ssize_t ply_read_chunk (int     fd,
                        void   *chunk);

bool ply_fd_has_data (int fd);
bool ply_fd_can_take_data (int fd);
bool ply_fd_may_block (int fd);
char **ply_copy_string_array (const char * const *array);
void ply_free_string_array (char **array);
void ply_close_all_fds (void);
double ply_get_timestamp (void);
#endif

#endif /* PLY_UTILS_H */
/* vim: set ts=4 sw=4 expandtab autoindent cindent cino={.5s,(0: */
