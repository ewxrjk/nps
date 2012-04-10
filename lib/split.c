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
  else {
    struct passwd *pw = getpwnam(s);
    if(pw)
      a.uid = pw->pw_uid;
    else
      fatal(0, "unknown user '%s'", s);
  }
  return a; 
}

union arg arg_group(const char *s) {
  union arg a;
  if(strspn(s, "0123456789") == strlen(s)) 
    a.gid = atol(s);
  else {
    struct group *gr = getgrnam(s);
    if(gr)
      a.gid = gr->gr_gid;
    else
      fatal(0, "unknown group '%s'", s);
  }
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
  struct stat sb;
  char buffer[1024];

  if(s[0] != '/') {
    if(s[0] >= '0' && s[0] <= '9')
      snprintf(buffer, sizeof buffer, "/dev/tty%s", s);
    else
      snprintf(buffer, sizeof buffer, "/dev/%s", s);
    s = buffer;
  }
  if(stat(s, &sb) < 0)
    fatal(errno, "unrecognized tty %s", s);
  if(!S_ISCHR(sb.st_mode))
    fatal(0, "%s is not a terminal", s);
  a.tty = sb.st_rdev;
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
