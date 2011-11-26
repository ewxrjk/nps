/*
 * This file is part of nps.
 * Copyright (C) 2011 Richard Kettlewell
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */
#include <config.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include "format.h"

char *bytes(uintmax_t n,
            int fieldwidth,
            int ch,
            char buffer[],
            size_t bufsize) {
  if(!ch) {
    if(n < 1024)
      ch = 0;
    else if(n < 1024 * 1024)
      ch = -'K';
    else if(n < 1024 * 1024 * 1024)
      ch = -'M';
    else if(n < (uintmax_t)1024 * 1024 * 1024 * 1024)
      ch = -'G';
    else
      ch = -'T';
  }
  switch(abs(ch)) {
  case 'K': n /= 1024; break;
  case 'M': n /= (1024 * 1024); break;
  case 'G': n /= (1024 * 1024 * 1024); break;
  case 'T': n /= ((uintmax_t)1024 * 1024 * 1024 * 1024); break;
  case 'p': n /= sysconf(_SC_PAGESIZE); break;
  }
  if(ch < 0)
    snprintf(buffer, bufsize, "%*ju%c", fieldwidth - 1, n, -ch);
  else
    snprintf(buffer, bufsize, "%*ju", fieldwidth, n);
  return buffer;
}

int bytes_ch(const char *name) {
  const char *p = name + strlen(name) - 1;
  switch(*p) {
  case 'K': case 'M': case 'G': return *p;
  case 'P': return 'p';
  default: return 0;
  }
}
