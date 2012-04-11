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
#include "format.h"
#include "compare.h"
#include "buffer.h"
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

int select_has_terminal(struct procinfo *pi,
                        taskident task,
                        union arg attribute((unused)) *args,
                        size_t attribute((unused)) nargs) {
  return proc_get_tty(pi, task) > 0;
}

int select_all(struct procinfo attribute((unused)) *pi,
               taskident attribute((unused)) task,
               union arg attribute((unused)) *args,
               size_t attribute((unused)) nargs) {
  return 1;
}

int select_not_session_leader(struct procinfo *pi,
                              taskident task,
                              union arg attribute((unused)) *args,
                              size_t attribute((unused)) nargs) {
  return proc_get_session(pi, task) != task.pid;
}

int select_pid(struct procinfo attribute((unused)) *pi, taskident task,
               union arg *args, size_t nargs) {
  size_t n;

  for(n = 0; n < nargs; ++n) {
    if(task.pid == args[n].pid)
      return 1;
  }
  return 0;
}

int select_ppid(struct procinfo *pi, taskident task,
               union arg *args, size_t nargs) {
  size_t n;
  pid_t ppid = proc_get_ppid(pi, task);

  for(n = 0; n < nargs; ++n) {
    if(ppid == args[n].pid)
      return 1;
  }
  return 0;
}

int select_apid(struct procinfo *pi, taskident task,
               union arg *args, size_t nargs) {
  size_t n;

  for(n = 0; n < nargs; ++n) {
    taskident t = { args[n].pid, -1 };
    if(proc_is_ancestor(pi, t, task))
      return 1;
  }
  return 0;
}

int select_terminal(struct procinfo *pi, taskident task,
                    union arg *args, size_t nargs) {
  size_t n;
  int tty_nr = proc_get_tty(pi, task);
  for(n = 0; n < nargs; ++n) {
    if(tty_nr == args[n].tty)
      return 1;
  }
  return 0;
}

int select_leader(struct procinfo *pi, taskident task,
                  union arg *args, size_t nargs) {
  taskident leader = { proc_get_session(pi, task), -1 };
  if(leader.pid == -1)
    return 0;
  return select_pid(pi, leader, args, nargs);
}

int select_rgid(struct procinfo *pi, taskident task,
                union arg *args, size_t nargs) {
  size_t n;
  gid_t gid = proc_get_egid(pi, task);
  for(n = 0; n < nargs; ++n) {
    if(gid == args[n].gid)
      return 1;
  }
  return 0;
}

int select_egid(struct procinfo *pi, taskident task,
                union arg *args, size_t nargs) {
  size_t n;
  gid_t gid = proc_get_egid(pi, task);
  for(n = 0; n < nargs; ++n) {
    if(gid == args[n].gid)
      return 1;
  }
  return 0;
}

int select_euid(struct procinfo *pi, taskident task,
                union arg *args, size_t nargs) {
  size_t n;
  uid_t uid = proc_get_euid(pi, task);
  for(n = 0; n < nargs; ++n) {
    if(uid == args[n].uid)
      return 1;
  }
  return 0;
}

int select_ruid(struct procinfo *pi, taskident task, union arg *args, size_t nargs) {
  size_t n;
  uid_t uid = proc_get_ruid(pi, task);
  for(n = 0; n < nargs; ++n) {
    if(uid == args[n].uid)
      return 1;
  }
  return 0;
}

int select_uid_tty(struct procinfo *pi, taskident task,
                   union arg attribute((unused)) *args, 
                   size_t attribute((unused)) nargs) {
  /* "By default, ps shall select all processes with the same
   * effective user ID as the current user and the same controlling
   * terminal as the invoker."
   *
   * See also priv.h.  If we were installed setuid then the effective
   * user in this case will be the real UID of the caller.  The
   * effective UID of the caller will have been lost.
   */
  return proc_get_euid(pi, task) == geteuid()
    && proc_get_tty(pi, task) == self_tty(pi);
}

int select_nonidle(struct procinfo *pi, taskident task,
                   union arg attribute((unused)) *args, 
                   size_t attribute((unused)) nargs) {
  return proc_get_state(pi, task) != 'Z'
    && proc_get_pcpu(pi, task) > 0;
}

int select_string_match(struct procinfo *pi, taskident task, union arg *args,
                        size_t nargs) {
  struct buffer b[1];
  int rc;
  assert(nargs == 2);
  buffer_init(b);
  format_value(pi, task, args[0].string, b, 0);
  rc = !strcmp(b->base, args[1].string);
  free(b->base);
  return rc;
}

int select_compare(struct procinfo *pi, taskident task, union arg *args,
                   size_t nargs) {
  struct buffer b[1];
  int c;
  assert(nargs == 3);
  buffer_init(b);
  format_value(pi, task, args[0].string, b, FORMAT_RAW);
  c = qlcompare(b->base, args[2].string);
  free(b->base);
  switch(args[1].operator) {
  case '<': return c < 0;
  case '=': return c == 0;
  case '>': return c > 0;
  case LE: return c <= 0;
  case GE: return c >= 0;
  case NE: return c != 0;
  default:
    fatal(0, "unrecognized comparison operator %#x", args[1].operator);
  }
}

int select_regex_match(struct procinfo *pi, taskident task, union arg *args,
                       size_t nargs) {
  char buffer[1024];
  struct buffer b[1];
  int rc;
  assert(nargs == 2);
  buffer_init(b);
  format_value(pi, task, args[0].string, b, 0);
  rc = regexec(&args[1].regex, b->base, 0, 0, 0);
  free(b->base);
  switch(rc) {
  case 0:
    return 1;
  case REG_NOMATCH:
    return 0;
  default:
    regerror(rc, &args[1].regex, buffer, sizeof buffer);
    fatal(0, "regexec: %s", buffer);
  }
}
