/* ply-trigger.c - Calls closure at later time.
 *
 * Copyright (C) 2008 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Written by: Ray Strode <rstrode@redhat.com>
 */
#include "config.h"
#include "ply-trigger.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "ply-logger.h"
#include "ply-utils.h"

struct _ply_trigger
{
  ply_trigger_handler_t  handler;
  void                  *user_data;
  ply_trigger_t         **free_address;
};

ply_trigger_t *
ply_trigger_new (ply_trigger_handler_t  handler,
                 void                 *user_data,
                 ply_trigger_t       **free_address)
{
  ply_trigger_t *trigger;

  trigger = calloc (1, sizeof (ply_trigger_t));
  trigger->handler = handler;
  trigger->user_data = user_data;
  trigger->free_address = free_address;

  return trigger;
}

void
ply_trigger_free (ply_trigger_t *trigger)
{
  if (trigger == NULL)
    return;

  if (trigger->free_address != NULL)
    *trigger->free_address = NULL;

  free (trigger);
}

void
ply_trigger_pull (ply_trigger_t *trigger,
                  const void    *data)
{
  assert (trigger != NULL);

  if (trigger->handler != NULL)
    trigger->handler (trigger->user_data, data, trigger);

  if (trigger->free_address != NULL)
    ply_trigger_free (trigger);
}
/* vim: set ts=4 sw=4 expandtab autoindent cindent cino={.5s,(0: */
