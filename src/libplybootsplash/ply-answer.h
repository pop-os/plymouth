/* ply-answer.h - Object that takes a string and triggers a closure
 *                to use the string
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
#ifndef PLY_ANSWER_H
#define PLY_ANSWER_H

#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>

#include "ply-buffer.h"
#include "ply-event-loop.h"
#include "ply-frame-buffer.h"

typedef struct _ply_answer ply_answer_t;

typedef void (* ply_answer_handler_t) (void         *user_data,
                                       const char   *string,
                                       ply_answer_t *answer);

#ifndef PLY_HIDE_FUNCTION_DECLARATIONS
ply_answer_t *ply_answer_new (ply_answer_handler_t  handler,
                              void                 *user_data);
void ply_answer_free (ply_answer_t *answer);

void ply_answer_with_string (ply_answer_t *answer,
                             const char   *string);

void ply_answer_unknown (ply_answer_t *answer);
char *ply_answer_get_string (ply_answer_t *answer);
#endif

#endif /* PLY_ANSWER_H */
/* vim: set ts=4 sw=4 expandtab autoindent cindent cino={.5s,(0: */
