AC_PREREQ([2.69])
AC_INIT([nbd-proxy], 1.0, [jk@ozlabs.org])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([subdir-objects -Wall -Werror foreign dist-xz])
AM_SILENT_RULES([yes])

AC_PROG_CC
AM_PROG_AR
AC_PROG_INSTALL
AC_PROG_MAKE_SET

AX_APPEND_COMPILE_FLAGS([-Wall -Werror], [CFLAGS])

AC_CHECK_FUNCS(splice)

PKG_CHECK_MODULES(JSON, [json-c])
PKG_CHECK_MODULES(UDEV, [libudev])

AC_ARG_VAR(CONFIG_JSON, [The JSON file containing the device configurations.])
AS_IF([test "x$CONFIG_JSON" == "x"], \
    [CONFIG_JSON="$srcdir/config.json"])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
