SUBDIRS = libply . splash-plugins rhgb-client
INCLUDES = -I$(top_srcdir)                                                    \
           -I$(srcdir)/libply                                                 \
           -I$(srcdir)

noinst_PROGRAMS = plymouth 

plymouth_CFLAGS = $(PLYMOUTH_CFLAGS)
plymouth_LDADD = $(PLYMOUTH_LIBS) libply/libply.la
plymouth_SOURCES =                                                            \
                   ply-boot-server.h                                          \
                   ply-boot-server.c                                          \
                   ply-boot-splash-plugin.h                                   \
                   ply-boot-splash.h                                          \
                   ply-boot-splash.c                                          \
                   main.c

MAINTAINERCLEANFILES = Makefile.in
