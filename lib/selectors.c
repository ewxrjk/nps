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
#include "selectors.h"
#include "process.h"
#include "utils.h"
#include <unistd.h>
#include <stdio.h>

int select_has_terminal(pid_t pid,
                        union arg attribute((unused)) *args,
                        size_t attribute((unused)) nargs) {
  return proc_get_tty(pid) > 0;
}

int select_all(pid_t attribute((unused)) pid,
               union arg attribute((unused)) *args,
               size_t attribute((unused)) nargs) {
  return 1;
}

int select_not_session_leader(pid_t pid,
                              union arg attribute((unused)) *args,
                              size_t attribute((unused)) nargs) {
  return proc_get_session(pid) != pid;
}

int select_pid(pid_t pid, union arg *args, size_t nargs) {
  size_t n;

  for(n = 0; n < nargs; ++n) {
    if(pid == args[n].pid)
      return 1;
  }
  return 0;
}

int select_terminal(pid_t pid, union arg *args, size_t nargs) {
  size_t n;
  int tty_nr = proc_get_tty(pid);
  for(n = 0; n < nargs; ++n) {
    if(tty_nr == args[n].tty)
      return 1;
  }
  return 0;
}

int select_leader(pid_t pid, union arg *args, size_t nargs) {
  pid_t leader = proc_get_session(pid);
  if(leader == -1)
    return 0;
  return select_pid(leader, args, nargs);
}

int select_rgid(pid_t pid, union arg *args, size_t nargs) {
  size_t n;
  gid_t gid = proc_get_egid(pid);
  for(n = 0; n < nargs; ++n) {
    if(gid == args[n].gid)
      return 1;
  }
  return 0;
}

int select_egid(pid_t pid, union arg *args, size_t nargs) {
  size_t n;
  gid_t gid = proc_get_egid(pid);
  for(n = 0; n < nargs; ++n) {
    if(gid == args[n].gid)
      return 1;
  }
  return 0;
}

int select_euid(pid_t pid, union arg *args, size_t nargs) {
  size_t n;
  uid_t uid = proc_get_euid(pid);
  for(n = 0; n < nargs; ++n) {
    if(uid == args[n].uid)
      return 1;
  }
  return 0;
}

int select_ruid(pid_t pid, union arg *args, size_t nargs) {
  size_t n;
  uid_t uid = proc_get_ruid(pid);
  for(n = 0; n < nargs; ++n) {
    if(uid == args[n].uid)
      return 1;
  }
  return 0;
}

int select_uid_tty(pid_t pid, union arg attribute((unused)) *args, 
                   size_t attribute((unused)) nargs) {
  /* "By default, ps shall select all processes with the same
   * effective user ID as the current user and the same controlling
   * terminal as the invoker." */
  return proc_get_euid(pid) == geteuid()
          && proc_get_tty(pid) == proc_get_tty(getpid());
}
