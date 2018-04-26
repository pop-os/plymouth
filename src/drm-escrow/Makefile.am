AM_CPPFLAGS = -I$(top_srcdir)                                                 \
           -I$(top_srcdir)/src                                                \
           -I$(top_srcdir)/src/libply                                         \
           -I$(srcdir)

plymouth_drm_escrowdir = $(plymouthclientdir)

plymouth_drm_escrow_PROGRAMS = plymouth-drm-escrow

plymouth_drm_escrow_CFLAGS = $(PLYMOUTH_CFLAGS) -DPLYMOUTH_PLUGIN_PATH=\"$(PLYMOUTH_PLUGIN_PATH)\"
plymouth_drm_escrow_LDADD = $(PLYMOUTH_LIBS) ../libply/libply.la
plymouth_drm_escrow_SOURCES = $(srcdir)/plymouth-drm-escrow.c

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = ply-boot-client.pc

MAINTAINERCLEANFILES = Makefile.in
