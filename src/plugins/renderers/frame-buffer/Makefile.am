INCLUDES = -I$(top_srcdir)                                                    \
           -I$(srcdir)/../../../libply                                        \
           -I$(srcdir)/../../../libplybootsplash                              \
           -I$(srcdir)/../../..                                               \
           -I$(srcdir)/../..                                                  \
           -I$(srcdir)/..                                                     \
           -I$(srcdir)

plugindir = $(libdir)/plymouth/renderers
plugin_LTLIBRARIES = frame-buffer.la

frame_buffer_la_CFLAGS = $(PLYMOUTH_CFLAGS)

frame_buffer_la_LDFLAGS = -module -avoid-version -export-dynamic
frame_buffer_la_LIBADD = $(PLYMOUTH_LIBS)                                     \
                         ../../../libply/libply.la                            \
                         ../../../libplybootsplash/libplybootsplash.la
frame_buffer_la_SOURCES = $(srcdir)/plugin.c

MAINTAINERCLEANFILES = Makefile.in
