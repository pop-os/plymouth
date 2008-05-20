INCLUDES = -I$(top_srcdir)                                                    \
           -I$(top_srcdir)/src                                                \
           -I$(top_srcdir)/src/libply                                         \
           -I$(srcdir)
plymouthdir = $(bindir)
plymouth_PROGRAMS = plymouth

plymouth_CFLAGS = $(PLYMOUTH_CFLAGS) -DPLYMOUTH_PLUGIN_PATH=\"$(prefix)/\$${LIB}/plymouth/\"
plymouth_LDADD = $(PLYMOUTH_LIBS) ../libply/libply.la
plymouth_SOURCES = \
                      $(srcdir)/../ply-boot-protocol.h                        \
                      $(srcdir)/ply-boot-client.h                             \
                      $(srcdir)/ply-boot-client.c                             \
                      $(srcdir)/plymouth.c

install-data-hook:
	(cd $(DESTDIR)$(plymouthdir); ln -s plymouth rhgb-client)

uninstall-hook:
	rm -f $(DESTDIR)$(plymouthdir)/rhgb-client

MAINTAINERCLEANFILES = Makefile.in
