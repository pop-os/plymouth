INCLUDES = -I$(top_srcdir)                                                    \
           -I$(srcdir)/../../libply                                           \
           -I$(srcdir)/../..                                                  \
           -I$(srcdir)/..                                                     \
           -I$(srcdir)

plugindir = $(libdir)/plymouth
plugin_LTLIBRARIES = text.la

text_la_CFLAGS = $(PLYMOUTH_CFLAGS)
text_la_LDFLAGS = -module -avoid-version -export-dynamic
text_la_LIBADD = $(PLYMOUTH_LIBS) ../../libply/libply.la
text_la_SOURCES = $(srcdir)/../../ply-boot-splash-plugin.h         \
                  $(srcdir)/../../ply-window.h                     \
                  $(srcdir)/../../ply-window.c                     \
                  $(srcdir)/plugin.c

MAINTAINERCLEANFILES = Makefile.in
