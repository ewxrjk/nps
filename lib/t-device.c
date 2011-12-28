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
#include "utils.h"
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <sys/stat.h>

static void check(const char *path) {
  struct stat s;
  const char *p;
  if(stat(path, &s) < 0)
    fatal(errno, "stat %s", path);
  p = device_path(S_ISBLK(s.st_mode), s.st_rdev);
  assert(!strcmp(p, path));
}

int main() {
  /* A bunch of character devices which are reasonably likely to be
   * present */
  check("/dev/null");
  check("/dev/tty");
  check("/dev/zero");
  check("/dev/full");
  check("/dev/random");
  check("/dev/urandom");
  /* loop0 seems to be there even if you're not using any loop
   * devices, so let's use that */
  check("/dev/loop0");
  return 0;
}
