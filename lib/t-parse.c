/*
 * This file is part of nps.
 * Copyright (C) 2011, 12, 13 Richard Kettlewell
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
#include "parse.h"
#include <stddef.h>
#include <string.h>
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

static int verbose;

static void try(const char *string,
                ptrdiff_t consumed,
                int xsign,
                const char *xname,
                size_t xsize,
                const char *xheading,
                const char *xarg,
                unsigned flags) {
  enum parse_status ps;
  const char *ptr = string;
  char *name, *heading, *arg;
  size_t size = SIZE_MAX;
  int sign = 99;

  if(verbose)
    fprintf(stderr, "%15s %3zu %2d %10s %20zu %10s %10s %04x\n",
            string, consumed, xsign, xname, xsize,
            xheading ? xheading : "<NULL>",
            xarg ? xarg : "<NULL>",
            flags);
  ps = parse_element(&ptr, &sign, &name, &size, &heading, &arg, flags);
  if(verbose)
    fprintf(stderr, "%15s %3td %2d %10s %20zu %10s %10s\n",
            "",
            ptr - string, sign, name, size,
            heading ? heading : "<NULL>",
            arg ? arg : "<NULL>");
  assert(ps == parse_ok);
  assert(ptr - string == consumed);
  assert(sign == xsign);
  assert(!strcmp(name, xname));
  assert(size == xsize);
  if(xheading == NULL)
    assert(heading == NULL);
  else {
    assert(heading != NULL);
    assert(!strcmp(heading, xheading));
  }
  if(xarg == NULL)
    assert(arg == NULL);
  else {
    assert(arg != NULL);
    assert(!strcmp(arg, xarg));
  }
}

static void try_eof(const char *string) {
  enum parse_status ps;
  const char *ptr = string;
  ps = parse_element(&ptr, NULL, NULL, NULL, NULL, NULL, 0);
  assert(ps == parse_eof);
  assert(ptr == string + strlen(string));
}

static void try_error(const char *string, unsigned flags) {
  enum parse_status ps;
  const char *ptr = string;
  char *name, *heading, *arg;
  size_t size = SIZE_MAX;
  int sign = 99;
  ps = parse_element(&ptr, &sign, &name, &size, &heading, &arg,
                     FORMAT_CHECK|flags);
  assert(ps == parse_error);
  assert(ptr == string);
}

int main(void) {
  verbose = !!getenv("VERBOSE");

  try("rss", 3, 99, "rss", SIZE_MAX, NULL, NULL, 0);
  try("rss,pss", 3, 99, "rss", SIZE_MAX, NULL, NULL, 0);
  try("rss pss", 3, 99, "rss", SIZE_MAX, NULL, NULL, 0);

  try("rss", 3, 0, "rss", SIZE_MAX, NULL, NULL, FORMAT_SIGN);
  try("+rss", 4, '+', "rss", SIZE_MAX, NULL, NULL, FORMAT_SIGN);
  try("-rss", 4, '-', "rss", SIZE_MAX, NULL, NULL, FORMAT_SIGN);
  try("-rss", 4, 99, "-rss", SIZE_MAX, NULL, NULL, 0);

  try("rss:23", 6, 99, "rss", 23, NULL, NULL, FORMAT_SIZE);

  try("rss=RSS", 7, 99, "rss", SIZE_MAX, "RSS", NULL, FORMAT_HEADING);
  try("rss=\"RSS\"", 9, 99, "rss", SIZE_MAX, "\"RSS\"", NULL, FORMAT_HEADING);
  try("rss=\"RSS\"", 9, 99, "rss", SIZE_MAX, "RSS", NULL, FORMAT_HEADING|FORMAT_QUOTED);
  try("rss='RSS'", 9, 99, "rss", SIZE_MAX, "RSS", NULL, FORMAT_HEADING|FORMAT_QUOTED);

  try("rss/K", 5, 99, "rss", SIZE_MAX, NULL, "K", FORMAT_ARG);
  try("rss/'K'", 7, 99, "rss", SIZE_MAX, NULL, "K", FORMAT_ARG);
  try("rss/\"K\"", 7, 99, "rss", SIZE_MAX, NULL, "K", FORMAT_ARG);

  try("rss=\"RSS\"/K", 11, 99, "rss", SIZE_MAX, "RSS", "K", FORMAT_HEADING|FORMAT_QUOTED|FORMAT_ARG);
  try("rss=\"RS\\\\\"/K", 12, 99, "rss", SIZE_MAX, "RS\\", "K", FORMAT_HEADING|FORMAT_QUOTED|FORMAT_ARG);
  try("rss:23=\"RSS\"/K", 14, 99, "rss", 23, "RSS", "K", FORMAT_SIZE|FORMAT_HEADING|FORMAT_QUOTED|FORMAT_ARG);

  try(" rss", 4, 99, "rss", SIZE_MAX, NULL, NULL, 0);
  try(",rss", 4, 99, "rss", SIZE_MAX, NULL, NULL, 0);
  try(", rss", 5, 99, "rss", SIZE_MAX, NULL, NULL, 0);
  try(" ,rss", 5, 99, "rss", SIZE_MAX, NULL, NULL, 0);

  try_eof("");
  try_eof(" ");
  try_eof(",");
  try_eof(",,");
  try_eof(" ,");
  try_eof(",");

  try_error("-", FORMAT_SIGN);
  try_error("rss:", FORMAT_SIZE);
  try_error("rss:z", FORMAT_SIZE);
  try_error("rss:18446744073709551616", FORMAT_SIZE);
  try_error("rss=\"x", FORMAT_QUOTED|FORMAT_HEADING);
  try_error("rss=\"\\", FORMAT_QUOTED|FORMAT_HEADING);


  return 0;
}
