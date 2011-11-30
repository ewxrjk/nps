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
#ifndef SYSINFO_H
#define SYSINFO_H

/** @file sysinfo.h
 * @brief System information
 */

#include <stddef.h>

struct procinfo;
struct buffer;

/** @brief Set system information format
 * @param format Format string
 * @param flags Flags
 * @return Nonzero on success, 0 if @p ordering is not valid
 *
 * @p flags should be a combination of the following values:
 * - @ref FORMAT_CHECK to check @p ordering rather than act on it
 * - @ref FORMAT_ADD to add to the list rather than replacing it
 *
 * If @ref FORMAT_CHECK is specified then any errors cause a 0 return.
 * If it is not specified then errors are either ignored or cause a
 * call to fatal().
 */
int sysinfo_set(const char *format,
                   unsigned flags);

/** @brief Reset cached system information 
 * @return The number of sysinfo elements
 */
size_t sysinfo_reset(void);

/** @brief Get a system information element
 * @param pi Pointer to process information
 * @param n Element number
 * @param b String buffer for output
 * @return 0 on success, -1 if there are no more elements
 *
 * Any existing contents of @p b will be overwritten.  It will not be
 * 0-terminated.
 */
int sysinfo_format(struct procinfo *pi, size_t n, struct buffer *b);

/** @brief Return system information help
 * @return NULL-terminated list of strings
 *
 * Caller is responsible for freeing the returned array including all
 * the strings in it.
 */
char **sysinfo_help(void);

/** @brief Retrieve system information format as a single string
 * @return Format string
 *
 * Caller is responsible for freeing the returned string.
 */
char *sysinfo_get(void);

#endif
