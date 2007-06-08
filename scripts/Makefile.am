SUBDIRS = 

noinst_SCRIPTS = new-object.sh 

scriptsdir = $(libexecdir)/plymouth
scripts_SCRIPTS = plymouth-update-initrd

install-exec-hook: 
	$(scriptsdir)/plymouth-update-initrd

MAINTAINERCLEANFILES = Makefile.in
