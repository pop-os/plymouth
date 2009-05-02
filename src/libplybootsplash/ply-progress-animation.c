/* progress-animation.c - boot progress animation
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
 * Written by: William Jon McCann <jmccann@redhat.com>
 *
 */
#include "config.h"

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <values.h>
#include <unistd.h>
#include <wchar.h>

#include "ply-progress-animation.h"
#include "ply-array.h"
#include "ply-logger.h"
#include "ply-frame-buffer.h"
#include "ply-image.h"
#include "ply-utils.h"
#include "ply-window.h"

#include <linux/kd.h>

struct _ply_progress_animation
{
  ply_array_t *frames;
  char *image_dir;
  char *frames_prefix;

  ply_window_t            *window;
  ply_frame_buffer_t      *frame_buffer;
  ply_frame_buffer_area_t  area;
  ply_frame_buffer_area_t  frame_area;

  double percent_done;

  uint32_t is_hidden : 1;
};

ply_progress_animation_t *
ply_progress_animation_new (const char *image_dir,
                    const char *frames_prefix)
{
  ply_progress_animation_t *progress_animation;

  assert (image_dir != NULL);
  assert (frames_prefix != NULL);

  progress_animation = calloc (1, sizeof (ply_progress_animation_t));

  progress_animation->frames = ply_array_new ();
  progress_animation->frames_prefix = strdup (frames_prefix);
  progress_animation->image_dir = strdup (image_dir);
  progress_animation->is_hidden = true;
  progress_animation->percent_done = 0.0;
  progress_animation->area.x = 0;
  progress_animation->area.y = 0;
  progress_animation->area.width = 0;
  progress_animation->area.height = 0;
  progress_animation->frame_area.x = 0;
  progress_animation->frame_area.y = 0;
  progress_animation->frame_area.width = 0;
  progress_animation->frame_area.height = 0;

  return progress_animation;
}

static void
ply_progress_animation_remove_frames (ply_progress_animation_t *progress_animation)
{
  int i;
  ply_image_t **frames;

  frames = (ply_image_t **) ply_array_steal_elements (progress_animation->frames);
  for (i = 0; frames[i] != NULL; i++)
    ply_image_free (frames[i]);
  free (frames);
}

void
ply_progress_animation_free (ply_progress_animation_t *progress_animation)
{
  if (progress_animation == NULL)
    return;

  ply_progress_animation_remove_frames (progress_animation);
  ply_array_free (progress_animation->frames);

  free (progress_animation->frames_prefix);
  free (progress_animation->image_dir);
  free (progress_animation);
}

static void
draw_background (ply_progress_animation_t *progress_animation)
{
  ply_window_erase_area (progress_animation->window,
                         progress_animation->area.x, progress_animation->area.y,
                         progress_animation->frame_area.width,
                         progress_animation->frame_area.height);
}

void
ply_progress_animation_draw (ply_progress_animation_t *progress_animation)
{
  int number_of_frames;
  int frame_number;
  ply_image_t * const * frames;
  uint32_t *frame_data;

  if (progress_animation->is_hidden)
    return;

  ply_window_set_mode (progress_animation->window, PLY_WINDOW_MODE_GRAPHICS);

  number_of_frames = ply_array_get_size (progress_animation->frames);

  if (number_of_frames == 0)
    return;

  frame_number = progress_animation->percent_done * (number_of_frames - 1);

  ply_frame_buffer_pause_updates (progress_animation->frame_buffer);
  if (progress_animation->frame_area.width > 0)
    draw_background (progress_animation);

  frames = (ply_image_t * const *) ply_array_get_elements (progress_animation->frames);

  progress_animation->frame_area.x = progress_animation->area.x;
  progress_animation->frame_area.y = progress_animation->area.y;
  progress_animation->frame_area.width = ply_image_get_width (frames[frame_number]);
  progress_animation->frame_area.height = ply_image_get_height (frames[frame_number]);
  frame_data = ply_image_get_data (frames[frame_number]);

  ply_frame_buffer_fill_with_argb32_data (progress_animation->frame_buffer,
                                          &progress_animation->frame_area, 0, 0,
                                          frame_data);

  ply_frame_buffer_unpause_updates (progress_animation->frame_buffer);
}

static bool
ply_progress_animation_add_frame (ply_progress_animation_t *progress_animation,
                                  const char               *filename)
{
  ply_image_t *image;

  image = ply_image_new (filename);

  if (!ply_image_load (image))
    {
      ply_image_free (image);
      return false;
    }

  ply_array_add_element (progress_animation->frames, image);

  progress_animation->area.width = MAX (progress_animation->area.width, ply_image_get_width (image));
  progress_animation->area.height = MAX (progress_animation->area.height, ply_image_get_height (image));

  return true;
}

static bool
ply_progress_animation_add_frames (ply_progress_animation_t *progress_animation)
{
  struct dirent **entries;
  int number_of_entries;
  int i;
  bool load_finished;

  entries = NULL;

  number_of_entries = scandir (progress_animation->image_dir, &entries, NULL, versionsort);

  if (number_of_entries < 0)
    return false;

  load_finished = false;
  for (i = 0; i < number_of_entries; i++)
    {
      if (strncmp (entries[i]->d_name,
                   progress_animation->frames_prefix,
                   strlen (progress_animation->frames_prefix)) == 0
          && (strlen (entries[i]->d_name) > 4)
          && strcmp (entries[i]->d_name + strlen (entries[i]->d_name) - 4, ".png") == 0)
        {
          char *filename;

          filename = NULL;
          asprintf (&filename, "%s/%s", progress_animation->image_dir, entries[i]->d_name);

          if (!ply_progress_animation_add_frame (progress_animation, filename))
            goto out;

          free (filename);
        }

      free (entries[i]);
      entries[i] = NULL;
    }
  load_finished = true;

out:
  if (!load_finished)
    {
      ply_progress_animation_remove_frames (progress_animation);

      while (entries[i] != NULL)
        {
          free (entries[i]);
          i++;
        }
    }
  free (entries);

  return load_finished;
}

bool
ply_progress_animation_load (ply_progress_animation_t *progress_animation)
{
  if (ply_array_get_size (progress_animation->frames) != 0)
    ply_progress_animation_remove_frames (progress_animation);

  if (!ply_progress_animation_add_frames (progress_animation))
    return false;

  return true;
}

void
ply_progress_animation_show (ply_progress_animation_t *progress_animation,
                             ply_window_t             *window,
                             long                      x,
                             long                      y)
{
  assert (progress_animation != NULL);

  progress_animation->window = window;
  progress_animation->frame_buffer = ply_window_get_frame_buffer (window);;

  progress_animation->area.x = x;
  progress_animation->area.y = y;

  progress_animation->is_hidden = false;
  ply_progress_animation_draw (progress_animation);
}

void
ply_progress_animation_hide (ply_progress_animation_t *progress_animation)
{
  if (progress_animation->is_hidden)
    return;

  if (progress_animation->frame_area.width > 0)
    draw_background (progress_animation);

  progress_animation->frame_buffer = NULL;
  progress_animation->window = NULL;

  progress_animation->is_hidden = true;
}

bool
ply_progress_animation_is_hidden (ply_progress_animation_t *progress_animation)
{
  return progress_animation->is_hidden;
}

long
ply_progress_animation_get_width (ply_progress_animation_t *progress_animation)
{
  return progress_animation->area.width;
}

long
ply_progress_animation_get_height (ply_progress_animation_t *progress_animation)
{
  return progress_animation->area.height;
}

void
ply_progress_animation_set_percent_done (ply_progress_animation_t *progress_animation,
                                 double            percent_done)
{
  progress_animation->percent_done = percent_done;
}

double
ply_progress_animation_get_percent_done (ply_progress_animation_t *progress_animation)
{
  return progress_animation->percent_done;
}

/* vim: set ts=4 sw=4 expandtab autoindent cindent cino={.5s,(0: */
