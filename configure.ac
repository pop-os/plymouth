AC_INIT(plymouth, 0.0.1, "rstrode@redhat.com")
AC_CONFIG_SRCDIR(src/main.c)
AC_CONFIG_HEADER(config.h)
AC_CONFIG_AUX_DIR(build-tools)

AC_GNU_SOURCE
AC_PROG_AWK
AC_PROG_CC
AM_PROG_CC_C_O
AC_HEADER_STDC
AC_C_CONST

AM_INIT_AUTOMAKE([dist-bzip2 no-dist-gzip])
AM_MAINTAINER_MODE

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

AC_ARG_ENABLE(tracing, AS_HELP_STRING([--enable-tracing],[enable verbose tracing code]),enable_tracing=$enableval,enable_tracing=yes)

if test x$enable_tracing = xyes; then
  AC_DEFINE(PLY_ENABLE_TRACING, 1, [Build in verbose debug tracing spew])
fi

AC_ARG_WITH(boot-entry, AS_HELP_STRING([--with-boot-entry],[create initrd and grub entry]),with_boot_entry=$withval,with_boot_entry=yes)

AM_CONDITIONAL(ADD_BOOT_ENTRY,  [test "$with_boot_entry" = yes])

AC_ARG_WITH(logo, AC_HELP_STRING([--with-logo], [logo used by boot splash plugins]),logofile=${withval},logofile=$datadir/pixmaps/system-logo-white.png)
AC_SUBST(logofile)

AC_ARG_WITH(background-color, AC_HELP_STRING([--with-background-color], [background color used by boot splash plugins]),background_color=${withval},background_color=0x005391)
AC_SUBST(background_color)

AC_OUTPUT([Makefile
           src/libply/Makefile
           src/splash-plugins/Makefile
           src/splash-plugins/spinfinity/Makefile
           src/splash-plugins/fade-in/Makefile
           src/splash-plugins/text/Makefile
           src/splash-plugins/details/Makefile
           src/Makefile
           src/client/Makefile
           src/tests/Makefile
           src/libply/tests/Makefile
           src/client/tests/Makefile
	   scripts/Makefile
])
