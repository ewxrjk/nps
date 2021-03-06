/*
 * This file is part of nps.
 * Copyright (C) 2011, 2017 Richard Kettlewell
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
#include <time.h>
#include <assert.h>
#include <stdlib.h>

int main() {
  struct tm t;
  time_t zero = 0;
  struct buffer b[1];

  buffer_init(b);
  assert(b->base == NULL);
  assert(b->pos == 0);
  assert(b->size == 0);

  buffer_putc(b, 'a');
  assert(b->base != NULL);
  assert(!strncmp(b->base, "a", 1));
  assert(b->pos == 1);
  assert(b->pos < b->size);

  buffer_putc(b, 'b');
  assert(b->base != NULL);
  assert(!strncmp(b->base, "ab", 2));
  assert(b->pos == 2);
  assert(b->pos < b->size);

  buffer_append(b, "12345678901234567890");
  assert(b->base != NULL);
  assert(!strncmp(b->base, "ab12345678901234567890", 22));
  assert(b->pos == 22);
  assert(b->pos < b->size);

  buffer_append_n(b, "spong", 4);
  assert(b->base != NULL);
  assert(!strncmp(b->base, "ab12345678901234567890spon", 26));
  assert(b->pos == 26);
  assert(b->pos < b->size);

  buffer_printf(b, "%s", "");
  assert(b->base != NULL);
  assert(!strncmp(b->base, "ab12345678901234567890spon", 26));
  assert(b->pos == 26);
  assert(b->pos < b->size);

  buffer_printf(b, "%s", "123456");
  assert(b->base != NULL);
  assert(!strncmp(b->base, "ab12345678901234567890spon123456", 32));
  assert(b->pos == 32);
  assert(b->pos < b->size);
  b->pos -= 6;

  buffer_printf(b, "%s", "ABCDEFGHIJKLMNOPQRSTUVWXYZ");
  assert(b->base != NULL);
  assert(!strncmp(b->base, "ab12345678901234567890sponABCDEFGHIJKLMNOPQRSTUVWXYZ", 52));
  assert(b->pos == 52);
  assert(b->pos < b->size);

  buffer_terminate(b);
  assert(b->base != NULL);
  assert(!strcmp(b->base, "ab12345678901234567890sponABCDEFGHIJKLMNOPQRSTUVWXYZ"));
  assert(b->pos == 52);
  assert(b->pos < b->size);

  b->pos = 0;
  gmtime_r(&zero, &t);
  buffer_strftime(b, "", &t);
  assert(b->pos == 0);
  buffer_strftime(b, "%F %T", &t);
  buffer_terminate(b);
  assert(!strcmp(b->base, "1970-01-01 00:00:00"));

  free(b->base);
  return 0;
}
