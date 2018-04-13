#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.69)
AC_INIT(ehttpd, 0.03, lari.korpi@iki.fi)
AC_CONFIG_SRCDIR(config.h.in)
AC_CONFIG_HEADERS(config.h)
AM_INIT_AUTOMAKE

# Checks for programs.
AC_PROG_CC

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS(arpa/inet.h stdlib.h string.h sys/socket.h unistd.h)

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_PID_T
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_FORK
AC_CHECK_FUNCS(memset regcomp socket strtol)

AC_CONFIG_FILES(Makefile
                 source/Makefile)
AC_OUTPUT
