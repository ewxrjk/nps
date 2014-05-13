/*
 * This file is part of nps.
 * Copyright (C) 2011, 2014 Richard Kettlewell
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
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>

void *xmalloc(size_t n) {
  void *ptr;

  if(!n)
    return NULL;
  if(!(ptr = malloc(n)))
    fatal(errno, "malloc");
  return ptr;
}

void *xrealloc(void *ptr, size_t n) {
  if(!n) {
    free(ptr);
    return NULL;
  }
  if(!(ptr = realloc(ptr, n)))
    fatal(errno, "realloc");
  return ptr;
}

void *xrecalloc(void *ptr, size_t n, size_t s) {
  if(n && s > SIZE_MAX / n)
    fatal(0, "out of memory");
  return xrealloc(ptr, n * s);
}

char *xstrdup(const char *s) {
  return xstrndup(s, strlen(s));
}

char *xstrndup(const char *s, size_t n) {
  char *ptr;
  if((size_t)(n + 1) == 0)
    fatal(0, "out of memory");
  ptr = xmalloc(n + 1);
  memcpy(ptr, s, n);
  ptr[n] = 0;
  return ptr;
}

void free_strings(char **strings) {
  char **s;

  for(s = strings; *s; ++s)
    free(*s);
  free(strings);
}
