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
#include "io.h"
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdlib.h>

struct device {
  char *path;
  int type;
  dev_t device;
};

static size_t ndevices, nslots;
static struct device *devices;

const char *forcedev;

static void device_register(char *path, int type, dev_t device) {
  if(ndevices >= nslots) {
    if((ssize_t)(nslots = nslots ? 2 * nslots : 64) < 0)
      fatal(0, "too many devices");
    devices = xrecalloc(devices, nslots, sizeof *devices);
  }
  devices[ndevices].path = path;
  devices[ndevices].type = type;
  devices[ndevices].device = device;
  ++ndevices;
}

static int device_compare(const void *av, const void *bv) {
  const struct device *a = av, *b = bv;
  if(a->type < b->type)
    return -1;
  else if(a->type > b->type)
    return 1;
  else if(a->device < b->device)
    return -1;
  else if(a->device > b->device)
    return 1;
  else
    return 0;
}

static void device_clear(void) {
  size_t n = 0;
  for(n = 0; n < ndevices; ++n)
    free(devices[n].path);
  ndevices = 0;
}

static void device_map(const char *dir) {
  char *path;
  struct dirent *de;
  DIR *dp;
  struct stat sb;

  if(!(dp = opendir(dir))) {
    /* Just skip bits of /dev we can't see into, for any reason; we'll
     * probably still be able to see the bits we care about and if
     * something's *really* wrong someone will find out soon enough. */
    return;
  }
  errno = 0;
  while((de = readdir(dp))) {
    if(de->d_name[0] != '.') {
      xasprintf(&path, "%s/%s", dir, de->d_name);
      if(lstat(path, &sb) >= 0) {
        if(S_ISDIR(sb.st_mode))
          device_map(path);
        else if(S_ISCHR(sb.st_mode) || S_ISBLK(sb.st_mode)) {
          device_register(path, S_ISBLK(sb.st_mode), sb.st_rdev);
          path = NULL;
        }
      }
      free(path);
    }
    errno = 0;
  }
  if(errno)
    fatal(errno, "readdir %s", dir);
  closedir(dp);
}

static void device_force(void) {
  char buffer[256], *p;
  FILE *fp = xfopen(forcedev, "r");
  int type;
  dev_t dev;
  while(fgets(buffer, sizeof buffer, fp)) {
    if((p = strchr(buffer, '\n')))
      *p = 0;
    p = buffer;
    type = strtol(p, &p, 0);
    if(*p)
      ++p;
    dev = strtoul(p, &p, 0);
    if(*p)
      ++p;
    device_register(xstrdup(p), type, dev);
  }
  fclose(fp);
}

const char *device_path(int type, dev_t device) {
  ssize_t l, r, m;
  int i;
  for(i = 0; i < 2; ++i) {
    l = 0;
    r = ndevices - 1;
    while(l <= r) {
      m = l + (r - l) / 2;
      if(type  < devices[m].type)
        r = m - 1;
      else if(type > devices[m].type)
        l = m + 1;
      else if(device < devices[m].device)
        r = m - 1;
      else if(device > devices[m].device)
        l = m + 1;
      else
        return devices[m].path;
    }
    device_clear();
    if(!forcedev)
      device_map("/dev");
    else
      device_force();
    qsort(devices, ndevices, sizeof *devices, device_compare);
  }
  return NULL;
}

static dev_t device_id(const char *path) {
  size_t n;
  if(!forcedev) {
    struct stat sb;
    if(stat(path, &sb) < 0)
      return -1;
    return sb.st_rdev;
  } else {
    if(ndevices == 0)
      device_force();
    for(n = 0; n < ndevices; ++n)
      if(!strcmp(path, devices[n].path))
        return devices[n].device;
    return -1;
  }
}

dev_t tty_id(const char *name) {
  char buffer[1024];

  if(name[0] != '/') {
    if(name[0] >= '0' && name[0] <= '9')
      snprintf(buffer, sizeof buffer, "/dev/tty%s", name);
    else
      snprintf(buffer, sizeof buffer, "/dev/%s", name);
    return device_id(buffer);
  } else
    return device_id(name);
}
