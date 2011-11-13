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
bin_PROGRAMS=nps nps-top
man_MANS=nps.1

nps_SOURCES=ps.c
nps_LDADD=../lib/libps.a

nps_top_SOURCES=top.c
nps_top_LDADD=../lib/libps.a $(CURSES)

AM_CPPFLAGS=-I${top_srcdir}/lib

EXTRA_DIST=${man_MANS}
