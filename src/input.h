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
#ifndef INPUT_H
#define INPUT_H

#include <stddef.h>

struct input_context {
  char *buffer;
  size_t bufsize;
  size_t len;
  size_t cursor;
  size_t offset;
  const char *prompt;
  int (*validate)(const char *buffer);
};

void input_key(int ch, struct input_context *ctx);
void input_draw(struct input_context *ctx);

#define ESCBIT (KEY_MAX + 1)

#endif /* INPUT_H */

