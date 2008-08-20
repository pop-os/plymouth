/* ply-answer.h - Object that takes a string and triggers a closure
 *                to use the string
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
#include "config.h"
#include "ply-answer.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "ply-logger.h"
#include "ply-utils.h"

struct _ply_answer
{
  ply_answer_handler_t  handler;
  void                 *user_data;
  char                 *string;
};

ply_answer_t *
ply_answer_new (ply_answer_handler_t  handler,
                void                 *user_data)
{
  ply_answer_t *answer;

  answer = calloc (1, sizeof (ply_answer_t));
  answer->handler = handler;
  answer->user_data = user_data;

  return answer;
}

void
ply_answer_free (ply_answer_t *answer)
{
  if (answer == NULL)
    return;

  free (answer->string);
  free (answer);
}

void
ply_answer_with_string (ply_answer_t *answer,
                        const char   *string)
{
  assert (answer != NULL);

  answer->string = strdup (string);

  if (answer->handler != NULL)
    answer->handler (answer->user_data, string, answer);
}

char *
ply_answer_get_string (ply_answer_t *answer)
{
  return strdup (answer->string);
}

void
ply_answer_unknown (ply_answer_t *answer)
{
  assert (answer != NULL);

  if (answer->handler != NULL)
    answer->handler (answer->user_data, NULL, answer);
}

#ifdef PLY_ANSWER_ENABLE_TEST

#include <stdio.h>

#include "ply-event-loop.h"
#include "ply-answer.h"

static void
on_timeout (ply_answer_t     *answer,
            ply_event_loop_t *loop)
{
  ply_event_loop_exit (loop, 0);
}

static void
on_keypress (ply_answer_t *answer,
             const char   *keyboard_input)
{
  printf ("key '%c' (0x%x) was pressed\n",
          keyboard_input[0], (unsigned int) keyboard_input[0]);
}

int
main (int    argc,
      char **argv)
{
  ply_event_loop_t *loop;
  ply_answer_t *answer;
  int exit_code;
  const char *tty_name;

  exit_code = 0;

  loop = ply_event_loop_new ();

  if (argc > 1)
    tty_name = argv[1];
  else
    tty_name = "/dev/tty1";

  answer = ply_answer_new (tty_name);
  ply_answer_attach_to_event_loop (answer, loop);
  ply_answer_set_keyboard_input_handler (answer,
                                         (ply_answer_keyboard_input_handler_t)
                                         on_keypress, answer);

  if (!ply_answer_open (answer))
    {
      ply_save_errno ();
      perror ("could not open answer");
      ply_restore_errno ();
      return errno;
    }

  if (!ply_answer_set_mode (answer, PLY_ANSWER_MODE_TEXT))
    {
      ply_save_errno ();
      perror ("could not set answer for graphics mode");
      ply_restore_errno ();
    }

  ply_event_loop_watch_for_timeout (loop,
                                    15.0,
                                   (ply_event_loop_timeout_handler_t)
                                   on_timeout,
                                   answer);
  exit_code = ply_event_loop_run (loop);

  ply_answer_close (answer);
  ply_answer_free (answer);

  return exit_code;
}

#endif /* PLY_ANSWER_ENABLE_TEST */
/* vim: set ts=4 sw=4 expandtab autoindent cindent cino={.5s,(0: */
