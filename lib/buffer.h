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
#ifndef BUFFER_H
#define BUFFER_H

/** @file buffer.h
 * @brief String buffer
 */

/** @brief String buffer
 *
 * Use buffer_putc() and buffer_append() to add to buffers, and
 * buffer_terminate() to write a 0 terminator when finished.
 *
 * You can write more than @p size bytes to a buffer, but the excess
 * bytes will be discarded.
 *
 * Buffers can have size 0, in which case no bytes are ever written,
 * not even a terminator.  In this case @c base is never used so it is
 * safe to make it a null pointer.
 */
struct buffer {
  char *base;                   /**< @brief Base of buffer */
  size_t pos;                   /**< @brief Where to write next character
                                 * Can be larger than @p size */
  size_t size;                  /**< @brief Buffer size */
};

/** @brief Append a character to a string buffer
 * @param b Pointer to string buffer
 * @param c Character to append
 */
static inline void buffer_putc(struct buffer *b, int c) {
  if(b->pos < b->size)
    b->base[b->pos] = c;
  ++b->pos;
}

/** @brief Append a string to a string buffer
 * @param b Pointer to string buffer
 * @param s String to append
 */
static inline void buffer_append(struct buffer *b, const char *s) {
  if(s)
    while(*s)
      buffer_putc(b, *s++);
}

/** @brief Append a string to a string buffer
 * @param b Pointer to string buffer
 * @param s String to append
 * @param n Length of string
 */
static inline void buffer_append_n(struct buffer *b, const char *s, size_t n) {
  if(s) {
    while(n--)
      buffer_putc(b, *s++);
  }
}

/** @brief Null-terminate a string buffer
 * @param b Pointer to string buffer
 */
static inline void buffer_terminate(struct buffer *b) {
  if(b->pos < b->size)
    b->base[b->pos] = 0;
  else if(b->size)
    b->base[b->size - 1] = 0;
}

#endif
