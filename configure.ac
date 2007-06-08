AC_INIT(plymouth, 0.0.1, "rstrode@redhat.com")
AC_CONFIG_SRCDIR(src/main.c)
AC_CONFIG_HEADER(config.h)

AC_GNU_SOURCE
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
AM_PROG_LIBTOOL

## increment if the interface has additions, changes, removals.
LT_CURRENT=1

## increment any time the source changes; set to
##  0 if you increment CURRENT
LT_REVISION=0

## increment if any interfaces have been added; set to 0
## if any interfaces have been changed or removed. removal has
## precedence over adding, so set to 0 if both happened.
LT_AGE=0

AC_SUBST(LT_CURRENT)
AC_SUBST(LT_REVISION)
AC_SUBST(LT_AGE)

PKG_CHECK_MODULES(IMAGE, [libpng >= 1.2.16 ]) 
AC_SUBST(IMAGE_CFLAGS)
AC_SUBST(IMAGE_LIBS)

PLYMOUTH_CFLAGS="$IMAGE_CFLAGS"
PLYMOUTH_LIBS="-lm -ldl $IMAGE_LIBS"

AC_SUBST(PLYMOUTH_CFLAGS)
AC_SUBST(PLYMOUTH_LIBS)

AC_OUTPUT([Makefile
           src/libply/Makefile
           src/splash-plugins/Makefile
           src/splash-plugins/fedora-fade-in/Makefile
           src/Makefile
           src/rhgb-client/Makefile
           src/tests/Makefile
           src/libply/tests/Makefile
           src/rhgb-client/tests/Makefile
           src/splash-plugins/tests/Makefile
	   scripts/Makefile
           po/Makefile.in
])
