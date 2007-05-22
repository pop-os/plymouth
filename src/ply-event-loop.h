#ifndef PLY_EVENT_LOOP_H
#define PLY_EVENT_LOOP_H

#include <stdbool.h>

typedef struct _ply_event_loop ply_event_loop_t;

typedef void (* ply_event_handler_t) (void *user_data,
                                      int   source);

#ifndef PLY_HIDE_FUNCTION_DECLARATIONS
ply_event_loop_t *ply_event_loop_new (void);
void ply_event_loop_free (ply_event_loop_t *loop);
bool ply_event_loop_watch_fd (ply_event_loop_t *loop,
                              int               fd,
                              ply_event_handler_t new_data_handler,
                              ply_event_handler_t disconnected_handler,
                              void          *user_data);
bool ply_event_loop_stop_watching_fd (ply_event_loop_t *loop, 
		                      int               fd);
bool ply_event_loop_watch_signal (ply_event_loop_t     *loop,
                                  int                   signal_number,
                                  ply_event_handler_t   signal_handler,
                                  void                  *user_data);
bool ply_event_loop_stop_watching_signal (ply_event_loop_t *loop,
                                          int                signal_number);

int ply_event_loop_run (ply_event_loop_t *loop);
void ply_event_loop_exit (ply_event_loop_t *loop,
                          int          exit_code);
#endif

#endif
