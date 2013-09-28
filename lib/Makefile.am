# This file is part of nps.
# Copyright (C) 2011, 13 Richard Kettlewell
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
noinst_LIBRARIES=libps.a
noinst_PROGRAMS=$(TESTS)
LDADD=libps.a
EXTRA_DIST=mainpage arch.svg

libps_a_SOURCES=buffer.h compare.h format.h general.h io.h parse.h	\
priv.h tasks.h rc.h selectors.h sysinfo.h utils.h buffer.c bytes.c	\
compare.c device.c error.c format.c io.c memory.c parse.c priv.c	\
tasks.c rc.c select.c selectors.c signal.c split.c sysinfo.c time.c	\
uptime.c user.h user.c parse_interval.c

TESTS=t-buffer t-bytes t-device t-error t-parse t-compare t-format	\
t-time t-priv t-uptime

clean-local:
	rm -f *.gcno *.gcda *.gcov

