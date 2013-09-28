/*
 * This file is part of nps.
 * Copyright (C) 2013 Richard Kettlewell
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
#include <assert.h>
#include <unistd.h>

static int runfn(void *u) {
  int *counterp = u;
  ++*counterp;
  return 99;
}

int main() {
  int counter = 0;
  priv_init(1, NULL);
  assert(priv_euid == getuid());
  assert(priv_ruid == getuid());
  assert(!privileged());
  assert(priv_run(runfn, &counter) == 99);
  assert(counter == 1);
  return 0;
}
