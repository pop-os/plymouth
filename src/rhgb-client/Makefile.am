SUBDIRS = tests
INCLUDES = -I$(top_srcdir)                                                    \
           -I$(srcdir)/../libply                                              \
           -I$(srcdir)/..                                                     \
           -I$(srcdir)

bin_PROGRAMS = rhgb-client 

rhgb_client_CFLAGS = $(PLYMOUTH_CFLAGS)
rhgb_client_LDADD = $(PLYMOUTH_LIBS) ../libply/libply.la
rhgb_client_SOURCES = \
		      $(srcdir)/../ply-boot-protocol.h                        \
                      $(srcdir)/ply-boot-client.h                             \
                      $(srcdir)/ply-boot-client.c                             \
                      $(srcdir)/rhgb-client.c

MAINTAINERCLEANFILES = Makefile.in
