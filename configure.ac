# This file is part of nps.
# Copyright (C) 2011 Richard Kettlewell
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
# USA
AC_PREREQ([2.61])
AC_INIT([nps], [0.DEV], [rjk@greenend.org.uk])
AC_CONFIG_AUX_DIR([config.aux])
AM_INIT_AUTOMAKE([1.10])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES])
AC_CONFIG_SRCDIR([src/ps.c])
AM_CONFIG_HEADER([config.h])
AC_LANG([C])
AC_PROG_CC
AC_PROG_RANLIB
AC_SET_MAKE
AC_C_BIGENDIAN
AC_CHECK_FUNCS([getc_unlocked])
AC_CHECK_LIB([ncurses],[initscr],[LIBCURSES=-lncurses])
AC_CHECK_LIB([rt],[clock_gettime])
AC_SUBST([LIBCURSES])
AC_CHECK_LIB([m],[floor],[LIBM=-lm])
AC_SUBST([LIBM])
AC_DEFINE([_GNU_SOURCE], [1], [use GNU extensions])
if test "x$GCC" = xyes; then
  CC="$CC -Wall -W -Werror -Wpointer-arith -Wwrite-strings -Wmissing-prototypes -Wmissing-declarations -Wnested-externs -Wshadow"
fi

AH_BOTTOM([#ifdef __GNUC__
# define attribute(x) __attribute__(x)
#else
# define attribute(x)
#endif])

AC_CONFIG_FILES([Makefile lib/Makefile src/Makefile])
AC_OUTPUT
