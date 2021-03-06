/*
 * This file is part of nps.
 * Copyright (C) 2011, 2017 Richard Kettlewell
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
#include <stdarg.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "utils.h"
#include "io.h"

int xprintf(const char *format, ...) {
  va_list ap;
  int rc;

  va_start(ap, format);
  rc = vfprintf(stdout, format, ap);
  if(rc < 0)
    fatal(errno, "writing to stdout");
  va_end(ap);
  return rc;
}

int xasprintf(char **sp, const char *format, ...) {
  va_list ap;
  int rc;

  va_start(ap, format);
  rc = vasprintf(sp, format, ap);
  if(rc < 0)
    fatal(errno, "vasprintf");
  va_end(ap);
  return rc;
}

void xexit(int rc) {
  if(!rc)
    xfclose(stdout, "stdout");
  exit(rc);
}

FILE *xfopen(const char *path, const char *mode) {
  FILE *fp = fopen(path, mode);
  if(!fp)
    fatal(errno, "opening %s", path);
  return fp;
}

void xfclose(FILE *fp, const char *path) {
  if(fclose(fp) < 0)
    fatal(errno, "closing %s", path);
}

FILE *fopenf(char **pathp, const char *mode, const char *format, ...) {
  char *path;
  va_list ap;
  va_start(ap, format);
  if(vasprintf(&path, format, ap) < 0)
    fatal(errno, "vasprintf");
  va_end(ap);
  if(pathp)
    *pathp = path;
  return fopen(path, mode);
}

FILE *xfopenf(char **pathp, const char *mode, const char *format, ...) {
  char *path;
  FILE *fp;
  va_list ap;
  va_start(ap, format);
  if(vasprintf(&path, format, ap) < 0)
    fatal(errno, "vasprintf");
  va_end(ap);
  if(pathp)
    *pathp = path;
  if(!(fp = fopen(path, mode)))
    fatal(errno, "opening %s", path);
  return fp;
}

DIR *opendirf(char **pathp, const char *format, ...) {
  char *path;
  va_list ap;
  DIR *dp;

  va_start(ap, format);
  if(vasprintf(&path, format, ap) < 0)
    fatal(errno, "vasprintf");
  va_end(ap);
  if(pathp)
    *pathp = path;
  dp = opendir(path);
  if(!pathp)
    free(path);
  return dp;
}

struct dirent *xreaddir(const char *dir, DIR *dp) {
  struct dirent *de;
  errno = 0;
  de = readdir(dp);
  if(!de && errno)
    fatal(errno, "reading %s", dir);
  return de;
}

void xmkdir(const char *path, mode_t mode) {
  if(mkdir(path, mode) < 0)
    fatal(errno, "mkdir %s", path);
}
