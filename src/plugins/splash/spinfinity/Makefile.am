INCLUDES = -I$(top_srcdir)                                                    \
           -I$(srcdir)/../../../libply                                        \
           -I$(srcdir)/../../../libplybootsplash                              \
           -I$(srcdir)/../../..                                               \
           -I$(srcdir)/../..                                                  \
           -I$(srcdir)/..                                                     \
           -I$(srcdir)

plugindir = $(libdir)/plymouth
plugin_LTLIBRARIES = spinfinity.la

spinfinity_la_CFLAGS = $(PLYMOUTH_CFLAGS)                                    \
                       -DPLYMOUTH_IMAGE_DIR=\"$(datadir)/plymouth/\"         \
                       -DPLYMOUTH_TIME_DIR=\"$(localstatedir)/lib/plymouth/\"\
                       -DPLYMOUTH_LOGO_FILE=\"$(logofile)\"                  \
                       -DPLYMOUTH_BACKGROUND_COLOR=$(background_color)       \
                       -DPLYMOUTH_BACKGROUND_END_COLOR=$(background_end_color)   \
                       -DPLYMOUTH_BACKGROUND_START_COLOR=$(background_start_color)

spinfinity_la_LDFLAGS = -module -avoid-version -export-dynamic
spinfinity_la_LIBADD = $(PLYMOUTH_LIBS)                                      \
                       ../../../libply/libply.la                             \
                       ../../../libplybootsplash/libplybootsplash.la
spinfinity_la_SOURCES = $(srcdir)/plugin.c

MAINTAINERCLEANFILES = Makefile.in
