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
#ifndef PARSE_H
#define PARSE_H
/** @file parse.h
 * @brief Parsing of format strings etc
 */

#include "format.h"

/** @brief Return values from parse_element() */
enum parse_status {
  /** @brief Parse succeeded and an element was parsed */
  parse_ok,

  /** @brief Parse reached the end of the string */
  parse_eof,

  /** @brief An error was detected (only possible with @ref FORMAT_CHECK) */
  parse_error,
};

/** @brief Parse one element in a format string
 * @param ptrp Pointer into string
 * @param signp Where to store sign indicator
 * @param namep Where to store name
 * @param sizep Where to store size request
 * @param headingp Where to store heading
 * @param argp Where to store argument string
 * @param flags Flags
 * @return Parse status
 *
 * The return value can be:
 * - @ref parse_ok if an element was parsed
 * - @ref parse_eof if the end of the string was reached
 * - @ref parse_error if @ref FORMAT_CHECK was specified and something went wrong
 *
 * @p flags should be a combination of the following values:
 * - @ref FORMAT_ARGUMENT if @p ptr is in argument syntax
 * - @ref FORMAT_QUOTED if @p ptr is in quoted syntax
 * - @ref FORMAT_CHECK to check @p ptr rather than act on it
 * - @ref FORMAT_SIGN to parse a sign indicator
 * - @ref FORMAT_SIZE to parse a size request
 * - @ref FORMAT_HEADING to parse a heading
 * - @ref FORMAT_ARG to parse an argument
 *
 * @p signp will be set if it is not a null pointer and @p FORMAT_SIGN
 * is set.
 *
 * @p namep will be set if it is not a null pointer.
 * 
 * @p sizep will be set if it is not a null pointer, and @ref
 * FORMAT_SIZE is set, and a size is specified.  (The caller should set
 * the default size first.)
 *
 * @p headingp will be set if is not a null pointer (even if @ref
 * FORMAT_HEADING is clear).  If no heading is supplied then it is set
 * to @c NULL.
 *
 * @p argp will be set if is not a null pointer (even if @ref
 * FORMAT_ARG is clear).  If no argument is supplied then it is set to
 * @c NULL.
 *
 * These rules aren't very consistent - perhaps they should be
 * changed?
 *
 * If @ref FORMAT_CHECK is set then any errors cause a @ref
 * parse_error return.  If it is not specified then errors are either
 * ignored or cause a call to fatal().
 */
enum parse_status parse_element(const char **ptrp,
                                int *signp,
                                char **namep,
                                size_t *sizep,
                                char **headingp,
                                char **argp,
                                unsigned flags);

#endif /* PARSE_H */
