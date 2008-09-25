/* ply-text-progress-bar.c -  simple text based progress bar
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
 * Written by: Adam Jackson <ajax@redhat.com>
 *             Bill Nottingham <notting@redhat.com>
 *             Ray Strode <rstrode@redhat.com>
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

#include "ply-text-progress-bar.h"
#include "ply-event-loop.h"
#include "ply-array.h"
#include "ply-logger.h"
#include "ply-utils.h"
#include "ply-window.h"

#include <linux/kd.h>

#ifndef FRAMES_PER_SECOND
#define FRAMES_PER_SECOND 5
#endif

#define NUMBER_OF_INDICATOR_COLUMNS 6

#define OS_STRING " "
char *os_string = OS_STRING;

struct _ply_text_progress_bar
{
  ply_event_loop_t *loop;

  ply_window_t            *window;

  int column, row;
  int number_of_rows;
  int number_of_columns;
  int spinner_position;
  double start_time, now;
};

ply_text_progress_bar_t *
ply_text_progress_bar_new (void)
{
  ply_text_progress_bar_t *progress_bar;

  progress_bar = calloc (1, sizeof (ply_text_progress_bar_t));

  progress_bar->row = 0;
  progress_bar->column = 0;
  progress_bar->spinner_position = 0;
  progress_bar->number_of_columns = 0;
  progress_bar->number_of_rows = 0;

  return progress_bar;
}

void
ply_text_progress_bar_free (ply_text_progress_bar_t *progress_bar)
{
  if (progress_bar == NULL)
    return;

  free (progress_bar);
}

static void
get_os_string (void)
{
   int fd;
   char *buf, *pos, *pos2;
   struct stat sbuf;
   
   fd = open("/etc/system-release", O_RDONLY);
   if (fd == -1) return;
   if (fstat(fd, &sbuf) == -1) return;
   buf = calloc(sbuf.st_size + 1, sizeof(char));
   read(fd, buf, sbuf.st_size);
   close(fd);
   
   pos = strstr(buf, " release ");
   if (!pos) goto out;
   pos2 = strstr(pos, " (");
   if (!pos2) goto out;
   *pos = '\0';
   pos+= 9;
   *pos2 = '\0';
   asprintf(&os_string," %s %s", buf, pos);
out:
   free(buf);
}

/* Hi Will! */
static double
woodsify(double time, double estimate)
{
    return 1.0 - pow(2.0, -pow(time, 1.45) / estimate);
}

#define STARTUP_TIME 20.0

static void
animate_at_time (ply_text_progress_bar_t *progress_bar,
                 double             time)
{
    int i, width = progress_bar->number_of_columns - 2 - strlen(os_string);
    double brown_fraction, blue_fraction, white_fraction;

    ply_window_set_mode (progress_bar->window, PLY_WINDOW_MODE_TEXT);
    ply_window_set_text_cursor_position(progress_bar->window,
					progress_bar->column,
					progress_bar->row);

    brown_fraction = woodsify(time, STARTUP_TIME);
    blue_fraction  = woodsify(time, STARTUP_TIME / brown_fraction);
    white_fraction = woodsify(time, STARTUP_TIME / blue_fraction);

    for (i = 0; i < width; i += 1) {
	double f = (double)i / (double)width;
	if (f < white_fraction)
	    ply_window_set_background_color (progress_bar->window,
					     PLY_WINDOW_COLOR_WHITE);
	else if (f < blue_fraction)
	    ply_window_set_background_color (progress_bar->window,
					     PLY_WINDOW_COLOR_BLUE);
	else if (f < brown_fraction)
	    ply_window_set_background_color (progress_bar->window,
					     PLY_WINDOW_COLOR_BROWN);
	else break;

	write (STDOUT_FILENO, " ", strlen (" "));
    }

    ply_window_set_background_color (progress_bar->window, PLY_WINDOW_COLOR_BLACK);

    if (brown_fraction > 0.5) {
	if (white_fraction > 0.875)
	    ply_window_set_foreground_color (progress_bar->window,
					     PLY_WINDOW_COLOR_WHITE);
	else if (blue_fraction > 0.66)
	    ply_window_set_foreground_color (progress_bar->window,
					     PLY_WINDOW_COLOR_BLUE);
	else
	    ply_window_set_foreground_color (progress_bar->window,
					     PLY_WINDOW_COLOR_BROWN);

	ply_window_set_text_cursor_position(progress_bar->window,
					    progress_bar->column + width,
					    progress_bar->row);


	write (STDOUT_FILENO, os_string, strlen(os_string));
	
	ply_window_set_foreground_color (progress_bar->window,
					 PLY_WINDOW_COLOR_DEFAULT);
    }
}

static void
on_timeout (ply_text_progress_bar_t *progress_bar)
{
  double sleep_time;
  progress_bar->now = ply_get_timestamp ();

#ifdef REAL_TIME_ANIMATION
  animate_at_time (progress_bar,
                   progress_bar->now - progress_bar->start_time);
#else
  static double time = 0.0;
  time += 1.0 / FRAMES_PER_SECOND;
  animate_at_time (progress_bar, time);
#endif

  sleep_time = 1.0 / FRAMES_PER_SECOND;
  sleep_time = MAX (sleep_time - (ply_get_timestamp () - progress_bar->now),
                    0.005);

  ply_event_loop_watch_for_timeout (progress_bar->loop,
                                    sleep_time,
                                    (ply_event_loop_timeout_handler_t)
                                    on_timeout, progress_bar);
}

bool
ply_text_progress_bar_start (ply_text_progress_bar_t  *progress_bar,
                       ply_event_loop_t   *loop,
                       ply_window_t       *window)
{
  assert (progress_bar != NULL);
  assert (progress_bar->loop == NULL);

  progress_bar->loop = loop;
  progress_bar->window = window;

  progress_bar->number_of_rows = ply_window_get_number_of_text_rows(window);
  progress_bar->row = progress_bar->number_of_rows - 1;
  progress_bar->number_of_columns = ply_window_get_number_of_text_columns(window);
  progress_bar->column = 2;

  progress_bar->start_time = ply_get_timestamp ();
  
  get_os_string ();

  ply_event_loop_watch_for_timeout (progress_bar->loop,
                                    1.0 / FRAMES_PER_SECOND,
                                    (ply_event_loop_timeout_handler_t)
                                    on_timeout, progress_bar);

  return true;
}

void
ply_text_progress_bar_stop (ply_text_progress_bar_t *progress_bar)
{
  progress_bar->window = NULL;

  if (progress_bar->loop != NULL)
    {
      ply_event_loop_stop_watching_for_timeout (progress_bar->loop,
                                                (ply_event_loop_timeout_handler_t)
                                                on_timeout, progress_bar);
      progress_bar->loop = NULL;
    }
}

int
ply_text_progress_bar_get_number_of_columns (ply_text_progress_bar_t *progress_bar)
{
  return progress_bar->number_of_columns;
}

int
ply_text_progress_bar_get_number_of_rows (ply_text_progress_bar_t *progress_bar)
{
  return progress_bar->number_of_rows;
}

/* vim: set ts=4 sw=4 expandtab autoindent cindent cino={.5s,(0: */
