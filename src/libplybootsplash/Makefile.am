INCLUDES = -I$(top_srcdir)                                                    \
           -I$(srcdir)                                                        \
           -I$(srcdir)/../libply                                              \
           -I$(srcdir)/../plugins/controls

lib_LTLIBRARIES = libplybootsplash.la

libplybootsplashdir = $(includedir)/plymouth-1/plybootsplash
libplybootsplash_HEADERS = \
		    ply-animation.h                                           \
		    ply-boot-splash.h                                         \
		    ply-boot-splash-plugin.h                                  \
		    ply-console.h                                             \
		    ply-entry.h                                               \
		    ply-image.h                                               \
		    ply-keyboard.h                                            \
		    ply-label.h                                               \
		    ply-label-plugin.h                                        \
		    ply-pixel-buffer.h                                        \
		    ply-pixel-display.h                                       \
		    ply-progress-animation.h                                  \
		    ply-progress-bar.h                                        \
		    ply-renderer.h                                            \
		    ply-renderer-plugin.h                                     \
		    ply-terminal.h                                            \
		    ply-text-display.h                                        \
		    ply-text-progress-bar.h                                   \
		    ply-throbber.h

libplybootsplash_la_CFLAGS = $(PLYMOUTH_CFLAGS)                               \
			     $(IMAGE_CFLAGS)                                  \
                             -DPLYMOUTH_BACKGROUND_COLOR=$(background_color)   \
                       -DPLYMOUTH_BACKGROUND_END_COLOR=$(background_end_color) \
                       -DPLYMOUTH_BACKGROUND_START_COLOR=$(background_start_color) \
                       -DPLYMOUTH_PLUGIN_PATH=\"$(PLYMOUTH_PLUGIN_PATH)\"
libplybootsplash_la_LIBADD = $(PLYMOUTH_LIBS) $(IMAGE_LIBS) ../libply/libply.la
libplybootsplash_la_LDFLAGS = -export-symbols-regex '^[^_].*' \
		    -version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE) \
		    -no-undefined
libplybootsplash_la_SOURCES = \
		    $(libplybootsplash_HEADERS)                              \
		    ply-console.c                                            \
		    ply-entry.c                                              \
		    ply-image.c                                              \
		    ply-keyboard.c                                           \
		    ply-label.c                                              \
		    ply-progress-bar.c                                       \
		    ply-throbber.c                                           \
		    ply-animation.c                                          \
		    ply-pixel-display.c                                      \
		    ply-progress-animation.c                                 \
		    ply-text-display.c                                       \
		    ply-text-progress-bar.c                                  \
		    ply-terminal.c                                           \
		    ply-pixel-buffer.c                                       \
		    ply-renderer.c                                           \
		    ply-boot-splash.c

MAINTAINERCLEANFILES = Makefile.in
