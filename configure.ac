#			-*- Autoconf -*-
# Process this file with autoconf to produce a configure script.
AC_PREREQ([2.69])

#base
AC_INIT([lsvpd],[1.7.15],[mahesh@linux.ibm.com,sv@linux.ibm.com])
AC_CONFIG_HEADER([config/config.h])
AC_SUBST(DATE, [`date`])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([config])
AM_INIT_AUTOMAKE([-Wall subdir-objects -Werror foreign])
LT_INIT

#programs
AC_PROG_LIBTOOL
AC_LANG([C++])
AC_PROG_CXX
AC_C_CONST
AC_C_INLINE
AC_HEADER_STDBOOL
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET

AC_COMPILE_IFELSE(
		  [AC_LANG_PROGRAM([int i;])],
		  [],
		  [AC_MSG_ERROR([C++ compiler is missing])],
		  )
#headers
AC_HEADER_STDBOOL
AC_TYPE_SIZE_T
AC_HEADER_DIRENT
AC_HEADER_STDC
AC_CHECK_HEADERS([arpa/inet.h fcntl.h stdlib.h string.h sys/ioctl.h sys/param.h syslog.h unistd.h ctype.h regex.h])
AC_CHECK_HEADER([scsi/sg_cmds.h], ,[AC_MSG_ERROR([sg_utils header files are required for building lsvpd])])
AC_CHECK_HEADER([sqlite3.h],,[AC_MSG_ERROR([sqlite header files are required for building lsvpd])])
AC_CHECK_HEADER([zlib.h],,[AC_MSG_ERROR([zlib header files are required for building lsvpd])])
AC_CHECK_HEADER([libvpd-2/lsvpd.hpp],,[AC_MSG_ERROR([libvpd header files required for building lsvpd])])

#libraries
AC_CHECK_LIB(rtas, rtas_get_vpd, [], [
			echo "rtas library is required for lsvpd"
			exit 1 ])
AC_CHECK_LIB(sqlite3, sqlite3_libversion_number, [], [
			echo "sqlite3 library is required for lsvpd"
			exit 1 ])

AC_CHECK_LIB(sgutils,sg_lib_version, [SGUTILS_LIB="sgutils" ],[])
AC_CHECK_LIB(sgutils2,sg_lib_version, [SGUTILS_LIB="sgutils2"],[])

#Can't use AM_COND_IF unless you have automake v1.11 or later.
#AM_CONDITIONAL([SGUTIL0], [ test x$SGUTILS_LIB != x ])
#AM_COND_IF([SGUTIL0], [], [
#			echo "sgutils library is required for lsvpd"
#			exit 1 ])
AM_CONDITIONAL([SGUTIL1], [ test x$SGUTILS_LIB = xsgutils ])
AM_CONDITIONAL([SGUTIL2], [ test x$SGUTILS_LIB = xsgutils2 ])
PKG_CHECK_MODULES([LIBVPD2], [libvpd_cxx-2 >= 2.2.9],[],[
			echo "VPD library(libvpd) version 2.2.9 is required for lsvpd"
			exit 1])

AC_FUNC_CLOSEDIR_VOID
AC_PROG_GCC_TRADITIONAL
AC_FUNC_LSTAT
AC_FUNC_LSTAT_FOLLOWS_SLASHED_SYMLINK
AC_HEADER_MAJOR
AC_FUNC_MALLOC
AC_FUNC_STAT
AC_CHECK_FUNCS([memmove memset mkdir socket strdup strerror strtol uname strchr strndup])

#finished
AC_CONFIG_FILES([Makefile lsvpd.spec])
AC_OUTPUT
