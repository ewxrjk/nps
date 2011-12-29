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
#include "format.h"
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>

int main() {
  char output[64];

  assert(bytes(0, 0, 0, output, sizeof output, 1) == output);
  assert(!strcmp(output, "0"));

  assert(bytes(1024, 0, 0, output, sizeof output, 1) == output);
  assert(!strcmp(output, "1K"));

  assert(bytes(1024 * 1024, 0, 0, output, sizeof output, 1) == output);
  assert(!strcmp(output, "1M"));

  assert(bytes(1024 * 1024 * 1024, 0, 0, output, sizeof output, 1) == output);
  assert(!strcmp(output, "1G"));

  assert(bytes(1024LL * 1024 * 1024 * 1024, 0, 0, output, sizeof output, 1) == output);
  assert(!strcmp(output, "1T"));

  assert(bytes(1024LL * 1024 * 1024 * 1024 * 1024, 0, 0, output, sizeof output, 1) == output);
  assert(!strcmp(output, "1P"));

  assert(bytes(1024, 0, 'M', output, sizeof output, 1) == output);
  assert(!strcmp(output, "0"));

  assert(bytes(1024 * 1024, 0, 'M', output, sizeof output, 1) == output);
  assert(!strcmp(output, "1"));

  assert(bytes(1024 * 1024 * 1024, 0, 'M', output, sizeof output, 1) == output);
  assert(!strcmp(output, "1024"));

  assert(bytes(1024LL * 1024 * 1024 * 1024, 0, 'M', output, sizeof output, 1) == output);
  assert(!strcmp(output, "1048576"));

  assert(bytes(1024LL * 1024 * 1024 * 1024 * 1024, 0, 'M', output, sizeof output, 1) == output);
  assert(!strcmp(output, "1073741824"));

  assert(bytes(1024LL * 1024 * 1024 * 1024 * 1024, 20, 'M', output, sizeof output, 1) == output);
  assert(!strcmp(output, "          1073741824"));

  assert(bytes(16 * sysconf(_SC_PAGESIZE), 0, 'p', output, sizeof output, 1) == output);
  assert(!strcmp(output, "16"));

  return 0;
}
