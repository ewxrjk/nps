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
#include "format.h"

char *bytes(uintmax_t n,
            int fieldwidth,
            char buffer[],
            size_t bufsize) {
  int ch;
  if(n < 1024)
    ch = 0;
  else if(n < 1024 * 1024) {
    n /= 1024;
    ch = 'K';
  } else if(n < 1024 * 1024 * 1024) {
    n /= (1024 * 1024);
    ch = 'M';
  } else {
    n /= (1024 * 1024 * 1024);
    ch = 'G';
  }
  snprintf(buffer, bufsize, "%*ju%c", fieldwidth, n, ch);
  return buffer;
}
