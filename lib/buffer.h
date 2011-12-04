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

#include <string.h>

struct tm;

/** @brief String buffer
 *
 * Use buffer_putc() and buffer_append() to add to buffers, and
 * buffer_terminate() to write a 0 terminator when finished.
 */
struct buffer {
  char *base;                   /**< @brief Base of buffer */
  size_t pos;                   /**< @brief Where to write next character */
  size_t size;                  /**< @brief Current buffer size */
};

/** @brief Initialize a string buffer
 * @param b Pointer to string buffer
 */
static inline void buffer_init(struct buffer *b) {
  b->base = 0;
  b->pos = 0;
  b->size = 0;
}

/** @brief Append a character to a string buffer
 * @param b Pointer to string buffer
 * @param c Character to append
 */
void buffer_putc_outline(struct buffer *b, int c);

/** @brief Append a string to a string buffer
 * @param b Pointer to string buffer
 * @param s String to append
 * @param n Length of string
 */
void buffer_append_n(struct buffer *b, const char *s, size_t n);

/** @brief Append a character to a string buffer
 * @param b Pointer to string buffer
 * @param c Character to append
 */
static inline void buffer_putc(struct buffer *b, int c) {
  if(b->pos < b->size)
    b->base[b->pos++] = c;
  else
    buffer_putc_outline(b, c);
}

/** @brief Append a string to a string buffer
 * @param b Pointer to string buffer
 * @param s String to append
 */
static inline void buffer_append(struct buffer *b, const char *s) {
  buffer_append_n(b, s, strlen(s));
}

/** @brief Append a formatted string to a string buffer
 * @param b Pointer to string buffer
 * @param fmt Format string
 * @param ... Arguments to @p fmt
 * @return Number of characters appended or -1 on error
 */
int buffer_printf(struct buffer *b, const char *fmt, ...)
  attribute((format(printf,2,3)));

/** @brief Append a formatted time to a strinf buffer
 * @param b Pointer to string buffer
 * @param format Format string
 * @param tm Time to format
 */
void buffer_strftime(struct buffer *b, const char *format, const struct tm *tm);

/** @brief Null-terminate a string buffer
 * @param b Pointer to string buffer
 */
void buffer_terminate(struct buffer *b);

#endif
