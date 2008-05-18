SUBDIRS = tests
INCLUDES = -I$(top_srcdir)                                                    \
           -I$(srcdir)

lib_LTLIBRARIES = libply.la

libply_la_CFLAGS = $(PLYMOUTH_CFLAGS)
libply_la_LIBADD = $(PLYMOUTH_LIBS)
libply_la_LDFLAGS = -export-symbols-regex '^[^_].*' \
		    -version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE) \
		    -no-undefined
libply_la_SOURCES = ply-event-loop.c                                          \
		    ply-event-loop.h                                          \
		    ply-frame-buffer.h                                        \
		    ply-frame-buffer.c                                        \
		    ply-buffer.c                                              \
		    ply-buffer.h                                              \
		    ply-list.c                                                \
		    ply-list.h                                                \
		    ply-logger.c                                              \
		    ply-logger.h                                              \
		    ply-image.c                                               \
		    ply-image.h                                               \
		    ply-terminal.c                                            \
		    ply-terminal.h                                            \
		    ply-terminal-session.c                                    \
		    ply-terminal-session.h                                    \
		    ply-utils.c                                               \
		    ply-utils.h

MAINTAINERCLEANFILES = Makefile.in
