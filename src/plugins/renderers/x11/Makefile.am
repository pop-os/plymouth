INCLUDES = -I$(top_srcdir)                                                    \
           -I$(srcdir)/../../../libply                                        \
           -I$(srcdir)/../../../libplybootsplash                              \
           -I$(srcdir)/../../..                                               \
           -I$(srcdir)/../..                                                  \
           -I$(srcdir)/..                                                     \
           -I$(srcdir)

plugindir = $(libdir)/plymouth/renderers
plugin_LTLIBRARIES = x11.la

x11_la_CFLAGS = $(GTK_CFLAGS) $(PLYMOUTH_CFLAGS)
x11_la_LDFLAGS = -module -avoid-version -export-dynamic
x11_la_LIBADD = $(PLYMOUTH_LIBS)                             \
                $(GTK_LIBS)                                  \
                ../../../libply/libply.la                    \
                ../../../libplybootsplash/libplybootsplash.la
x11_la_SOURCES = $(srcdir)/plugin.c

MAINTAINERCLEANFILES = Makefile.in
