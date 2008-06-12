INCLUDES = -I$(top_srcdir)                                                    \
           -I$(srcdir)                                                        \
           -I$(srcdir)/../libply

lib_LTLIBRARIES = libplybootsplash.la

libplybootsplashdir = $(includedir)/plymouth-1/plybootsplash
libplybootsplash_HEADERS = ply-answer.h ply-throbber.h ply-window.h ply-boot-splash-plugin.h

libplybootsplash_la_CFLAGS = $(PLYMOUTH_CFLAGS)                               \
                             -DPLYMOUTH_BACKGROUND_COLOR=$(background_color)
libplybootsplash_la_LIBADD = $(PLYMOUTH_LIBS) ../libply/libply.la
libplybootsplash_la_LDFLAGS = -export-symbols-regex '^[^_].*' \
		    -version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE) \
		    -no-undefined
libplybootsplash_la_SOURCES = \
		    $(libplybootsplash_HEADERS)                              \
		    ply-answer.c                                             \
		    ply-throbber.c                                           \
		    ply-window.c

MAINTAINERCLEANFILES = Makefile.in
