INCLUDES =                                                                    \
           -I$(top_srcdir)                                                    \
           -I$(srcdir)/..                                                     \
           -I$(srcdir)
TESTS = 

include $(srcdir)/ply-frame-buffer-test.am
include $(srcdir)/ply-image-test.am
include $(srcdir)/ply-terminal-test.am
include $(srcdir)/ply-terminal-session-test.am
include $(srcdir)/ply-init-control-test.am
include $(srcdir)/ply-logger-test.am
include $(srcdir)/ply-list-test.am

noinst_PROGRAMS = $(TESTS)
