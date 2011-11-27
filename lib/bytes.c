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
#include "general.h"

char *bytes(uintmax_t n,
            int fieldwidth,
            int ch,
            char buffer[],
            size_t bufsize) {
  if(!ch) {
    if(n < KILOBYTE)
      ch = 0;
    else if(n < MEGABYTE)
      ch = -'K';
    else if(n < GIGABYTE)
      ch = -'M';
    else if(n < TERABYTE)
      ch = -'G';
    else if(n < PETABYTE)
      ch = -'T';
    else
      ch = -'P';
  }
  switch(abs(ch)) {
  case 'K': n /= KILOBYTE; break;
  case 'M': n /= MEGABYTE; break;
  case 'G': n /= GIGABYTE; break;
  case 'T': n /= TERABYTE; break;
  case 'P': n /= PETABYTE; break;
  case 'p': n /= sysconf(_SC_PAGESIZE); break;
  }
  if(ch < 0)
    snprintf(buffer, bufsize, "%*ju%c", fieldwidth - 1, n, -ch);
  else
    snprintf(buffer, bufsize, "%*ju", fieldwidth, n);
  return buffer;
}

