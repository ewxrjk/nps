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
#include <ctype.h>
#include "format.h"
#include "general.h"

char *bytes(uintmax_t n,
            int fieldwidth,
            int ch,
            char buffer[],
            size_t bufsize,
            unsigned cutoff) {
  if(!ch) {
    if(!cutoff)
      cutoff = 1;
    if(n < KILOBYTE * cutoff)
      ch = 0;
    else if(n < MEGABYTE * cutoff)
      ch = -'K';
    else if(n < GIGABYTE * cutoff)
      ch = -'M';
    else if(n < TERABYTE * cutoff)
      ch = -'G';
    else if(n < PETABYTE * cutoff)
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

int parse_byte_arg(const char *arg, unsigned *cutoff, unsigned flags) {
  *cutoff = 1;
  if(flags & FORMAT_RAW)
    return 'b';
  else if(arg) {
    if(isdigit((unsigned char)arg[0])) {
      *cutoff = atoi(arg);
      return 0;
    } else
      return *arg;
  } else
    return 0;
}

