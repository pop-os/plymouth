/* ply-event-loop.c - small epoll based event loop
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
#include "ply-event-loop.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>

#include "ply-logger.h"
#include "ply-list.h"
#include "ply-utils.h"

#ifndef PLY_EVENT_LOOP_NUM_EVENT_HANDLERS 
#define PLY_EVENT_LOOP_NUM_EVENT_HANDLERS 64
#endif

typedef void (* ply_event_loop_free_handler_t) (void *);

typedef struct
{
  int fd;                                 
  ply_event_handler_t new_data_handler; 
  ply_event_handler_t disconnected_handler;
  void *user_data;
  ply_event_loop_free_handler_t free_function; 
} ply_event_source_t;

typedef struct
{
  int signal_number;
  ply_event_handler_t handler;
  void *user_data;

  sighandler_t old_posix_signal_handler;
} ply_signal_source_t;

static int ply_signal_dispatcher_sender_fd = -1,
           ply_signal_dispatcher_receiver_fd = -1;

typedef struct
{
  ply_list_t *sources;
} ply_signal_dispatcher_t;

typedef struct
{
  ply_event_loop_exit_handler_t  handler;
  void                          *user_data;
} ply_event_loop_exit_closure_t;

struct _ply_event_loop
{
  int epoll_fd;                      
  int exit_code;                    

  ply_list_t *sources;
  ply_list_t *exit_closures;

  ply_signal_dispatcher_t *signal_dispatcher; 

  uint32_t should_exit : 1; 
};

static void ply_event_loop_process_pending_events (ply_event_loop_t *loop);
static void ply_event_loop_remove_source (ply_event_loop_t    *loop,
                                          ply_event_source_t *source);
ply_list_node_t *ply_event_loop_find_source_node (ply_event_loop_t *loop,
                                                  int               fd);

ply_list_node_t *
ply_signal_dispatcher_find_source_node (ply_signal_dispatcher_t *dispatcher,
                                        int                      signal_number);


static ply_signal_source_t *
ply_signal_source_new (int                  signal_number,
                       ply_event_handler_t  signal_handler,
                       void                *user_data)
{
  ply_signal_source_t *source;

  source = calloc (1, sizeof (ply_signal_source_t));
  source->signal_number = signal_number;
  source->handler = signal_handler;
  source->user_data = user_data;
  source->old_posix_signal_handler = NULL;

  return source;
}

static void
ply_signal_source_free (ply_signal_source_t *handler)
{
  if (handler == NULL)
    return;

  free (handler);
}
 
static ply_signal_dispatcher_t *
ply_signal_dispatcher_new (void)
{
  ply_signal_dispatcher_t *dispatcher;
 
  if (!ply_open_unidirectional_pipe (&ply_signal_dispatcher_sender_fd,
                                     &ply_signal_dispatcher_receiver_fd))
    return NULL;

  dispatcher = calloc (1, sizeof (ply_signal_dispatcher_t));

  dispatcher->sources = ply_list_new ();

  return dispatcher;
}

static void
ply_signal_dispatcher_free (ply_signal_dispatcher_t *dispatcher)
{
  ply_list_node_t *node;

  if (dispatcher == NULL)
    return;

  close (ply_signal_dispatcher_receiver_fd);
  ply_signal_dispatcher_receiver_fd = -1;
  close (ply_signal_dispatcher_sender_fd);
  ply_signal_dispatcher_sender_fd = -1;

  node = ply_list_get_first_node (dispatcher->sources);
  while (node != NULL)
    {
      ply_list_node_t *next_node;
      ply_signal_source_t *source;

      source = (ply_signal_source_t *) ply_list_node_get_data (node);

      next_node = ply_list_get_next_node (dispatcher->sources, node);

      ply_signal_source_free (source);

      node = next_node;
    }

  ply_list_free (dispatcher->sources);

  free (dispatcher);
}

static void
ply_signal_dispatcher_posix_signal_handler (int signal_number)
{
  if (ply_signal_dispatcher_sender_fd < 0)
    return;

  ply_write (ply_signal_dispatcher_sender_fd, &signal_number, 
             sizeof (signal_number));
}

static int
ply_signal_dispatcher_get_next_signal_from_pipe (ply_signal_dispatcher_t *dispatcher)
{
  int signal_number;

  if (!ply_read (ply_signal_dispatcher_receiver_fd, &signal_number, 
                 sizeof (signal_number)))
    signal_number = 0;

  return signal_number;
}

static void
ply_signal_dispatcher_dispatch_signal (ply_signal_dispatcher_t *dispatcher,
                                       int                      fd)
{
  ply_list_node_t *node;
  int signal_number;

  assert (fd == ply_signal_dispatcher_receiver_fd);

  signal_number = ply_signal_dispatcher_get_next_signal_from_pipe (dispatcher);

  node = ply_list_get_first_node (dispatcher->sources);
  while (node != NULL)
    {
      ply_signal_source_t *source;

      source = (ply_signal_source_t *) ply_list_node_get_data (node);

      if (source->signal_number == signal_number) 
        {
          if (source->handler != NULL)
            source->handler (source->user_data, signal_number);
        }

      node = ply_list_get_next_node (dispatcher->sources, node);
    }
}

static void
ply_signal_dispatcher_reset_signal_sources (ply_signal_dispatcher_t *dispatcher,
                                            int                      fd)
{
  ply_list_node_t *node;

  node = ply_list_get_first_node (dispatcher->sources);
  while (node != NULL)
    {
      ply_signal_source_t *handler;

      handler = (ply_signal_source_t *) ply_list_node_get_data (node);

      signal (handler->signal_number, 
              handler->old_posix_signal_handler != NULL?
              handler->old_posix_signal_handler : SIG_DFL);

      node = ply_list_get_next_node (dispatcher->sources, node);
    }
}

static ply_event_source_t *
ply_event_source_new (int                  fd,
                      ply_event_handler_t  new_data_handler,
                      ply_event_handler_t  disconnected_handler,
                      void                *user_data,
                      ply_event_loop_free_handler_t  free_function)
{
  ply_event_source_t *source;

  source = calloc (1, sizeof (ply_event_source_t));

  source->fd = fd;
  source->new_data_handler = new_data_handler;
  source->disconnected_handler = disconnected_handler;
  source->user_data = user_data;
  source->free_function = free_function;

  return source;
}

static void
ply_event_source_free (ply_event_source_t *source)
{
  if (source == NULL)
    return;

  if (source->free_function != NULL)
    source->free_function ((void *) source->user_data);

  free (source);
}

ply_event_loop_t *
ply_event_loop_new (void)
{
  ply_event_loop_t *loop;

  loop = calloc (1, sizeof (ply_event_loop_t));

  loop->epoll_fd = epoll_create (PLY_EVENT_LOOP_NUM_EVENT_HANDLERS);

  assert (loop->epoll_fd >= 0);

  loop->should_exit = false;
  loop->exit_code = 0;

  loop->sources = ply_list_new ();
  loop->exit_closures = ply_list_new ();

  loop->signal_dispatcher = ply_signal_dispatcher_new ();

  if (loop->signal_dispatcher == NULL)
    return NULL;

  ply_event_loop_watch_fd (loop, 
                           ply_signal_dispatcher_receiver_fd,
                           (ply_event_handler_t)
                           ply_signal_dispatcher_dispatch_signal,
                           (ply_event_handler_t)
                           ply_signal_dispatcher_reset_signal_sources,
                           loop->signal_dispatcher);

  return loop;
}

static void
ply_event_loop_free_sources (ply_event_loop_t *loop)
{
  ply_list_node_t *node;

  node = ply_list_get_first_node (loop->sources);
  while (node != NULL)
    {
      ply_list_node_t *next_node;
      ply_event_source_t *source;

      source = (ply_event_source_t *) ply_list_node_get_data (node);
      next_node = ply_list_get_next_node (loop->sources, node);
      ply_event_loop_remove_source (loop, source);
      ply_event_source_free (source);

      node = next_node;
    }
  ply_list_free (loop->sources);
}

static void
ply_event_loop_free_exit_closures (ply_event_loop_t *loop)
{
  ply_list_node_t *node;

  node = ply_list_get_first_node (loop->exit_closures);
  while (node != NULL)
    {
      ply_list_node_t *next_node;
      ply_event_loop_exit_closure_t *closure;

      closure = (ply_event_loop_exit_closure_t *) ply_list_node_get_data (node);
      next_node = ply_list_get_next_node (loop->exit_closures, node);
      free (closure);

      node = next_node;
    }
  ply_list_free (loop->exit_closures);
}

static void
ply_event_loop_run_exit_closures (ply_event_loop_t *loop)
{
  ply_list_node_t *node;

  node = ply_list_get_first_node (loop->exit_closures);
  while (node != NULL)
    {
      ply_list_node_t *next_node;
      ply_event_loop_exit_closure_t *closure;

      closure = (ply_event_loop_exit_closure_t *) ply_list_node_get_data (node);

      assert (closure->handler != NULL);
      next_node = ply_list_get_next_node (loop->exit_closures, node);

      closure->handler (closure->user_data, loop->exit_code, loop);

      node = next_node;
    }
}

void
ply_event_loop_free (ply_event_loop_t *loop)
{

  if (loop == NULL)
    return;

  ply_signal_dispatcher_free (loop->signal_dispatcher);
  ply_event_loop_free_sources (loop);
  ply_event_loop_free_exit_closures (loop);

  close (loop->epoll_fd);
  free (loop);
}

static bool
ply_event_loop_add_source (ply_event_loop_t    *loop,
                           ply_event_source_t  *source)
{
  struct epoll_event event = { 0 };

  event.events = EPOLLIN | EPOLLPRI | EPOLLERR | EPOLLHUP;
  event.data.ptr = source;

  if (epoll_ctl (loop->epoll_fd, EPOLL_CTL_ADD, source->fd, &event) < 0)
    return false;

  ply_list_append_data (loop->sources, source);

  return true;
}

static void
ply_event_loop_remove_source_node (ply_event_loop_t *loop,
                                   ply_list_node_t  *source_node)
{
  ply_event_source_t *source;
  struct epoll_event event = { 0 };
  int status;

  source = (ply_event_source_t *) ply_list_node_get_data (source_node);

  assert (source != NULL);

  event.events = EPOLLIN | EPOLLPRI | EPOLLERR | EPOLLHUP;
  event.data.ptr = source;

  status = epoll_ctl (loop->epoll_fd, EPOLL_CTL_DEL, source->fd, &event);

  assert (status == 0);

  ply_list_remove_node (loop->sources, source_node);
}

static void
ply_event_loop_remove_source (ply_event_loop_t   *loop,
                              ply_event_source_t *source)
{
  ply_list_node_t *source_node;

  source_node = ply_list_find_node (loop->sources, source);

  assert (source_node != NULL);

  ply_event_loop_remove_source_node (loop, source_node);
}

ply_list_node_t *
ply_event_loop_find_source_node (ply_event_loop_t *loop,
                                 int               fd)
{
  ply_list_node_t *node;

  node = ply_list_get_first_node (loop->sources);
  while (node != NULL)
    {
      ply_event_source_t *source;

      source = (ply_event_source_t *) ply_list_node_get_data (node);
      
      if (source->fd == fd)
        break;

      node = ply_list_get_next_node (loop->sources, node);
    }

  return node;
}

bool
ply_event_loop_watch_fd (ply_event_loop_t   *loop,
                        int                  fd,
                        ply_event_handler_t  new_data_handler,
                        ply_event_handler_t  disconnected_handler,
                        void                *user_data)
{
  ply_list_node_t *node;
  ply_event_source_t *source;

  node = ply_event_loop_find_source_node (loop, fd);

  assert (node == NULL);

  source = ply_event_source_new (fd,
                                   new_data_handler,
                                   disconnected_handler,
                                   user_data, NULL);

  if (!ply_event_loop_add_source (loop, source))
    return false;

  return true;
}

void
ply_event_loop_stop_watching_fd (ply_event_loop_t  *loop,
                                 int           fd)
{
  ply_list_node_t *node;
  ply_event_source_t *source;

  node = ply_event_loop_find_source_node (loop, fd);

  assert (node != NULL);

  source = (ply_event_source_t *) ply_list_node_get_data (node);

  ply_event_loop_remove_source_node (loop, node);

  ply_event_source_free (source);
}

ply_list_node_t *
ply_signal_dispatcher_find_source_node (ply_signal_dispatcher_t *dispatcher,
                                        int                 signal_number)
{
  ply_list_node_t *node;

  node = ply_list_get_first_node (dispatcher->sources);
  while (node != NULL)
    {
      ply_signal_source_t *handler;

      handler = (ply_signal_source_t *) ply_list_node_get_data (node);
      
      if (handler->signal_number == signal_number)
        break;

      node = ply_list_get_next_node (dispatcher->sources, node);
    }

  return node;
}

bool
ply_event_loop_watch_signal (ply_event_loop_t   *loop,
                            int                  signal_number,
                            ply_event_handler_t  signal_handler,
                            void                *user_data)
{
  ply_signal_source_t *source;

  source = ply_signal_source_new (signal_number,
                                  signal_handler,
                                  user_data);

  source->old_posix_signal_handler = 
      signal (signal_number, ply_signal_dispatcher_posix_signal_handler);
  ply_list_append_data (loop->signal_dispatcher->sources, source);

  return true;
}

static void
ply_signal_dispatcher_remove_source_node (ply_signal_dispatcher_t  *dispatcher,
                                           ply_list_node_t          *node)
{
  ply_signal_source_t *source;

  source = (ply_signal_source_t *) ply_list_node_get_data (node);

  signal (source->signal_number, 
          source->old_posix_signal_handler != NULL?
          source->old_posix_signal_handler : SIG_DFL);

  ply_list_remove_node (dispatcher->sources, node);
}

void
ply_event_loop_stop_watching_signal (ply_event_loop_t *loop,
                                     int               signal_number)
{
  ply_list_node_t *node;

  node = ply_signal_dispatcher_find_source_node (loop->signal_dispatcher,
                                                 signal_number);

  assert (node != NULL);

  ply_signal_dispatcher_remove_source_node (loop->signal_dispatcher, node);
}

void 
ply_event_loop_watch_for_exit (ply_event_loop_t              *loop,
                               ply_event_loop_exit_handler_t  exit_handler,
                               void                          *user_data)
{
  ply_event_loop_exit_closure_t *exit_closure;

  assert (loop != NULL);
  assert (exit_handler != NULL);

  exit_closure = calloc (1, sizeof (ply_event_loop_exit_closure_t));
  exit_closure->handler = exit_handler;
  exit_closure->user_data = user_data;

  ply_list_append_data (loop->exit_closures, exit_closure);
}

static void
ply_event_loop_process_pending_events (ply_event_loop_t *loop)
{
  int number_of_received_events, i;
  static struct epoll_event *events = NULL;
  int saved_errno;

  if (events == NULL)
    events = 
        malloc (PLY_EVENT_LOOP_NUM_EVENT_HANDLERS * sizeof (struct epoll_event));

  memset (events, -1, 
          PLY_EVENT_LOOP_NUM_EVENT_HANDLERS * sizeof (struct epoll_event));

  do
   {
     number_of_received_events = epoll_wait (loop->epoll_fd, events,
                                             sizeof (events), -1);

     if (number_of_received_events < 0)
       {
         saved_errno = errno;

         if (saved_errno != EINTR)
           {
             ply_event_loop_exit (loop, 255);
             return;
           }
       }
    } 
  while ((number_of_received_events < 0) && (saved_errno == EINTR));

  for (i = 0; i < number_of_received_events; i++)
    {
      ply_event_source_t *source;

      source = (ply_event_source_t *) (events[i].data.ptr);

      if ((events[i].events & EPOLLIN) || (events[i].events & EPOLLPRI))
        {
          if (source->new_data_handler != NULL)
            source->new_data_handler (source->user_data, source->fd);
        }
      else if ((events[i].events & EPOLLHUP) || (events[i].events & EPOLLERR))
        {
          if (source->disconnected_handler != NULL)
            source->disconnected_handler (source->user_data, source->fd);

          ply_event_loop_remove_source (loop, source);
          ply_event_source_free (source);
          source = NULL;
        }

      if (loop->should_exit)
        break;
    }
}

void
ply_event_loop_exit (ply_event_loop_t *loop, int exit_code)
{
  loop->should_exit = true;
  loop->exit_code = exit_code;
}

int
ply_event_loop_run (ply_event_loop_t *loop)
{
  if (loop->should_exit)
    return loop->exit_code;

  do
    {
      ply_event_loop_process_pending_events (loop);
    }
  while (!loop->should_exit);

  ply_event_loop_run_exit_closures (loop);

  loop->should_exit = false;

  return loop->exit_code;
}

#ifdef PLY_EVENT_LOOP_ENABLE_TEST

static ply_event_loop_t *loop;

static void
usr1_signal_handler (void)
{
  write (1, "got sigusr1\n", sizeof ("got sigusr1\n") - 1);
}

static void
hangup_signal_handler (void)
{
  write (1, "got hangup\n", sizeof ("got hangup\n") - 1);
}

static void
terminate_signal_handler (void)
{
  write (1, "got terminate\n", sizeof ("got terminate\n") - 1);
  ply_event_loop_exit (loop, 0);
}

static void
line_received_handler (void)
{
  char line[512] = { 0 };
  printf ("Received line: ");
  fflush (stdout);

  fgets (line, sizeof (line), stdin);
  printf ("%s", line);
}

int
main (int    argc, 
      char **argv)
{
  int exit_code;

  loop = ply_event_loop_new ();

  ply_event_loop_watch_signal (loop, SIGHUP,
			     (ply_event_handler_t) hangup_signal_handler,
			     NULL);
  ply_event_loop_watch_signal (loop, SIGTERM,
			     (ply_event_handler_t)
			     terminate_signal_handler, NULL);
  ply_event_loop_watch_signal (loop, SIGUSR1,
			     (ply_event_handler_t)
			     usr1_signal_handler, NULL);

  ply_event_loop_watch_fd (loop, 0,
                          (ply_event_handler_t) line_received_handler,
                          (ply_event_handler_t) line_received_handler,
                          NULL);

  exit_code = ply_event_loop_run (loop);

  ply_event_loop_free (loop);

  return exit_code;
}
#endif /* PLY_EVENT_LOOP_ENABLE_TEST */
/* vim: set ts=4 sw=4 expandtab autoindent cindent cino={.5s,(0: */
