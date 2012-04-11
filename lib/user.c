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
#include "user.h"
#include "io.h"
#include <pwd.h>
#include <grp.h>
#include <string.h>
#include <stdlib.h>

const char *forceusers;
const char *forcegroups;

static char *lookup_generic(const char *path, int id);

const char *lookup_user(uid_t uid) {
  if(!forceusers) {
    struct passwd *pw = getpwuid(uid);
    return pw ? pw->pw_name : NULL;
  } else
    return lookup_generic(forceusers, uid);
}

const char *lookup_group(gid_t gid) {
  if(!forcegroups) {
    struct group *gr = getgrgid(gid);
    return gr ? gr->gr_name : NULL;
  } else
    return lookup_generic(forceusers, gid);
}

static char *lookup_generic(const char *path, int id) {
  static char name[128];
  char buffer[256];
  FILE *fp = xfopen(path, "r");
  int n;
  while(fgets(buffer, sizeof buffer, fp)) {
    char *s = strchr(buffer, ':');
    if(!s || s > buffer + (sizeof name) / 2)
      continue;
    n = atoi(s + 1);
    if(n == id) {
      memcpy(name, buffer, s - buffer);
      name[s - buffer] = 0;
      fclose(fp);
      return name;
    }
  }
  fclose(fp);
  return NULL;
}
