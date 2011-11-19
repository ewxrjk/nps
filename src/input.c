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
#include "input.h"
#include "utils.h"
#include <curses.h>
#include <string.h>

void input_key(int ch, struct input_context *ctx) {
  switch(ch) {
  case 2:                       /* ^B */
  case KEY_LEFT:
    if(ctx->cursor)
      --ctx->cursor;
    break;
  case 6:                       /* ^F */
  case KEY_RIGHT:
    if(ctx->cursor < ctx->len)
      ++ctx->cursor;
    break;
  case 8:                       /* ^H */
  case 0x7F:
    if(ctx->cursor) {
      --ctx->cursor;
      memmove(ctx->buffer + ctx->cursor,
              ctx->buffer + ctx->cursor + 1,
              ctx->len - ctx->cursor - 1);
      --ctx->len;
    }
    break;
  case 4:                       /* ^D */
  case KEY_DC:
    if(ctx->cursor < ctx->len) {
      memmove(ctx->buffer + ctx->cursor,
              ctx->buffer + ctx->cursor + 1,
              ctx->len - ctx->cursor - 1);
      --ctx->len;
    }
    break;
  case 1:                       /* ^A */
    ctx->cursor = 0;
    break;
  case 5:                       /* ^E */
    ctx->cursor = ctx->len;
    break;
  case 12:                      /* ^L */
    if(redrawwin(stdscr) == ERR)
      fatal(0, "redrawwin failed");
    break;
  case 21:                      /* ^U */
    ctx->len = ctx->cursor = 0;
    break;
  default:
    if(ch >= ' ' && ch <= 0x7E && ctx->len + 2 <= ctx->bufsize) {
      memmove(ctx->buffer + ctx->cursor + 1,
              ctx->buffer + ctx->cursor,
              ctx->len - ctx->cursor);
      ctx->buffer[ctx->cursor++] = ch;
      ++ctx->len;
    }
    break;
  }
  ctx->buffer[ctx->len] = 0;
  input_draw(ctx);
  if(refresh() == ERR)
    fatal(0, "refresh failed");
}

void input_draw(struct input_context *ctx) {
  int maxx, maxy, x;
  size_t pl = strlen(ctx->prompt);

  getmaxyx(stdscr, maxy, maxx);
  maxx -= pl + 1;
  if(ctx->cursor < ctx->offset)
    ctx->offset = ctx->cursor;
  if(ctx->cursor - ctx->offset >= (unsigned)maxx)
    ctx->offset = ctx->cursor - maxx + 1;
  mvaddstr(maxy - 1, 0, ctx->prompt);
  for(x = 0; x < maxx; ++x) {
    int ch = ctx->offset + x < ctx->len ? ctx->buffer[ctx->offset + x] : '_';
    if(mvaddch(maxy - 1, pl + x, ch) == ERR)
      fatal(0, "mvaddch failed");
  }
  move(maxy - 1, pl + ctx->cursor - ctx->offset);
}
