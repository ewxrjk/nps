/*
 * This file is part of nps.
 * Copyright (C) 2011, 13, 17 Richard Kettlewell
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
#include "format.h"
#include "buffer.h"
#include "utils.h"
#include <errno.h>
#include <stdlib.h>

static const char *parse_arg(const char *ptr,
                             char **resultp,
                             unsigned flags);

static unsigned separator(int c, unsigned flags) {
  switch(c) {
  case ' ':
  case ',':
    return 1;
  case ':':
    return flags & FORMAT_SIZE;
  case '=':
    return flags & FORMAT_HEADING;
  case '/':
    return flags & FORMAT_ARG;
  default:
    return 0;
  }
}

enum parse_status parse_element(const char **ptrp,
                                int *signp,
                                char **namep,
                                size_t *sizep,
                                char **headingp,
                                char **argp,
                                unsigned flags) {
  int sign;
  size_t size;
  char *end;
  struct buffer b[1];
  const char *ptr = *ptrp;

  if(namep) *namep = NULL;
  if(headingp) *headingp = NULL;
  if(argp) *argp = NULL;
  while(*ptr == ' ' || *ptr == ',')
    ++ptr;
  if(!*ptr) {
    *ptrp = ptr;
    return parse_eof;
  }
  buffer_init(b);
  if(flags & FORMAT_SIGN) {
    if(*ptr == '+' || *ptr == '-') {
      sign = *ptr++;
      if(!*ptr || separator(*ptr, flags)) {
        if(flags & FORMAT_CHECK)
          return parse_error;
        else
          fatal(errno, "invalid column name");
      }
    } else
      sign = 0;
    if(signp)
      *signp = sign;
  }
  while(*ptr && !separator(*ptr, flags)) {
    if(namep)
      buffer_putc(b, *ptr);
    ++ptr;
  }
  if(namep) {
    buffer_terminate(b);
    *namep = b->base;
  }
  if((flags & FORMAT_SIZE) && *ptr == ':') {
    ++ptr;
    errno = 0;
    size = strtoul(ptr, &end, 10);
    if(errno || end == ptr) {
      if(flags & FORMAT_CHECK)
        return parse_error;
      else
        fatal(errno, "invalid column size");
    }
    ptr = end;
    if(sizep)
      *sizep = size;
  }
  if((flags & FORMAT_HEADING) && *ptr == '=') {
    if(!(ptr = parse_arg(ptr + 1, headingp, flags))) {
      goto error;
    }
  } else if(headingp)
    *headingp = NULL;
  if((flags & FORMAT_ARG) && *ptr == '/') {
    if(!(ptr = parse_arg(ptr + 1, argp, flags|FORMAT_QUOTED)))
      goto error;
  } else if(argp)
    *argp = NULL;
  *ptrp = ptr;
  return parse_ok;
error:
  if(namep) {
    free(*namep);
    *namep = NULL;
  }
  if(headingp) {
    free(*headingp);
    *headingp = NULL;
  }
  return parse_error;
}

static const char *parse_arg(const char *ptr,
                             char **resultp,
                             unsigned flags) {
  int q;
  struct buffer b[1];

  buffer_init(b);
  if(flags & FORMAT_QUOTED) {
    /* property=heading extends until we hit a separator
     * property="heading" extends to the close quote; ' is allowed too
     */
    if(*ptr == '"' || *ptr == '\'') {
      q = *ptr++;
      while(*ptr && *ptr != q) {
        /* \ escapes the next character (there must be one) */
        if(*ptr == '\\') {
          if(ptr[1] != 0)
            ++ptr;
          else if(flags & FORMAT_CHECK)
            goto error;
        }
        if(resultp)
          buffer_putc(b, *ptr);
        ++ptr;
      }
      /* The close quotes must exist */
      if(*ptr == q)
        ++ptr;
      else if(flags & FORMAT_CHECK)
        goto error;
    } else {
      /* unquoted heading */
      while(*ptr && (*ptr != ' ' && *ptr != ',')) {
        if(resultp)
          buffer_putc(b, *ptr);
        ++ptr;
      }
    }
  } else {
    /* property=heading extends to the end of the argument, not until
     * the next comma; "The default header can be overridden by
     * appending an <equals-sign> and the new text of the
     * header. The rest of the characters in the argument shall be
     * used as the header text." */
    while(*ptr) {
      if(resultp)
        buffer_putc(b, *ptr);
      ++ptr;
    }
  }
  if(resultp) {
    buffer_terminate(b);
    *resultp = b->base;
  }
  return ptr;
error:
  free(b->base);
  return NULL;
}
