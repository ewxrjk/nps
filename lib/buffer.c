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
#include "buffer.h"
#include "utils.h"
#include <stdarg.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>

static void buffer_need(struct buffer *b, size_t n) {
  size_t newsize;
  if(n > b->size) {
    newsize = b->size ? b->size : 16;
    while(newsize && n > newsize)
      newsize *= 2;
    if(!newsize)
      fatal(0, "buffer size limit exceeded");
    b->base = xrealloc(b->base, newsize);
    b->size = newsize;
  }
}

void buffer_putc_outline(struct buffer *b, int c) {
  buffer_need(b, b->pos + 1);
  b->base[b->pos++] = c;
}

void buffer_append_n(struct buffer *b, const char *s, size_t n) {
  buffer_need(b, b->pos + n);
  memcpy(b->base + b->pos, s, n);
  b->pos += n;
}

int buffer_printf(struct buffer *b, const char *fmt, ...) {
  va_list ap;
  int n;

  va_start(ap, fmt);
  n = vsnprintf(b->base + b->pos, b->size - b->pos, fmt, ap);
  va_end(ap);
  if(n < 0)
    fatal(errno, "vsnprintf");
  if(n > 0) {
    /* >= because an exact fit will actually have lost the trailing 0 */
    if((size_t)n >= b->size - b->pos) {
      buffer_need(b, b->pos + n + 1);
      va_start(ap, fmt);
      n = vsnprintf(b->base + b->pos, b->size - b->pos, fmt, ap);
      va_end(ap);
      if(n < 0)
        fatal(errno, "vsnprintf");
    }
  }
  b->pos += n;
  return n;
}

void buffer_strftime(struct buffer *b, const char *format, const struct tm *tm) {
  size_t n;
  n = strftime(NULL, 0, format, tm);
  buffer_need(b, n + 1);
  b->pos += strftime(b->base + b->pos, b->size - b->pos, format, tm);
}

void buffer_terminate(struct buffer *b) {
  buffer_need(b, b->pos + 1);
  b->base[b->pos] = 0;
}
