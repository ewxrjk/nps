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
#include <ctype.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <inttypes.h>
#include <unistd.h>
#include "format.h"
#include "compare.h"

struct procinfo *global_procinfo;

int compare_pid(const void *av, const void *bv) {
  pid_t a = *(const pid_t *)av;
  pid_t b = *(const pid_t *)bv;
  return format_compare(global_procinfo, a, b);
}

#define GET_VALUE(W) do {                                               \
  errno = 0;                                                            \
  W##d = strtoumax(W, &e##W, 0);                                        \
  if(errno || W == e##W)                                                \
    goto string_compare;                                                \
  if(*e##W == 'K') { W##d *= 1024; ++e##W; }                            \
  if(*e##W == 'M') { W##d *= 1024 * 1024; ++e##W; }                     \
  if(*e##W == 'G') { W##d *= 1024 * 1024 * 1024; ++e##W; }              \
  if(*e##W == 'T') { W##d *= (uintmax_t)1024 * 1024 * 1024 * 1024; ++e##W; } \
  if(*e##W == 'P') { W##d *= (uintmax_t)1024 * 1024 * 1024 * 1024 * 1024; ++e##W; } \
  if(*e##W == 'p') { W##d *= sysconf(_SC_PAGESIZE); ++e##W; }           \
} while(0)


int qlcompare(const char *a, const char *b) {
  uintmax_t ad, bd;
  char *ea, *eb;
  int ca, cb;
  while(*a && *b) {
    if(isdigit((unsigned char)*a) && isdigit((unsigned char)*b)) {
      GET_VALUE(a);
      GET_VALUE(b);
      if(ad < bd)
        return -1;
      else if(ad > bd)
        return 1;
      a = ea;
      b = eb;
    } else {
    string_compare:
      ca = (unsigned char)*a++;
      cb = (unsigned char)*b++;
      if(ca < cb)
        return -1;
      else if(ca > cb)
        return 1;
    }
  }
  if(*a)
    return 1;
  else if(*b)
    return -1;
  else
    return 0;
}
