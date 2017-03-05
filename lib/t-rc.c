/*
 * This file is part of nps.
 * Copyright (C) 2014, 17 Richard Kettlewell
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
#include <string.h>
#include <assert.h>
#include "rc.h"
#include "io.h"
#include "utils.h"

int main() {
  char *testdata, *testin, *testexp, *testout, *diffcmd;
  const char *srcdir;
  DIR *dp;
  struct dirent *de;

  if(!(srcdir = getenv("srcdir")))
    srcdir = ".";
  dp = opendirf(&testdata, "%s/%s", srcdir, "testdata-rc");
  assert(dp != NULL);
  while((de = xreaddir(testdata, dp))) {
    if(strchr(de->d_name, '.') || strchr(de->d_name, '~'))
      continue;
    xasprintf(&testin, "%s/%s", testdata, de->d_name);
    xasprintf(&testout, "%s/%s.out", testdata, de->d_name);
    xasprintf(&testexp, "%s/%s.exp", testdata, de->d_name);
    reset_rc();
    read_rc_path(testin);
    write_rc_path(testout);
    xasprintf(&diffcmd, "diff -u %s %s", testexp, testout);
    if(system(diffcmd))
      exit(1);
    free(testin);
    free(testout);
    free(testexp);
    free(diffcmd);
  }
  free(testdata);
  closedir(dp);
  return 0;
}
