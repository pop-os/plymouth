#include <config.h>

#include "ply-utils.h"

bool 
ply_open_unidirectional_pipe (int *sender_fd,
                              int *receiver_fd)
{
  init pipe_fds[2];

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
