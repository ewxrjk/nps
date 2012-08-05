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
#include <stdio.h>
#if HAVE_SYS_CAPABILITY_H
#include <sys/capability.h>
#endif

/* nps supports being installed in a variety of ways. */

/* Support being installed setuid to root. */

static int detect_setuid(void) {
  if(priv_euid == priv_ruid)
    return 0;
#if ! SUPPORT_SETUID
  /* If setuid support was disabled in configure, reject attempts to
   * run that way. */
  fatal(0, "inappropriately installed setuid");
#endif
  /* Drop privilege */
  if(seteuid(priv_ruid) < 0)
    fatal(errno, "seteuid");
  assert(geteuid() == getuid());
  return 1;
}

static void ascend_setuid(void) {
  if(seteuid(priv_euid) < 0)
    fatal(errno, "seteuid");
}

static void descend_setuid(void) {
  if(seteuid(priv_ruid) < 0)
    fatal(errno, "seteuid");
}

/* Support being installed setcap to CAP_SYS_PTRACE. */

#if HAVE_SYS_CAPABILITY_H
static cap_t cap;

static void set_capability(cap_flag_t flag, cap_flag_value_t capvalue) {
  const cap_value_t cap_sys_ptrace = CAP_SYS_PTRACE;
  if(cap_set_flag(cap, flag, 1, &cap_sys_ptrace, capvalue) < 0)
    fatal(errno, "cap_set_flag");
  if(cap_set_proc(cap) < 0)
    fatal(errno, "cap_set_proc");
}

static int detect_capability(void) {
  cap_flag_value_t capvalue;
  if(!(cap = cap_get_proc()))
    fatal(errno, "cap_get_proc");
  if(cap_get_flag(cap, CAP_SYS_PTRACE, CAP_PERMITTED, &capvalue) < 0)
    fatal(errno, "cap_get_flag");
  if(capvalue == CAP_CLEAR)
    return 0;
  /* Reduce our permitted set to just CAP_SYS_PTRACE and the
   * effective set to nothing */
  if(cap_clear(cap) < 0)
    fatal(errno, "cap_clear");
  set_capability(CAP_PERMITTED, CAP_SET);
  return 1;
}

static void ascend_capability(void) {
  set_capability(CAP_EFFECTIVE, CAP_SET);
}

static void descend_capability(void) {
  set_capability(CAP_EFFECTIVE, CAP_CLEAR);
}
#endif

/* Support being invoked as root */

static int detect_root(void) {
  return priv_ruid == 0;
}

/* Support being invoked completely unprivileged */

static int detect_unprivileged(void) {
  return 1;
}

static void ascend_none() {
}

typedef struct {
  int (*detect)(void);
  void (*ascend)(void);
  void (*descend)(void);
} privmode_t;
  
static const privmode_t privmodes[] = {
  { detect_setuid, ascend_setuid, descend_setuid },
#if HAVE_SYS_CAPABILITY_H
  { detect_capability, ascend_capability, descend_capability },
#endif
  { detect_root, ascend_none, ascend_none },
  { detect_unprivileged, ascend_none, ascend_none },
}, *privmode;

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
  size_t i;
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
  /* Discover how privileged (if at all) we are */
  for(i = 0; i < sizeof privmodes / sizeof *privmodes; ++i) {
    if(privmodes[i].detect()) {
      privmode = &privmodes[i];
      return;
    }
  }
  fatal(0, "no privmode found");
}

int priv_run(int (*op)(void *u), void *u) {
  int rc;
  /* Elevate privilege */
  privmode->ascend();
  rc = op(u);
  privmode->descend();
  return rc;
}

int privileged(void) {
  return privmode->ascend != ascend_none;
}
