noinst_SCRIPTS = new-object.sh 

initrdscriptdir = $(libexecdir)/plymouth
initrdscript_SCRIPTS = plymouth-update-initrd

install-data-hook: 
	$(initrdscriptdir)/plymouth-update-initrd

EXTRA_DIST=$(initrdscript_SCRIPTS)
MAINTAINERCLEANFILES = Makefile.in
