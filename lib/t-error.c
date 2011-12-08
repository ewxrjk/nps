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
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <assert.h>

static int before(void) {
  assert(write(2, "X", 1) == 1);
  return 0;
}

static void launder_exit(int) attribute((noreturn));

static void launder_exit(int rc) {
  assert(write(2, "Y", 1) == 1);
  _exit(rc);
}

static void check(int errno_value) {
  int p[2], w;
  char buffer[512], expect[512];
  ssize_t n;
  size_t total;
  pid_t pid;

  if(pipe(p) < 0)
    fatal(errno, "pipe");
  switch(pid = fork()) {
  case 0:
    terminate = _exit;
    if(dup2(p[1], 2) < 0)
      fatal(errno, "dup2");
    onfatal = before;
    terminate = launder_exit;
    fatal(errno_value, "test");
    _exit(-1);
  case -1:
    fatal(errno, "fork");
  }
  if(close(p[1]) < 0)
    fatal(errno, "close");
  total = 0;
  do {
    n = read(p[0], buffer + total, sizeof buffer - total);
    if(n < 0)
      fatal(errno, "read");
    total += n;
    assert(total < sizeof buffer - 1);
  } while(n);
  buffer[total] = 0;
  snprintf(expect, sizeof expect, "XERROR: test%s%s\nY",
           errno_value == 0 ? "" : ": ",
           errno_value == 0 ? "" : strerror(errno_value));
  assert(!strcmp(buffer, expect));
  if(close(p[0]) < 0)
    fatal(errno, "close");
  if(waitpid(pid, &w, 0) < 0)
    fatal(errno, "waitpid");
  assert(WIFEXITED(w) && WEXITSTATUS(w) == 1);
}

int main(void) {
  check(0);
  check(ENOENT);
  return 0;
}
