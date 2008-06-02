SUBDIRS = libply . splash-plugins client tests
INCLUDES = -I$(top_srcdir)                                                    \
           -I$(srcdir)/libply                                                 \
           -I$(srcdir)                                                        \
           -DPLYMOUTH_LOGO_FILE=\"$(logofile)\"

plymouthdbindir = $(libexecdir)/plymouth
plymouthdbin_PROGRAMS = plymouthd

plymouthd_CFLAGS = $(PLYMOUTH_CFLAGS) -DPLYMOUTH_PLUGIN_PATH=\"$(libdir)/plymouth/\"
plymouthd_LDADD = $(PLYMOUTH_LIBS) libply/libply.la
plymouthd_SOURCES =                                                            \
                   ply-answer.h                                               \
                   ply-answer.c                                               \
                   ply-boot-protocol.h                                        \
                   ply-boot-server.h                                          \
                   ply-boot-server.c                                          \
                   ply-boot-splash-plugin.h                                   \
                   ply-boot-splash.h                                          \
                   ply-boot-splash.c                                          \
                   ply-window.h                                               \
                   ply-window.c                                               \
                   main.c

plymouthdrundir = $(localstatedir)/run/plymouth

install-data-hook:
	-mkdir -p $(DESTDIR)$(plymouthdrundir)

MAINTAINERCLEANFILES = Makefile.in
