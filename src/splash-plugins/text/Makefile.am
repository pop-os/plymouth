INCLUDES = -I$(top_srcdir)                                                    \
           -I$(srcdir)/../../libply                                           \
           -I$(srcdir)/../../libplybootsplash                                 \
           -I$(srcdir)/../..                                                  \
           -I$(srcdir)/..                                                     \
           -I$(srcdir)

plugindir = $(libdir)/plymouth
plugin_LTLIBRARIES = text.la

text_la_CFLAGS = $(PLYMOUTH_CFLAGS)
text_la_LDFLAGS = -module -avoid-version -export-dynamic
text_la_LIBADD = $(PLYMOUTH_LIBS)                                            \
		    ../../libply/libply.la                                   \
		    ../../libplybootsplash/libplybootsplash.la
text_la_SOURCES = $(srcdir)/plugin.c

MAINTAINERCLEANFILES = Makefile.in
