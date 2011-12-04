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

int main() {
  struct stat s;
  const char *p;
  if(stat("/dev/null", &s) < 0)
    fatal(errno, "stat /dev/null");
  p = device_path(0, s.st_rdev);
  assert(!strcmp(p, "/dev/null"));
  return 0;
}