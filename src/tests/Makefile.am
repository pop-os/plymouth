INCLUDES =                                                                    \
           -I$(top_srcdir)                                                    \
           -I$(srcdir)/..                                                     \
           -I$(srcdir)/../libply                                              \
           -I$(srcdir)
TESTS =

include $(srcdir)/ply-boot-server-test.am
include $(srcdir)/ply-boot-splash-test.am

noinst_PROGRAMS = $(TESTS)
MAINTAINERCLEANFILES = Makefile.in
