SUBDIRS = tests
INCLUDES = -I$(top_srcdir)                                                    \
           -I$(srcdir)

plymouth_CFLAGS = $(PLYMOUTH_CFLAGS)
plymouth_LDADD = $(PLYMOUTH_LIBS)
plymouth_SOURCES =                                                            \
                   ply-boot-server.h                                          \
                   ply-boot-server.c                                          \
                   ply-boot-splash-plugin.h                                   \
                   ply-boot-splash.h                                          \
                   ply-boot-splash.c                                          \
                   ply-list.h                                                 \
                   ply-list.c                                                 \
                   ply-logger.h                                               \
                   ply-logger.c                                               \
                   ply-event-loop.h                                           \
                   ply-event-loop.c                                           \
		   ply-terminal.h                                             \
		   ply-terminal.c                                             \
		   ply-terminal-session.h                                     \
		   ply-terminal-session.c                                     \
		   ply-utils.h                                                \
                   ply-utils.c                                                \
                   main.c
rhgb_client_CFLAGS = $(PLYMOUTH_CFLAGS)
rhgb_client_LDADD = $(PLYMOUTH_LIBS)
rhgb_client_SOURCES = ply-utils.h                                             \
                      ply-utils.c                                             \
                      ply-logger.h                                            \
                      ply-logger.c                                            \
                      ply-list.h                                              \
                      ply-list.c                                              \
                      ply-event-loop.h                                        \
                      ply-event-loop.c                                        \
                      ply-boot-client.h                                       \
                      ply-boot-client.c                                       \
                      rhgb-client.c

noinst_PROGRAMS = plymouth rhgb-client
