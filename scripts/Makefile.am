noinst_SCRIPTS = new-object.sh 

initrdscriptdir = $(libexecdir)/plymouth
initrdscript_SCRIPTS = plymouth-update-initrd plymouth-populate-initrd

if ADD_BOOT_ENTRY
install-data-hook:
	PLYMOUTH_LOGO_FILE=$(logofile) $(initrdscriptdir)/plymouth-update-initrd
endif

EXTRA_DIST=$(initrdscript_SCRIPTS) $(noinst_SCRIPTS) default.cfg
MAINTAINERCLEANFILES = Makefile.in
