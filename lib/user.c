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
#include "utils.h"
#include <pwd.h>
#include <grp.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>

const char *forceusers;
const char *forcegroups;

struct userinfo {
  char *name;
  int id;
};

static int lookup_generic(const char *path,
                          int id,
                          const char *name,
                          struct userinfo *result);
static char *lookup_generic_by_id(const char *path, int id);
static int lookup_generic_by_name(const char *path, const char *what, 
                                  const char *name);

const char *lookup_user_by_id(uid_t uid) {
  if(!forceusers) {
    struct passwd *pw = getpwuid(uid);
    return pw ? pw->pw_name : NULL;
  } else
    return lookup_generic_by_id(forceusers, uid);
}

const char *lookup_group_by_id(gid_t gid) {
  if(!forcegroups) {
    struct group *gr = getgrgid(gid);
    return gr ? gr->gr_name : NULL;
  } else
    return lookup_generic_by_id(forcegroups, gid);
}

static char *lookup_generic_by_id(const char *path, int id) {
  struct userinfo u[1];
  if(lookup_generic(path, id, NULL, u))
    return u->name;
  else
    return NULL;
}

uid_t lookup_user_by_name(const char *name) {
  if(!forceusers) {
    struct passwd *pw = getpwnam(name);
    if(pw)
      return pw->pw_uid;
    else
      fatal(0, "unknown user '%s'", name);
  } else
    return lookup_generic_by_name("user", forceusers, name);
}

uid_t lookup_group_by_name(const char *name) {
  if(!forcegroups) {
    struct group *gr = getgrnam(name);
    if(gr)
      return gr->gr_gid;
    else
      fatal(0, "unknown group '%s'", name);
  } else
    return lookup_generic_by_name("group", forcegroups, name);
}

static int lookup_generic_by_name(const char *what, const char *path,
                                  const char *name) {
  struct userinfo u[1];
  if(lookup_generic(path, -1, name, u))
    return u->id;
  else
    fatal(0, "unknown %s '%s'", what, name);
}

static int lookup_generic(const char *path,
                          int id,
                          const char *name,
                          struct userinfo *result) {
  static char namebuf[128];
  char buffer[256];
  FILE *fp = xfopen(path, "r");
  int n;
  while(fgets(buffer, sizeof buffer, fp)) {
    char *s = strchr(buffer, ':');
    if(!s || s > buffer + (sizeof namebuf) / 2)
      continue;
    n = atoi(s + 1);
    if(id != -1 && n == id) {
      memcpy(namebuf, buffer, s - buffer);
      namebuf[s - buffer] = 0;
      fclose(fp);
      result->name = namebuf;
      return 1;
    }
    if(name && s - buffer == (ptrdiff_t)strlen(name)
       && !strncmp(buffer, name, s - buffer)) {
      result->id = n;
      fclose(fp);
      return 1;
    }
  }
  fclose(fp);
  return 0;
}
