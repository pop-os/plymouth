noinst_SCRIPTS = new-object.sh 

initrdscriptdir = $(libexecdir)/plymouth
initrdscript_SCRIPTS = plymouth-update-initrd

install-data-hook: 
	$(DESTDIR)$(initrdscriptdir)/plymouth-update-initrd

EXTRA_DIST=$(initrdscript_SCRIPTS) $(noinst_SCRIPTS) default.cfg
MAINTAINERCLEANFILES = Makefile.in
