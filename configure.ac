#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([FULL-PACKAGE-NAME], [VERSION], [BUG-REPORT-ADDRESS])
AC_CONFIG_SRCDIR([config.h.in])
AC_CONFIG_HEADERS([config.h])

AM_INIT_AUTOMAKE([foreign subdir-objects -Wall -Werror])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.
PTHREAD_FLAGS="-lpthread"
PTHREAD_LIBS=-lpthread
AC_SUBST(PTHREAD_FLAGS)
AC_SUBST(PTHREAD_LIBS)

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h stdint.h stdlib.h string.h sys/ioctl.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_INT16_T
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AC_TYPE_INT8_T
AC_TYPE_SIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T
AC_CHECK_TYPES([ptrdiff_t])

# ARMv8 Cryptography Extension
crypto_ext="yes"

if test "x$crypto_ext" = xyes; then
	AC_DEFINE([USE_CRYPTO_EXT], [1], [Defined to 1 if ARMv8 Cryptography Extension support is wanted])
fi
AM_CONDITIONAL([HAVE_CRYPTO_EXT], [test x$crypto_ext = xyes])

btc08_fpga="no"

AC_ARG_ENABLE([btc08_fpga],
	[AC_HELP_STRING([--enable-btc08-fpga],[Compile support for BTC08 FPGA (default disabled)])],
	[btc08_fpga=$enableval]
	)
if test "x$btc08_fpga" = xyes; then
	AC_DEFINE([USE_BTC08_FPGA], [1], [Defined to 1 for using BTC08 FPGA])
fi
AM_CONDITIONAL([HAVE_BTC08_FPGA], [test x$btc08_fpga = xyes])

# Checks for library functions.
AC_FUNC_MALLOC

AC_CONFIG_FILES([
    Makefile
    adc_test/Makefile
    btc08_test/Makefile
])

AC_OUTPUT
