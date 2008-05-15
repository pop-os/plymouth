SUBDIRS = libply . splash-plugins rhgb-client tests
INCLUDES = -I$(top_srcdir)                                                    \
           -I$(srcdir)/libply                                                 \
           -I$(srcdir)
plymouthbindir = $(libexecdir)/plymouth
plymouthbin_PROGRAMS = plymouth 

plymouth_CFLAGS = $(PLYMOUTH_CFLAGS) -DPLYMOUTH_PLUGIN_PATH=\"$(libdir)/plymouth/\"
plymouth_LDADD = $(PLYMOUTH_LIBS) libply/libply.la
plymouth_SOURCES =                                                            \
	           ply-boot-protocol.h                                        \
                   ply-boot-server.h                                          \
                   ply-boot-server.c                                          \
                   ply-boot-splash-plugin.h                                   \
                   ply-boot-splash.h                                          \
                   ply-boot-splash.c                                          \
                   ply-window.h                                               \
                   ply-window.c                                               \
                   main.c

MAINTAINERCLEANFILES = Makefile.in
