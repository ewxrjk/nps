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
#include "priv.h"
#include "utils.h"
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <assert.h>

uid_t priv_euid, priv_ruid;

static void valid_fd(int fd) {
  struct stat sb;
  if(fstat(fd, &sb) < 0) {
    if(fd == 2)
      _exit(-1);                /* fatal() won't work... */
    else
      fatal(errno, "fstat %d", fd);
  }
}

void priv_init(int argc, char attribute((unused)) **argv) {
  /* Sanity checking */
  /* 1. If argv is empty we conclude that something is up and refuse to proceed. */
  if(!argc)
    fatal(0, "argc=0");
  /* 2. If any of the three stdio streams are close we give up. */
  valid_fd(2);
  valid_fd(1);
  valid_fd(0);
  /* Record what identities we run under */
  priv_euid = geteuid();
  priv_ruid = getuid();
  /* Drop privilege */
  if(priv_euid != priv_ruid) {
    if(seteuid(priv_ruid) < 0)
      fatal(errno, "seteuid");
  }
  assert(geteuid() == getuid());
}

int priv_run(int (*op)(void *u), void *u) {
  int rc;
  /* Elevate privilege */
  if(priv_euid != priv_ruid) {
    if(seteuid(priv_euid) < 0)
      fatal(errno, "seteuid");
  }
  rc = op(u);
  if(priv_euid != priv_ruid) {
    if(seteuid(priv_ruid) < 0)
      fatal(errno, "seteuid");
  }
  return rc;
}

int privileged(void) {
  return priv_euid != priv_ruid;
}
