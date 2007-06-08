SUBDIRS = libply . splash-plugins rhgb-client
INCLUDES = -I$(top_srcdir)                                                    \
           -I$(srcdir)/libply                                                 \
           -I$(srcdir)
plymouthbindir = $(libexecdir)/plymouth
plymouthbin_PROGRAMS = plymouth 

plymouth_CFLAGS = $(PLYMOUTH_CFLAGS) -DPLYMOUTH_PLUGIN_PATH=\"$(prefix)/$${LIB}/plymouth/\"
plymouth_LDADD = $(PLYMOUTH_LIBS) libply/libply.la
plymouth_SOURCES =                                                            \
                   ply-boot-server.h                                          \
                   ply-boot-server.c                                          \
                   ply-boot-splash-plugin.h                                   \
                   ply-boot-splash.h                                          \
                   ply-boot-splash.c                                          \
                   main.c

MAINTAINERCLEANFILES = Makefile.in
