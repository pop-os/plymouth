INCLUDES = -I$(top_srcdir)                                                    \
           -I$(srcdir)/../../libply                                           \
           -I$(srcdir)/../..                                                  \
           -I$(srcdir)/..                                                     \
           -I$(srcdir)

plugindir = $(libdir)/plymouth
plugin_LTLIBRARIES = fedora-fade-in.la

fedora_fade_in_la_CFLAGS = $(PLYMOUTH_CFLAGS) -DPLYMOUTH_IMAGE_DIR=\"$(datadir)/plymouth/\"
fedora_fade_in_la_LDFLAGS = -module -avoid-version -export-dynamic
fedora_fade_in_la_LIBADD = $(PLYMOUTH_LIBS) ../../libply/libply.la
fedora_fade_in_la_SOURCES = $(srcdir)/../../ply-boot-splash-plugin.h         \
                            $(srcdir)/../../ply-window.h                     \
                            $(srcdir)/../../ply-window.c                     \
                            $(srcdir)/plugin.c

plymouthdir = $(datadir)/plymouth/fedora-fade-in
plymouth_DATA = star.png bullet.png entry.png lock.png

EXTRA_DIST = $(plymouth_DATA)
MAINTAINERCLEANFILES = Makefile.in
