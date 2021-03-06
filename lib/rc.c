/*
 * This file is part of nps.
 * Copyright (C) 2011, 2014 Richard Kettlewell
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
#include "rc.h"
#include "utils.h"
#include "priv.h"
#include "io.h"
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <pwd.h>
#include <unistd.h>
#include <assert.h>

#define RC_ITEM(X) \
  X(ps_f_format) \
  X(ps_format) \
  X(ps_l_format) \
  X(top_delay) \
  X(top_format) \
  X(top_order) \
  X(top_sysinfo)
#define RC_DEFINE(F) char *rc_##F;
#define RC_MAP(F) { #F, &rc_##F },

RC_ITEM(RC_DEFINE)

static const struct rc_map {
  const char *name;
  char **value;
} rc_map[] = {
  RC_ITEM(RC_MAP)
};
#define NRC (sizeof rc_map / sizeof *rc_map)

static const struct rc_map *rc_find(const char *key) {
  ssize_t l = 0, r = NRC-1, m;
  int c;
  while(l <= r) {
    m = l + (r - l) / 2;
    c = strcmp(key, rc_map[m].name);
    if(c < 0)
      r = m - 1;
    else if(c > 0)
      l = m + 1;
    else
      return &rc_map[m];
  }
  return NULL;
}

static char *rcpath(const char *extra) {
  const char *home = getenv("HOME");
  char *path;
  if(!home) {
    struct passwd *pw = getpwuid(priv_ruid);
    if(!pw || !pw->pw_dir)
      return NULL;
    home = pw->pw_dir;
  }
  xasprintf(&path, "%s/.npsrc%s", home, extra ? extra : "");
  return path;
}

void read_rc(void) {
  char *path = rcpath(NULL);
  if(!path)
    return;
  read_rc_path(path);
  free(path);
}

void read_rc_path(const char *path) {
  FILE *fp;
  char *ptr, *eq;
  char buffer[1024];
  int line = 0;
  const struct rc_map *m;

  assert(getuid() == geteuid());
  if(!(fp = fopen(path, "r"))) {
    if(errno != ENOENT)
      fatal(errno, "opening %s", path);
    return;
  }
  while(fgets(buffer, sizeof buffer, fp)) {
    ++line;
    /* Strip trailing whitespace */
    ptr = buffer + strlen(buffer);
    while(ptr > buffer && isspace((unsigned char)ptr[-1]))
      --ptr;
    *ptr = 0;
    /* Strip leading whitespace */
    ptr = buffer;
    while(*ptr && isspace((unsigned char)*ptr))
      ++ptr;
    /* Skip comments and blank lines */
    if(!*ptr || *ptr == '#')
      continue;
    /* Format is key=value */
    for(eq = ptr; *eq && *eq != '=' && !isspace((unsigned char)*eq); ++eq)
      ;
    /* Skip whitespace before the '=' */
    if(isspace((unsigned char)*eq)) {
      *eq++ = 0;
      while(isspace((unsigned char)*eq))
        ++eq;
    }
    if(*eq != '=')
      fatal(0, "%s:%d: missing '='", path, line);
    *eq++ = 0;
    /* Skip whitespace after the '=' */
    while(*eq && isspace((unsigned char)*eq))
      ++eq;
    /* Find the key */
    if(!(m = rc_find(ptr)))
      fatal(0, "%s:%d: unknown key '%s'", path, line, ptr);
    /* Replace the value */
    if(*m->value)
      free(*m->value);
    *m->value = xstrdup(eq);
  }
  if(ferror(fp))
    fatal(errno, "reading %s", path);
  fclose(fp);
}

void write_rc(void) {
  char *path = rcpath(NULL), *tmp = rcpath(".new");

  if(!path)
    fatal(0, "cannot determine path to .npsrc");
  write_rc_path(tmp);
  if(rename(tmp, path) < 0)
    fatal(errno, "renaming %s to %s", tmp, path);
  free(path);
  free(tmp);
}

void write_rc_path(const char *path) {
  FILE *fp;
  size_t n;

  assert(getuid() == geteuid());
  fp = xfopen(path, "w");
  for(n = 0; n < NRC; ++n) {
    if(*rc_map[n].value)
      if(fprintf(fp, "%s=%s\n", rc_map[n].name, *rc_map[n].value) < 0)
        fatal(errno, "writing %s", path);
  }
  xfclose(fp, path);
}

void reset_rc(void) {
  size_t n;

  for(n = 0; n < NRC; ++n) {
    free(*rc_map[n].value);
    *rc_map[n].value = NULL;
  }
}
