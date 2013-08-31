/*
 * This file is part of nps.
 * Copyright (C) 2011, 2013 Richard Kettlewell
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
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <assert.h>
#include <setjmp.h>
#include <stdlib.h>
#include "utils.h"

static int exited, exit_status;

static int before(void) {
  assert(write(2, "X", 1) == 1);
  return 0;
}

static jmp_buf env;

static void launder_exit(int) attribute((noreturn));

static void launder_exit(int rc) {
  exit_status = rc;
  exited = 1;
  longjmp(env, 1);              /* because noreturn */
  assert(!"wtf");
}

static void check(int set_onfatal, int errno_value) {
  int p[2];
  char buffer[512], expect[512];
  ssize_t n;
  size_t total;
  int save_stderr;

  save_stderr = dup(2); assert(save_stderr >= 0);
  assert(pipe(p) >= 0);
  assert(dup2(p[1], 2) >= 0);
  assert(close(p[1]) >= 0);
  terminate = launder_exit;
  onfatal = set_onfatal ? before : NULL;
  terminate = launder_exit;
  if(setjmp(env) == 0) {
    fatal(errno_value, "test");
    assert(!"reached");
  }
  assert(dup2(save_stderr, 2) >= 0);
  assert(close(save_stderr) >= 0);
  total = 0;
  do {
    n = read(p[0], buffer + total, sizeof buffer - total);
    assert(n >= 0);
    total += n;
    assert(total < sizeof buffer - 1);
  } while(n);
  buffer[total] = 0;
  snprintf(expect, sizeof expect, "%sERROR: test%s%s\n",
           set_onfatal ? "X" : "",
           errno_value == 0 ? "" : ": ",
           errno_value == 0 ? "" : strerror(errno_value));
  if(strcmp(buffer, expect)) {
    fprintf(stderr, "expected: %s\n", expect);
    fprintf(stderr, "     got: %s\n", buffer);
    exit(1);
  }
  assert(!strcmp(buffer, expect));
  assert(close(p[0]) >= 0);
}

int main(void) {
  check(0, 0);
  check(0, ENOENT);
  check(1, EFAULT);
  return 0;
}
