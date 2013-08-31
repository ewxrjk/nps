/*
 * This file is part of nps.
 * Copyright (C) 2011, 13 Richard Kettlewell
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
#include <stdlib.h>
#include <errno.h>
#include <math.h>
#include "utils.h"

double parse_interval(const char *s) {
  double interval;
  char *e;
  errno = 0;
  interval = strtod(s, &e);
  if(errno)
    fatal(errno, "invalid interval '%s'", s);
  if(e == s
     || *e
     || isnan(interval)
     || isinf(interval)
     || interval <= 0)
    fatal(0, "invalid interval '%s'", s);
  return interval;
}
