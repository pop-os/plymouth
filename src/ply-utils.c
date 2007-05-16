#include <config.h>

#include "ply-utils.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdlib.h>
#include <sys/types.h>

bool 
ply_open_unidirectional_pipe (int *sender_fd,
                              int *receiver_fd)
{
  int pipe_fds[2];

  assert (sender_fd != NULL);
  assert (receiver_fd != NULL);

  if (pipe (pipe_fds) < 0)
    return false;

  if (fcntl (pipe_fds[0], F_SETFD, O_NONBLOCK | FD_CLOEXEC) < 0)
    {
      close (pipe_fds[0]);
      close (pipe_fds[1]);
      return false;
    }

  if (fcntl (pipe_fds[1], F_SETFD, O_NONBLOCK | FD_CLOEXEC) < 0)
    {
      close (pipe_fds[0]);
      close (pipe_fds[1]);
      return false;
    }

  *sender_fd = pipe_fds[1];
  *receiver_fd = pipe_fds[0];

  return true;
}

bool 
ply_write (int         fd,
           const void *buffer,
           size_t      number_of_bytes)
{
  size_t bytes_left_to_write;
  size_t total_bytes_written = 0;

  bytes_left_to_write = number_of_bytes;

  do
    {
      ssize_t bytes_written = 0;

      bytes_written = write (fd,
                             ((uint8_t *) buffer) + total_bytes_written,
                             bytes_left_to_write);

      if (bytes_written > 0)
        {
          total_bytes_written += bytes_written;
          bytes_left_to_write -= bytes_written;
        }
      else if ((errno != EINTR))
        break;
    }
  while (bytes_left_to_write > 0);

  return bytes_left_to_write == 0;
}

bool 
ply_fd_has_data (int fd)
{
  struct pollfd poll_data;
  int result;

  poll_data.fd = fd;
  poll_data.events = POLLIN | POLLPRI;
  poll_data.revents = 0;
  result = poll (&poll_data, 1, 10);

  return result == 1;
}

bool 
ply_fd_can_take_data (int fd)
{
  struct pollfd poll_data;
  int result;

  poll_data.fd = fd;
  poll_data.events = POLLOUT;
  poll_data.revents = 0;
  result = poll (&poll_data, 1, 10);

  return result == 1;
}
