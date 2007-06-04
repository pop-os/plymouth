/* ply-boot-client.h - APIs for talking to the boot status daemon
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
#include "ply-boot-client.h"

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "ply-event-loop.h"
#include "ply-list.h"
#include "ply-logger.h"
#include "ply-utils.h"

struct _ply_boot_client
{
  ply_event_loop_t *loop;
  ply_fd_watch_t *daemon_can_take_request_watch;
  ply_fd_watch_t *daemon_has_reply_watch;
  ply_list_t *requests_to_send;
  ply_list_t *requests_waiting_for_replies;
  int socket_fd;

  ply_boot_client_disconnect_handler_t disconnect_handler;
  void *disconnect_handler_user_data;

  uint32_t is_connected : 1;
};

typedef struct
{
  ply_boot_client_t *client;
  char *command;
  char *argument;
  ply_boot_client_response_handler_t handler;
  ply_boot_client_response_handler_t failed_handler;
  void *user_data;
} ply_boot_client_request_t;

static void ply_boot_client_cancel_request (ply_boot_client_t         *client,
                                            ply_boot_client_request_t *request);

ply_boot_client_t *
ply_boot_client_new (void)
{
  ply_boot_client_t *client;

  client = calloc (1, sizeof (ply_boot_client_t));
  client->daemon_can_take_request_watch = NULL;
  client->daemon_has_reply_watch = NULL;
  client->requests_to_send = ply_list_new ();
  client->requests_waiting_for_replies = ply_list_new ();
  client->loop = NULL;
  client->is_connected = false;
  client->disconnect_handler = NULL;
  client->disconnect_handler_user_data = NULL;

  return client;
}

static void
ply_boot_client_cancel_requests (ply_boot_client_t *client)
{
  ply_list_node_t *node;

  node = ply_list_get_first_node (client->requests_to_send);
  while (node != NULL)
    {
      ply_boot_client_request_t *request;

      request = (ply_boot_client_request_t *) ply_list_node_get_data (node);

      ply_boot_client_cancel_request (client, request);

      node = ply_list_get_next_node (client->requests_to_send, node);
    }

  node = ply_list_get_first_node (client->requests_waiting_for_replies);
  while (node != NULL)
    {
      ply_boot_client_request_t *request;

      request = (ply_boot_client_request_t *) ply_list_node_get_data (node);

      ply_boot_client_cancel_request (client, request);

      node = ply_list_get_next_node (client->requests_waiting_for_replies, node);
    }
}

void
ply_boot_client_free (ply_boot_client_t *client)
{
  if (client == NULL)
    return;

  ply_boot_client_cancel_requests (client);

  ply_list_free (client->requests_to_send);
  ply_list_free (client->requests_waiting_for_replies);

  free (client);
}

bool
ply_boot_client_connect (ply_boot_client_t *client,
                         ply_boot_client_disconnect_handler_t  disconnect_handler,
                         void                                 *user_data)
{
  assert (client != NULL);
  assert (!client->is_connected);
  assert (client->disconnect_handler == NULL);
  assert (client->disconnect_handler_user_data == NULL);

  client->socket_fd = 
      ply_connect_to_unix_socket (PLY_BOOT_PROTOCOL_SOCKET_PATH, true);

  if (client->socket_fd < 0)
    return false;

  client->disconnect_handler = disconnect_handler;
  client->disconnect_handler_user_data = user_data;

  client->is_connected = true;
  return true;
}

static ply_boot_client_request_t *
ply_boot_client_request_new (ply_boot_client_t                  *client,
                             const char                         *request_command,
                             const char                         *request_argument,
                             ply_boot_client_response_handler_t  handler,
                             ply_boot_client_response_handler_t  failed_handler,
                             void                               *user_data)
{
  ply_boot_client_request_t *request;

  assert (client != NULL);
  assert (request_command != NULL);
  assert (handler != NULL);

  request = calloc (1, sizeof (ply_boot_client_request_t));
  request->client = client;
  request->command = strdup (request_command);
  if (request_argument != NULL)
    request->argument = strdup (request_argument);
  request->handler = handler;
  request->failed_handler = failed_handler;
  request->user_data = user_data;

  return request;
}

static void
ply_boot_client_request_free (ply_boot_client_request_t *request)
{
  if (request == NULL)
    return;
  free (request->command);
  free (request);
}

static void
ply_boot_client_cancel_request (ply_boot_client_t         *client,
                                ply_boot_client_request_t *request)
{
  if (request->failed_handler != NULL)
    request->failed_handler (request->user_data, request->client);

  ply_boot_client_request_free (request);
}

static void
ply_boot_client_process_incoming_replies (ply_boot_client_t *client)
{
  ply_list_node_t *request_node;
  ply_boot_client_request_t *request;
  uint8_t byte[2] = "";

  assert (client != NULL);

  if (ply_list_get_length (client->requests_waiting_for_replies) == 0)
    {
      ply_error ("received unexpected response from boot status daemon");
      return;
    }

  request_node = ply_list_get_first_node (client->requests_waiting_for_replies);
  assert (request_node != NULL);

  request = (ply_boot_client_request_t *) ply_list_node_get_data (request_node);
  assert (request != NULL);

  if (!ply_read (client->socket_fd, byte, sizeof (uint8_t)))
    {
      if (request->failed_handler != NULL)
        request->failed_handler (request->user_data, client);
      ply_list_remove_node (client->requests_waiting_for_replies, request_node);
      return;
    }

  if (memcmp (byte, PLY_BOOT_PROTOCOL_RESPONSE_TYPE_ACK, sizeof (uint8_t)) != 0)
    {
      if (request->failed_handler != NULL)
        request->failed_handler (request->user_data, client);
      ply_list_remove_node (client->requests_waiting_for_replies, request_node);
      return;
    }

  request->handler (request->user_data, client);
  ply_list_remove_node (client->requests_waiting_for_replies, request_node);

  if (ply_list_get_length (client->requests_waiting_for_replies) == 0)
    {
      ply_event_loop_stop_watching_fd (client->loop,
                                       client->daemon_has_reply_watch);
      client->daemon_has_reply_watch = NULL;
    }
}

static char *
ply_boot_client_get_request_string (ply_boot_client_t         *client,
                                    ply_boot_client_request_t *request,
                                    size_t                    *request_size)
{
  char *request_string;

  assert (client != NULL);
  assert (request != NULL);
  assert (request_size != NULL);

  assert (request->command != NULL);

  if (request->argument == NULL)
    {
      request_string = strdup (request->command);
      *request_size = strlen (request_string) + 1;
      return request_string;
    }

  assert (strlen (request->argument) <= UCHAR_MAX);

  request_string = NULL;
  asprintf (&request_string, "%s\002%c%s", request->command, 
            (char) (strlen (request->argument) + 1), request->argument);
  *request_size = strlen (request_string) + 1;

  return request_string;
}

static bool
ply_boot_client_send_request (ply_boot_client_t         *client,
                              ply_boot_client_request_t *request)
{
  char *request_string;
  size_t request_size;

  assert (client != NULL);
  assert (request != NULL);

  request_string = ply_boot_client_get_request_string (client, request,
                                                       &request_size);
  if (!ply_write (client->socket_fd, request_string, request_size))
    {
      free (request_string);
      ply_boot_client_cancel_request (client, request);
      return false;
    }
  free (request_string);

  if (client->daemon_has_reply_watch == NULL)
    {
      assert (ply_list_get_length (client->requests_waiting_for_replies) == 0);
      client->daemon_has_reply_watch = 
          ply_event_loop_watch_fd (client->loop, client->socket_fd,
                                   PLY_EVENT_LOOP_FD_STATUS_HAS_DATA,
                                   (ply_event_handler_t)
                                   ply_boot_client_process_incoming_replies,
                                   NULL, client);
    }

  return true;
}

static void
ply_boot_client_process_pending_requests (ply_boot_client_t *client)
{
  ply_list_node_t *request_node;
  ply_boot_client_request_t *request;

  assert (ply_list_get_length (client->requests_to_send) != 0);
  assert (client->daemon_can_take_request_watch != NULL);

  request_node = ply_list_get_first_node (client->requests_to_send);
  assert (request_node != NULL);

  request = (ply_boot_client_request_t *) ply_list_node_get_data (request_node);
  assert (request != NULL);

  ply_list_remove_node (client->requests_to_send, request_node);

  if (ply_boot_client_send_request (client, request))
    ply_list_append_data (client->requests_waiting_for_replies, request);

  if (ply_list_get_length (client->requests_to_send) == 0)
    {
      ply_event_loop_stop_watching_fd (client->loop,
                                       client->daemon_can_take_request_watch);
      client->daemon_can_take_request_watch = NULL;
    }
}

static void
ply_boot_client_queue_request (ply_boot_client_t                  *client,
                               const char                         *request_command,
                               const char                         *request_argument,
                               ply_boot_client_response_handler_t  handler,
                               ply_boot_client_response_handler_t  failed_handler,
                               void                               *user_data)
{
  ply_boot_client_request_t *request;

  assert (client != NULL);
  assert (client->loop != NULL);
  assert (client->socket_fd >= 0);
  assert (request_command != NULL);
  assert (request_argument == NULL || strlen (request_argument) <= UCHAR_MAX);
  assert (handler != NULL);

  if (client->daemon_can_take_request_watch == NULL)
    {
      assert (ply_list_get_length (client->requests_to_send) == 0);
      client->daemon_can_take_request_watch = 
          ply_event_loop_watch_fd (client->loop, client->socket_fd,
                                   PLY_EVENT_LOOP_FD_STATUS_CAN_TAKE_DATA,
                                   (ply_event_handler_t)
                                   ply_boot_client_process_pending_requests,
                                   NULL, client);
    }

  request = ply_boot_client_request_new (client, request_command,
                                         request_argument, 
                                         handler, failed_handler, user_data);
  ply_list_append_data (client->requests_to_send, request);
}

void
ply_boot_client_ping_daemon (ply_boot_client_t                  *client,
                             ply_boot_client_response_handler_t  handler,
                             ply_boot_client_response_handler_t  failed_handler,
                             void                               *user_data)
{
  assert (client != NULL);

  ply_boot_client_queue_request (client, PLY_BOOT_PROTOCOL_REQUEST_TYPE_PING,
                                 NULL, handler, failed_handler, user_data);
}

void
ply_boot_client_update_daemon (ply_boot_client_t                  *client,
                               const char                         *status,
                               ply_boot_client_response_handler_t  handler,
                               ply_boot_client_response_handler_t  failed_handler,
                               void                               *user_data)
{
  assert (client != NULL);

  ply_boot_client_queue_request (client, PLY_BOOT_PROTOCOL_REQUEST_TYPE_UPDATE,
                                 status, handler, failed_handler, user_data);
}

void
ply_boot_client_disconnect (ply_boot_client_t *client)
{
  assert (client != NULL);

  close (client->socket_fd);
  client->socket_fd = -1;
  client->is_connected = false;
}

static void
ply_boot_client_detach_from_event_loop (ply_boot_client_t *client)
{
  assert (client != NULL);
  client->loop = NULL;
}

static void
ply_boot_client_on_hangup (ply_boot_client_t *client)
{
  assert (client != NULL);
  ply_boot_client_cancel_requests (client);

  if (client->disconnect_handler != NULL)
    client->disconnect_handler (client->disconnect_handler_user_data,
                                client);
}

void
ply_boot_client_attach_to_event_loop (ply_boot_client_t *client,
                                      ply_event_loop_t  *loop)
{
  assert (client != NULL);
  assert (loop != NULL);
  assert (client->loop == NULL);
  assert (client->socket_fd >= 0);

  client->loop = loop;

  ply_event_loop_watch_fd (client->loop, client->socket_fd,
                           PLY_EVENT_LOOP_FD_STATUS_NONE,
                           NULL, 
                           (ply_event_handler_t) ply_boot_client_on_hangup,
                           client);

  ply_event_loop_watch_for_exit (loop, (ply_event_loop_exit_handler_t) 
                                 ply_boot_client_detach_from_event_loop,
                                 client); 

}

#ifdef PLY_BOOT_CLIENT_ENABLE_TEST

#include <stdio.h>

#include "ply-event-loop.h"
#include "ply-boot-client.h"

static void
on_pinged (ply_event_loop_t *loop)
{
  printf ("PING!\n");
}

static void
on_ping_failed (ply_event_loop_t *loop)
{
  printf ("PING FAILED! %m\n");
  ply_event_loop_exit (loop, 1);
}

static void
on_update (ply_event_loop_t *loop)
{
  printf ("UPDATE!\n");
  ply_event_loop_exit (loop, 0);
}

static void
on_update_failed (ply_event_loop_t *loop)
{
  printf ("UPDATE FAILED! %m\n");
  ply_event_loop_exit (loop, 1);
}

static void
on_disconnect (ply_event_loop_t *loop)
{
  printf ("DISCONNECT!\n");
  ply_event_loop_exit (loop, 1);
}

int
main (int    argc,
      char **argv)
{
  ply_event_loop_t *loop;
  ply_boot_client_t *client;
  int exit_code;

  exit_code = 0;

  loop = ply_event_loop_new ();

  client = ply_boot_client_new ();

  if (!ply_boot_client_connect (client, 
                                (ply_boot_client_disconnect_handler_t) on_disconnect,
                                loop))
    {
      perror ("could not start boot client");
      return errno;
    }

  ply_boot_client_attach_to_event_loop (client, loop);
  ply_boot_client_ping_daemon (client, 
                               (ply_boot_client_response_handler_t) on_pinged,
                               (ply_boot_client_response_handler_t) on_ping_failed,
                               loop);

  ply_boot_client_update_daemon (client, 
                                 "loading",
                                 (ply_boot_client_response_handler_t) on_update,
                                 (ply_boot_client_response_handler_t) on_update_failed,
                                 loop);

  exit_code = ply_event_loop_run (loop);

  ply_boot_client_free (client);

  return exit_code;
}

#endif /* PLY_BOOT_CLIENT_ENABLE_TEST */
/* vim: set ts=4 sw=4 expandtab autoindent cindent cino={.5s,(0: */
