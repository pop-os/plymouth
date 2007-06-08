INCLUDES = -I$(top_srcdir)                                                    \
           -I$(srcdir)/../../libply                                           \
           -I$(srcdir)/../..                                                  \
           -I$(srcdir)/..                                                     \
           -I$(srcdir)

lib_LTLIBRARIES = fedora-fade-in.la

fedora_fade_in_la_CFLAGS = $(PLYMOUTH_CFLAGS)
fedora_fade_in_la_LDFLAGS = -module -avoid-version -export-dynamic
fedora_fade_in_la_LIBADD = $(PLYMOUTH_LIBS) ../../libply/libply.la
fedora_fade_in_la_SOURCES = $(srcdir)/../ply-boot-splash-plugin.h             \
                            $(srcdir)/fedora-fade-in.c

plymouthdir = $(datadir)/plymouth
plymouth_DATA = star.png

EXTRA_DIST = $(plymouth_DATA)
MAINTAINERCLEANFILES = Makefile.in
