/*
 * This file is part of nps.
 * Copyright (C) 2012, 17 Richard Kettlewell
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
#include "buffer.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#define ELAPSED(FORMAT,SECONDS,RESULT) do {     \
  b->pos = 0;                                   \
  strfelapsed(b, FORMAT, SECONDS);              \
  buffer_terminate(b);                          \
  assert(!strcmp(b->base, RESULT));             \
} while(0)

int main() {
  struct buffer b[1];

  buffer_init(b);

  ELAPSED("%%", 0, "%");
  ELAPSED("", 0, "");
  ELAPSED("%d", 0, "0");
  ELAPSED("%d", 86399, "0");
  ELAPSED("%d", 86400, "1");
  ELAPSED("%d", 172799, "1");
  ELAPSED("%d", 172800, "2");

  ELAPSED("%h", 0, "0");
  ELAPSED("%h", 3599, "0");
  ELAPSED("%h", 3600, "1");
  ELAPSED("%h", 7199, "1");
  ELAPSED("%h", 7200, "2");
  ELAPSED("%h", 86400, "24");
  ELAPSED("%h", 172800, "48");

  ELAPSED("%H", 0, "0");
  ELAPSED("%H", 3599, "0");
  ELAPSED("%H", 3600, "1");
  ELAPSED("%H", 7199, "1");
  ELAPSED("%H", 7200, "2");
  ELAPSED("%H", 86400, "0");
  ELAPSED("%H", 93600, "2");
  ELAPSED("%H", 172800, "0");

  ELAPSED("%m", 0, "0");
  ELAPSED("%m", 59, "0");
  ELAPSED("%m", 60, "1");
  ELAPSED("%m", 61, "1");
  ELAPSED("%m", 86400, "1440");
  ELAPSED("%M", 0, "0");
  ELAPSED("%M", 59, "0");
  ELAPSED("%M", 60, "1");
  ELAPSED("%M", 61, "1");
  ELAPSED("%M", 3660, "1");
  ELAPSED("%M", 3720, "2");
  ELAPSED("%M", 86400, "0");

  ELAPSED("%s", 0, "0");
  ELAPSED("%s", 59, "59");
  ELAPSED("%s", 86400, "86400");
  ELAPSED("%S", 0, "0");
  ELAPSED("%S", 59, "59");
  ELAPSED("%S", 86400, "0");
  ELAPSED("%S", 61, "1");
  ELAPSED("%S", 119, "59");

  ELAPSED("%3s", 0, "  0");
  ELAPSED("%3s", -9, " -9");
  ELAPSED("%3s", 100, "100");
  ELAPSED("%3s", 1000, "1000");
  ELAPSED("%03s", 0, "000");
  ELAPSED("%03s", 9, "009");
  ELAPSED("%03s", -9, "-09");
  ELAPSED("%03s", 100, "100");
  ELAPSED("%03s", 1000, "1000");

  ELAPSED("%.5s", 0, "00000");
  ELAPSED("%.5s", 9, "00009");
  ELAPSED("%.5s", -9, "-00009");
  ELAPSED("%.3s", 1000, "1000");

  ELAPSED("%d:%h:%m:%s", 0, "0:0:0:0");
  ELAPSED("%?d:%?h:%?m:%s", 0, ":::0");
  ELAPSED("%+:d%+:h%+:m%s", 0, "0:0:0:0");
  ELAPSED("%?+:d%?+:h%?+:m%s", 0, "0");

  ELAPSED("%3+:s", 0, "  0:");

  free(b->base);
  return 0;
}
