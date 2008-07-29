SUBDIRS = .

INCLUDES = -I$(top_srcdir)                                                    \
           -I$(srcdir)
plymouth_log_viewerdir = $(bindir)
plymouth_log_viewer_PROGRAMS = plymouth-log-viewer

plymouth_log_viewer_CFLAGS = $(GTK_CFLAGS) -DPLYMOUTH_LOG_DIRECTORY=\"$(localstatedir)/log\"
plymouth_log_viewer_LDADD = $(GTK_LIBS)
plymouth_log_viewer_SOURCES = $(srcdir)/plymouth-log-viewer.c

MAINTAINERCLEANFILES = Makefile.in
