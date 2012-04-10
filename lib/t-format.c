/*
 * This file is part of nps.
 * Copyright (C) 2012 Richard Kettlewell
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
#include "buffer.h"
#include <assert.h>
#include <stdio.h>

/* TODO this a bit rudimentary - there is lots of complicated stuff in
 * format.c that could use some tests. */

#define INTEGER(VALUE,BASE,RESULT) do {         \
  b->pos = 0;                                   \
  format_integer(VALUE, b, BASE);               \
  buffer_terminate(b);                          \
  assert(!strcmp(b->base, RESULT));             \
} while(0)

#define ADDRESS(VALUE,RESULT) do {              \
  b->pos = 0;                                   \
  format_addr(VALUE, b);                        \
  buffer_terminate(b);                          \
  assert(!strcmp(b->base, RESULT));             \
} while(0)

#define INTERVAL(VALUE,AH,CS,FORMAT,FLAGS,RESULT) do {  \
  b->pos = 0;                                           \
  format_interval(VALUE, b, AH, CS, FORMAT, FLAGS);     \
  buffer_terminate(b);                                  \
  fprintf(stderr, "got: [%s]\n", b->base); \
  assert(!strcmp(b->base, RESULT));                     \
} while(0)

int main() {
  struct buffer b[1];

  buffer_init(b);
  format_syntax(syntax_normal);

  INTEGER(100, 'd', "100");
  INTEGER(-100, 'd', "-100");
  INTEGER(100, 'u', "100");
  INTEGER(127, 'x', "7f");
  INTEGER(127, 'X', "7F");
  INTEGER(127, 'o', "177");

  ADDRESS(0x00000000000000FF, "000000ff");
  ADDRESS(0x000000FF000000FF, "00ff000000ff");
  ADDRESS(0x00FF0000000000FF, "00ff0000000000ff");

  INTERVAL(1000, 0, SIZE_MAX, "%h:%M:%S", 0, "0:16:40");
  INTERVAL(1000, 1, SIZE_MAX, "%h:%M:%S", 0, "0:16:40");
  INTERVAL(1000, 0, 0, "%h:%M:%S", 0, "0:16:40");

  INTERVAL(0, 0, SIZE_MAX, NULL, 0, "00:00");
  INTERVAL(1, 0, SIZE_MAX, NULL, 0, "00:01");
  INTERVAL(60, 0, SIZE_MAX, NULL, 0, "01:00");
  INTERVAL(3600, 0, SIZE_MAX, NULL, 0, "01:00:00");
  INTERVAL(0, 1, SIZE_MAX, NULL, 0, "00:00:00");
  INTERVAL(1, 1, SIZE_MAX, NULL, 0, "00:00:01");
  INTERVAL(60, 1, SIZE_MAX, NULL, 0, "00:01:00");
  INTERVAL(3600, 1, SIZE_MAX, NULL, 0, "01:00:00");

  INTERVAL(86400, 0, SIZE_MAX, NULL, 0, "1-00:00:00");
  INTERVAL(172800, 0, SIZE_MAX, NULL, 0, "2-00:00:00");
  INTERVAL(86400, 1, SIZE_MAX, NULL, 0, "1-00:00:00");
  INTERVAL(172800, 1, SIZE_MAX, NULL, 0, "2-00:00:00");

  INTERVAL(0, 0, 0, NULL, 0, "00m00");
  INTERVAL(1, 0, 0, NULL, 0, "00m01");
  INTERVAL(60, 0, 0, NULL, 0, "01m00");
  INTERVAL(3600, 0, 0, NULL, 0, "01h00");
  INTERVAL(0, 1, 0, NULL, 0, "00m00");
  INTERVAL(1, 1, 0, NULL, 0, "00m01");
  INTERVAL(60, 1, 0, NULL, 0, "01m00");
  INTERVAL(3600, 1, 0, NULL, 0, "01h00");

  INTERVAL(86400, 0, 0, NULL, 0, "1d00");
  INTERVAL(172800, 0, 0, NULL, 0, "2d00");
  INTERVAL(86400, 1, 0, NULL, 0, "1d00");
  INTERVAL(172800, 1, 0, NULL, 0, "2d00");

  INTERVAL(1000, 0, SIZE_MAX, "%h:%M:%S", FORMAT_RAW, "1000");
  INTERVAL(0, 0, SIZE_MAX, NULL, FORMAT_RAW, "0");
  INTERVAL(86400, 0, SIZE_MAX, NULL, FORMAT_RAW, "86400");
  INTERVAL(172800, 1, 0, NULL, FORMAT_RAW, "172800");

  format_syntax(syntax_csv);

  INTEGER(100, 'd', "100");
  INTEGER(-100, 'd', "-100");
  INTEGER(100, 'u', "100");
  INTEGER(127, 'x', "127");
  INTEGER(127, 'X', "127");
  INTEGER(127, 'o', "127");

  ADDRESS(0x00000000000000FF, "255");
  ADDRESS(0x000000FF000000FF, "1095216660735");
  ADDRESS(0x00FF0000000000FF, "71776119061217535");

  INTERVAL(1000, 0, SIZE_MAX, "%h:%M:%S", 0, "1000");
  INTERVAL(0, 0, SIZE_MAX, NULL, 0, "0");
  INTERVAL(86400, 0, SIZE_MAX, NULL, 0, "86400");
  INTERVAL(172800, 1, 0, NULL, 0, "172800");

  return 0;
}
