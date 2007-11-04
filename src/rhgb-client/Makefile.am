INCLUDES = -I$(top_srcdir)                                                    \
           -I$(top_srcdir)/src                                                \
           -I$(top_srcdir)/src/libply                                         \
           -I$(srcdir)
rhgb_clientdir = $(bindir)
rhgb_client_PROGRAMS = rhgb-client 

rhgb_client_CFLAGS = $(PLYMOUTH_CFLAGS) -DPLYMOUTH_PLUGIN_PATH=\"$(prefix)/\$${LIB}/plymouth/\"
rhgb_client_LDADD = $(PLYMOUTH_LIBS) ../libply/libply.la
rhgb_client_SOURCES = \
		      $(srcdir)/../ply-boot-protocol.h                        \
                      $(srcdir)/ply-boot-client.h                             \
                      $(srcdir)/ply-boot-client.c                             \
                      $(srcdir)/rhgb-client.c

MAINTAINERCLEANFILES = Makefile.in
