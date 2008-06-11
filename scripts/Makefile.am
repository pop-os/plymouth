noinst_SCRIPTS = new-object.sh 

initrdscriptdir = $(libexecdir)/plymouth
initrdscript_SCRIPTS = plymouth-update-initrd plymouth-populate-initrd

sbinscriptsdir = $(sbindir)
sbinscripts_SCRIPTS = plymouth-set-default-plugin

plymouth-update-initrd: plymouth-update-initrd.in
	sed -e 's,[@]logofile[@],$(logofile),g' plymouth-update-initrd.in > plymouth-update-initrd

plymouth-populate-initrd: plymouth-populate-initrd.in
	sed -e 's,[@]logofile[@],$(logofile),g' plymouth-populate-initrd.in > plymouth-populate-initrd

if ADD_BOOT_ENTRY
install-data-hook: plymouth-update-initrd plymouth-populate-initrd
	PLYMOUTH_LOGO_FILE=$(logofile) $(initrdscriptdir)/plymouth-update-initrd
endif

EXTRA_DIST=$(initrdscript_SCRIPTS) $(sbinscripts_SCRIPTS) $(noinst_SCRIPTS) default.cfg
MAINTAINERCLEANFILES = Makefile.in plymouth-update-inird plymouth-populate-initrd
