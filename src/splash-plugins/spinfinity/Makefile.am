INCLUDES = -I$(top_srcdir)                                                    \
           -I$(srcdir)/../../libply                                           \
           -I$(srcdir)/../..                                                  \
           -I$(srcdir)/..                                                     \
           -I$(srcdir)

plugindir = $(libdir)/plymouth
plugin_LTLIBRARIES = spinfinity.la

spinfinity_la_CFLAGS = $(PLYMOUTH_CFLAGS) -DPLYMOUTH_IMAGE_DIR=\"$(datadir)/plymouth/\"
spinfinity_la_LDFLAGS = -module -avoid-version -export-dynamic
spinfinity_la_LIBADD = $(PLYMOUTH_LIBS) ../../libply/libply.la
spinfinity_la_SOURCES = $(srcdir)/../../ply-boot-splash-plugin.h             \
                            $(srcdir)/../../ply-window.h                     \
                            $(srcdir)/../../ply-window.c                     \
                            $(srcdir)/spinfinity.c

plymouthdir = $(datadir)/plymouth
plymouth_DATA = bullet.png entry.png lock.png box.png

EXTRA_DIST = $(plymouth_DATA)
MAINTAINERCLEANFILES = Makefile.in
