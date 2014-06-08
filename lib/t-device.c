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
#include "utils.h"
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <sys/stat.h>
#include <stdio.h>

static int check(const char *path) {
  struct stat s;
  const char *p;
  if(stat(path, &s) < 0)        /* skip nonexistent devices */
    return 0;
  p = device_path(S_ISBLK(s.st_mode), s.st_rdev);
  assert(!strcmp(p, path));
  return 1;
}

int main() {
  int chardevs_checked = 0, blockdevs_checked = 0, rc = 0;
  /* A bunch of character devices which are reasonably likely to be
   * present */
  chardevs_checked += check("/dev/null");
  chardevs_checked += check("/dev/tty");
  chardevs_checked += check("/dev/zero");
  chardevs_checked += check("/dev/full");
  chardevs_checked += check("/dev/random");
  chardevs_checked += check("/dev/urandom");
  if(chardevs_checked == 0) {
    fprintf(stderr, "-- couldn't find any character devices to check\n");
    rc = 77;
  }
  /* A bunch of block devices which might be present */
  blockdevs_checked += check("/dev/sda");
  blockdevs_checked += check("/dev/hda");
  blockdevs_checked += check("/dev/vda");
  blockdevs_checked += check("/dev/loop0");
  blockdevs_checked += check("/dev/dm0");
  blockdevs_checked += check("/dev/dm-0");
  blockdevs_checked += check("/dev/sr0");
  blockdevs_checked += check("/dev/md127");
  if(blockdevs_checked == 0) {
    fprintf(stderr, "-- couldn't find any block devices to check\n");
    rc = 77;
  }
  return rc;
}
