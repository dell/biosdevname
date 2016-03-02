#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.
# vim:tw=0:ts=8:sw=8:et

AC_PREREQ(2.59)
AC_INIT([biosdevname],[0.7.2],[Jordan_Hargrave@dell.com])
AC_LANG([C])
AC_USE_SYSTEM_EXTENSIONS
AC_CONFIG_SRCDIR([src/read_proc.c])
AC_CONFIG_HEADER([config.h])
AC_PREFIX_DEFAULT()
AM_INIT_AUTOMAKE
AM_MAINTAINER_MODE

# Checks for programs.
AC_PROG_AWK
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_LN_S
AC_PROG_CXX
AC_PROG_RANLIB
AC_CHECK_PROGS([READELF], [readelf])
AC_CHECK_FUNCS([realpath])
# Checks for libraries.
AC_CHECK_LIB([c], [snprintf])
# libz has to come before libpci because libpci needs it now.
AC_CHECK_LIB([z], [gzopen], [true],
	    [AC_MSG_ERROR([You must install zlib-devel (Fedora/Red Hat/SuSE), libzip-dev (Debian/Ubuntu), or equivalent])])
AC_CHECK_LIB([pci], [pci_alloc], [true],
	    [AC_MSG_ERROR([You must install pciutils-devel (Fedora/Red Hat/SuSE), pciutils-dev (Debian/Ubuntu), or equivalent])],
	    -lz)

# Checks for header files.
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS([fcntl.h limits.h locale.h stddef.h stdint.h stdlib.h string.h sys/ioctl.h sys/time.h syslog.h unistd.h])
AC_CHECK_HEADER([pci/pci.h], ,
	    [AC_MSG_ERROR([You must install pciutils-devel or equivalent])])
AC_CHECK_HEADER([zlib.h], ,
	    [AC_MSG_ERROR([You must install zlib-devel or equivalent])])

# Check for pci_dev.domain
AC_CHECK_MEMBERS([struct pci_dev.domain], , , [#include<pci/pci.h>])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT8_T
AC_TYPE_UINT16_T
AC_HEADER_TIME
AC_STRUCT_TM

# Checks for library functions.
AC_FUNC_ALLOCA
AC_FUNC_FORK
AC_PROG_GCC_TRADITIONAL
AC_FUNC_MALLOC
AC_FUNC_MEMCMP
AC_FUNC_MMAP
AC_FUNC_REALLOC
AC_FUNC_SELECT_ARGTYPES
AC_FUNC_STAT
AC_FUNC_VPRINTF
AC_CHECK_FUNCS([dup2 gettimeofday memset munmap select socket strcasecmp strchr strdup strerror strncasecmp strpbrk strrchr strstr strtol strtoul])

# this is ugly, but accounts for SLES 10, Red Hat/Fedora, and Ubuntu
# handles default udev rules as of udev 114 or thereabouts
RULEDEST=/lib/udev/rules.d/71-biosdevname.rules

if [[ -e /etc/udev/rules.d/60-net.rules ]]; then
     # RHEL 5 / Fedora
     RULEDEST=/etc/udev/rules.d/60-biosdevname.rules
elif [[ -e /etc/udev/rules.d/31-network.rules ]]; then
     # SLES 10
     RULEDEST=/etc/udev/rules.d/31-biosdevname.rules
elif [[ -e /etc/udev/rules.d/25-iftab.rules ]]; then
     # older Ubuntu
     RULEDEST=/etc/udev/rules.d/25-biosdevname.rules
fi
RULEDIR=$(dirname $RULEDEST)
AC_SUBST(RULEDEST, $RULEDEST)
AC_SUBST(RULEDIR,  $RULEDIR)

echo "PACKAGE_NAME='$PACKAGE_NAME'" > version
echo "PACKAGE_VERSION='$PACKAGE_VERSION'" >> version
echo "PACKAGE_STRING='$PACKAGE_STRING'" >> version

AC_CONFIG_FILES([Makefile biosdevname.spec.fedora biosdevname.spec.suse src/bios_dev_name.h])
AC_OUTPUT
