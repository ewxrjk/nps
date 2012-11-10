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
#include "selectors.h"
#include "user.h"
#include "utils.h"
#include <pwd.h>
#include <grp.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>

union arg arg_user(const char *s) {
  union arg a;
  if(strspn(s, "0123456789") == strlen(s)) 
    a.uid = atol(s);
  else
    a.uid = lookup_user_by_name(s);
  return a; 
}

union arg arg_group(const char *s) {
  union arg a;
  if(strspn(s, "0123456789") == strlen(s)) 
    a.gid = atol(s);
  else
    a.gid = lookup_group_by_name(s);
  return a; 
}

union arg arg_process(const char *s) {
  union arg a;
  if(strspn(s, "0123456789") == strlen(s)) 
    a.pid = atol(s);
  else
    fatal(0, "invalid process ID '%s'", s);
  return a; 
}

union arg arg_tty(const char *s) {
  union arg a;

  if((a.tty = tty_id(s)) == -1)
    fatal(errno, "unrecognized tty %s", s);
  return a;
}

union arg *split_arg(char *arg,
                     union arg (*type)(const char *s),
                     size_t *lenp) {
  const char *start;
  union arg *results = NULL;
  union arg v;
  size_t nslots = 0, nresults = 0;
  while(*arg) {
    if(*arg == ' ' || *arg == ',') {
      ++arg;
      continue;
    }
    start = arg;
    while(*arg && *arg != ' ' && *arg != ',')
      ++arg;
    if(*arg)
      *arg++ = 0;
    v = type(start);
    if(nresults >= nslots) {
      if((ssize_t)(nslots = nslots ? 2 * nslots : 16) <= 0)
        fatal(0, "too many objects");
      results = xrecalloc(results, nslots, sizeof *results);
    }
    results[nresults++] = v;
  }
  *lenp = nresults;
  return results;
}
