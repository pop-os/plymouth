# the versioning scheme is 0.0.1-unstable
# followed by 0.1.0 for initial release
# followed by 0.1.0-stable and 0.1.0-unstable
# followed by 0.1.1 and 0.2.0
# followed by 0.1.1-stable and 0.2.0-unstable
# etc., etc.
AC_INIT(plymouth, 0.0.1-unstable, "rstrode@redhat.com")
AC_CONFIG_SRCDIR(src/main.c)
AC_CONFIG_HEADER(config.h)

AC_PROG_AWK
AC_PROG_CC
AM_PROG_CC_C_O
AC_HEADER_STDC
AC_C_CONST

AM_INIT_AUTOMAKE([dist-bzip2])
AM_MAINTAINER_MODE

GETTEXT_PACKAGE=plymouth
AC_DEFINE([GETTEXT_PACKAGE], [], "graphical boot splash")
AC_SUBST(GETTEXT_PACKAGE)
AM_GLIB_GNU_GETTEXT
ALL_LINGUAS=""

IT_PROG_INTLTOOL
PKG_PROG_PKG_CONFIG

PKG_CHECK_MODULES(PLYMOUTH, [glib-2.0   >= 2.12.11 
                             cairo      >= 1.4.2   
]) 
AC_SUBST(PLYMOUTH_CFLAGS)
AC_SUBST(PLYMOUTH_LIBS)

AC_PATH_PROG(GLIB_GENMARSHAL, glib-genmarshal)

AC_OUTPUT([Makefile
           src/Makefile
           po/Makefile.in
])
