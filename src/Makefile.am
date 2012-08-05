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
noinst_PROGRAMS=snapshot
man_MANS=nps.1 nps-top.1

TESTS=t-ps

nps_SOURCES=ps.c threads.h
nps_LDADD=../lib/libps.a

nps_top_SOURCES=top.c input.h input.c threads.h
nps_top_LDADD=../lib/libps.a $(LIBCURSES) $(LIBM)

snapshot_SOURCES=snapshot.c
snapshot_LDADD=../lib/libps.a

AM_CPPFLAGS=-I${top_srcdir}/lib

${srcdir}/nps.1: nps.1.in mkmanpage formatting.1
	${srcdir}/mkmanpage ${srcdir}/nps.1.in > ${srcdir}/nps.1.new
	rm -f ${srcdir}/nps.1
	mv ${srcdir}/nps.1.new ${srcdir}/nps.1
	chmod 444 ${srcdir}/nps.1

${srcdir}/nps-top.1: nps-top.1.in mkmanpage formatting.1 sysinfo.1
	${srcdir}/mkmanpage ${srcdir}/nps-top.1.in > ${srcdir}/nps-top.1.new
	rm -f ${srcdir}/nps-top.1
	mv ${srcdir}/nps-top.1.new ${srcdir}/nps-top.1
	chmod 444 ${srcdir}/nps-top.1

EXTRA_DIST=${man_MANS} formatting.1 nps.1.in nps-top.1.in formatting.1	\
sysinfo.1 mkmanpage t-ps testdata

maintainer-clean-local:
	rm -f ${srcdir}/nps.1 ${srcdir}/nps-top.1

clean-local:
	rm -f *.gcno *.gcda *.gcov

