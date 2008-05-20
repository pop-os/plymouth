INCLUDES = -I$(top_srcdir)                                                    \
           -I$(srcdir)/../../libply                                           \
           -I$(srcdir)/../..                                                  \
           -I$(srcdir)/..                                                     \
           -I$(srcdir)

plugindir = $(libdir)/plymouth
plugin_LTLIBRARIES = details.la

details_la_CFLAGS = $(PLYMOUTH_CFLAGS)
details_la_LDFLAGS = -module -avoid-version -export-dynamic
details_la_LIBADD = $(PLYMOUTH_LIBS) ../../libply/libply.la
details_la_SOURCES = $(srcdir)/../../ply-boot-splash-plugin.h         \
                  $(srcdir)/details.c

MAINTAINERCLEANFILES = Makefile.in
