/* ply-progress.c - calculats boot progress 
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
 *             Soeren Sandmann <sandmann@redhat.com>
 *             Charlie Brej <cbrej@cs.man.ac.uk>
 */

#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>


#include "ply-logger.h"
#include "ply-progress.h"
#include "ply-utils.h"

#ifndef UPDATES_PER_SECOND
#define UPDATES_PER_SECOND 30
#endif

#ifndef DEFAULT_BOOT_DURATION
#define DEFAULT_BOOT_DURATION 60.0
#endif

#define BOOT_DURATION_FILE PLYMOUTH_TIME_DIRECTORY "/boot-duration"


struct _ply_progress
{
  double boot_duration;
  double start_time;
  double wait_time;
};

ply_progress_t*
ply_progress_new (void)
{
  ply_progress_t *progress = calloc (1, sizeof (ply_progress_t));
  
  progress->boot_duration = DEFAULT_BOOT_DURATION;
  progress->start_time = ply_get_timestamp ();
  progress->wait_time=0;
  
  return progress;
}

void
ply_progress_free (ply_progress_t* progress)
{
  free(progress);
  return;
}


void
ply_progress_load_cache (ply_progress_t* progress)
{
  FILE *fp;
  int items_matched;

  fp = fopen (BOOT_DURATION_FILE,"r"); 

  if (fp == NULL)
    return;

  items_matched = fscanf (fp, "%lf", &progress->boot_duration);

  fclose (fp);

  if (items_matched != 1)
    progress->boot_duration = DEFAULT_BOOT_DURATION;
}

void
ply_progress_save_cache (ply_progress_t* progress)
{
  FILE *fp;
  fp = fopen (BOOT_DURATION_FILE,"w");
  if (fp != NULL)
    {
      fprintf (fp, "%.1lf\n", (ply_get_timestamp () - progress->start_time));
      fclose (fp);
    }
}


double
ply_progress_get_percentage (ply_progress_t* progress)
{
  return CLAMP((ply_get_timestamp() - progress->start_time)/progress->boot_duration, 0, 1);
  
}


double
ply_progress_get_time (ply_progress_t* progress)
{
  return ply_get_timestamp() - progress->start_time;
}

void
ply_progress_pause (ply_progress_t* progress)
{
  progress->wait_time = ply_get_timestamp ();
  return;
}


void
ply_progress_unpause (ply_progress_t* progress)
{
  progress->start_time += ply_get_timestamp() - progress->wait_time;
  return;
}

void
ply_progress_session_output (ply_progress_t* progress,
                             const char *output,
                             size_t      size)
{
  return;
}



#ifdef PLY_PROGRESS_ENABLE_TEST

#include <stdio.h>

int
main (int    argc,
      char **argv)
{
  double percent;
  double time;
  int i;
  ply_progress_t* progress = ply_progress_new ();

  percent = ply_progress_get_percentage (progress);
  time = ply_progress_get_time (progress);
  printf("Time:%f   \t Percentage: %f%%\n", time, percent);
  srand ((int) ply_get_timestamp ());

  for (i=0; i<10; i++)
    {
      usleep ((rand () % 500000));
      percent = ply_progress_get_percentage (progress);
      time = ply_progress_get_time (progress);
      printf("Time:%f   \t Percentage: %f%%\n", time, percent);
    }
  printf("Load cache\n");
  ply_progress_load_cache (progress);

  for (i=0; i<10; i++)
    {
      usleep ((rand () % 500000));
      percent = ply_progress_get_percentage (progress);
      time = ply_progress_get_time (progress);
      printf("Time:%f   \t Percentage: %f%%\n", time, percent);
    }

  printf("Save and reload cache\n");
  ply_progress_save_cache (progress);

  ply_progress_load_cache (progress);

  percent = ply_progress_get_percentage (progress);
  time = ply_progress_get_time (progress);
  printf("Time:%f   \t Percentage: %f%%\n", time, percent);

  ply_progress_free(progress);
  return 0;
}

#endif /* PLY_PROGRESS_ENABLE_TEST */
/* vim: set ts=4 sw=4 expandtab autoindent cindent cino={.5s,(0: */
