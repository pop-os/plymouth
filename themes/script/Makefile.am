themedir = $(datadir)/plymouth/themes/script
dist_theme_DATA =                                                     \
                    script.plymouth                                   \
                    script.script


MAINTAINERCLEANFILES = Makefile.in script.plymouth
CLEANFILES = script.plymouth

script.plymouth: $(srcdir)/script.plymouth.in
	sed -e 's,[@]PLYMOUTH_THEME_PATH[@],$(PLYMOUTH_THEME_PATH),g' \
	       $(srcdir)/script.plymouth.in > script.plymouth

EXTRA_DIST = script.plymouth.in
