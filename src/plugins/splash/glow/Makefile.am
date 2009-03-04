INCLUDES = -I$(top_srcdir)                                                    \
           -I$(srcdir)/../../../libply                                        \
           -I$(srcdir)/../../../libplybootsplash                              \
           -I$(srcdir)/../../..                                               \
           -I$(srcdir)/../..                                                  \
           -I$(srcdir)/..                                                     \
           -I$(srcdir)

plugindir = $(libdir)/plymouth
plugin_LTLIBRARIES = glow.la

glow_la_CFLAGS =    $(PLYMOUTH_CFLAGS)                                       \
                    -DPLYMOUTH_IMAGE_DIR=\"$(datadir)/plymouth/\"             \
                    -DPLYMOUTH_LOGO_FILE=\"$(logofile)\"                      \
                    -DPLYMOUTH_BACKGROUND_COLOR=$(background_color)           \
                    -DPLYMOUTH_BACKGROUND_END_COLOR=$(background_end_color)   \
                    -DPLYMOUTH_BACKGROUND_START_COLOR=$(background_start_color)
glow_la_LDFLAGS =   -module -avoid-version -export-dynamic
glow_la_LIBADD =    $(PLYMOUTH_LIBS)                                         \
                    ../../../libply/libply.la                                 \
                    ../../../libplybootsplash/libplybootsplash.la
glow_la_SOURCES = $(srcdir)/plugin.c

plymouthdir = $(datadir)/plymouth/glow
plymouth_DATA = \
	images/box.png			\
	images/bullet.png		\
	images/entry.png		\
	images/lock.png			\
	images/progress-00.png		\
	images/progress-01.png		\
	images/progress-02.png		\
	images/progress-03.png		\
	images/progress-04.png		\
	images/progress-05.png		\
	images/progress-06.png		\
	images/progress-07.png		\
	images/progress-08.png		\
	images/progress-09.png		\
	images/progress-10.png		\
	images/progress-11.png		\
	images/progress-12.png		\
	images/progress-13.png		\
	images/progress-14.png		\
	images/progress-15.png		\
	images/progress-16.png		\
	images/progress-17.png		\
	images/progress-18.png		\
	images/progress-19.png		\
	images/progress-20.png		\
	images/progress-21.png		\
	images/progress-22.png		\
	images/progress-23.png		\
	images/progress-24.png		\
	images/progress-25.png		\
	images/progress-26.png		\
	images/progress-27.png		\
	images/progress-28.png		\
	images/progress-29.png		\
	images/progress-30.png		\
	images/progress-31.png		\
	images/progress-32.png		\
	images/throbber-00.png		\
	images/throbber-01.png		\
	images/throbber-02.png		\
	images/throbber-03.png		\
	images/throbber-04.png		\
	images/throbber-05.png		\
	images/throbber-06.png		\
	images/throbber-07.png		\
	images/throbber-08.png		\
	images/throbber-09.png		\
	images/throbber-10.png		\
	images/throbber-11.png		\
	images/throbber-12.png		\
	images/throbber-13.png		\
	images/throbber-14.png		\
	images/throbber-15.png		\
	images/throbber-16.png		\
	images/throbber-17.png		\
	images/throbber-18.png		\
	images/throbber-19.png

EXTRA_DIST = $(plymouth_DATA)
MAINTAINERCLEANFILES = Makefile.in
