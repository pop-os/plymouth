INCLUDES = -I$(top_srcdir)                                                    \
           -I$(srcdir)/../../../libply                                        \
           -I$(srcdir)/../../../libplybootsplash                              \
           -I$(srcdir)/../../..                                               \
           -I$(srcdir)/../..                                                  \
           -I$(srcdir)/..                                                     \
           -I$(srcdir)

plugindir = $(libdir)/plymouth
plugin_LTLIBRARIES = script.la

script_la_CFLAGS =  $(PLYMOUTH_CFLAGS)                                        \
                    -DPLYMOUTH_IMAGE_DIR=\"$(datadir)/plymouth/\"             \
                    -DPLYMOUTH_LOGO_FILE=\"$(logofile)\"                      \
                    -DPLYMOUTH_BACKGROUND_COLOR=$(background_color)           \
                    -DPLYMOUTH_BACKGROUND_END_COLOR=$(background_end_color)   \
                    -DPLYMOUTH_BACKGROUND_START_COLOR=$(background_start_color)
script_la_LDFLAGS = -module -avoid-version -export-dynamic
script_la_LIBADD =  $(PLYMOUTH_LIBS)                                          \
                    ../../../libply/libply.la                                 \
                    ../../../libplybootsplash/libplybootsplash.la
script_la_SOURCES = $(srcdir)/plugin.c                                        \
                    $(srcdir)/plugin.h                                        \
                    $(srcdir)/script.c                                        \
                    $(srcdir)/script.h                                        \
                    $(srcdir)/script-parse.c                                  \
                    $(srcdir)/script-parse.h                                  \
                    $(srcdir)/script-execute.c                                \
                    $(srcdir)/script-execute.h                                \
                    $(srcdir)/script-object.c                                 \
                    $(srcdir)/script-object.h                                 \
                    $(srcdir)/script-lib-image.c                              \
                    $(srcdir)/script-lib-image.h                              \
                    $(srcdir)/script-lib-image.script                         \
                    $(srcdir)/script-lib-sprite.c                             \
                    $(srcdir)/script-lib-sprite.h                             \
                    $(srcdir)/script-lib-sprite.script                        \
                    $(srcdir)/script-lib-plymouth.c                           \
                    $(srcdir)/script-lib-plymouth.h                           \
                    $(srcdir)/script-lib-plymouth.script                      \
                    $(srcdir)/script-lib-math.c                               \
                    $(srcdir)/script-lib-math.h                               \
                    $(srcdir)/script-lib-math.script                          \
                    $(srcdir)/ply-scan.c                                      \
                    $(srcdir)/ply-scan.h                                      

MAINTAINERCLEANFILES = Makefile.in
CLEANFILES = *.string


EXTRA_DIST = stringify.pl

%.string: %.script stringify.pl
	./stringify.pl < $< > $@
    
script-lib-image.c : script-lib-image.string
script-lib-sprite.c : script-lib-sprite.string
script-lib-math.c : script-lib-math.string
script-lib-plymouth.c : script-lib-plymouth.string
