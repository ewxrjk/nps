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
#ifndef UTILS_H
#define UTILS_H

/** @file utils.h
 * @brief Miscelleneous utilities
 */

#include <sys/types.h>

/** @brief Convert a ticks-since-boot count into a timestamp
 * @param ticks Ticks since boot
 * @return Equivalent timestamp
 */
time_t clock_to_time(unsigned long long ticks);

/** @brief Convert a count of ticks into a number of seconds
 * @param ticks Ticks since boot
 * @return Equivalent number of seconds
 */
double clock_to_seconds(unsigned long long ticks);

// ----------------------------------------------------------------------------

/** @brief Compute the path of a device given its device number
 * @param type 1 for block devices and 0 for character devices
 * @param dev Device number
 * @return Pointer to path or @c NULL if not found
 *
 * Don't free the return value.
 */
char *device_path(int type, dev_t dev);

// ----------------------------------------------------------------------------

/** @brief Allocate memory
 * @param n Number of bytes to allocate
 * @return Pointer to allocated space
 *
 * Calls fatal() on error.
 */
void *xmalloc(size_t n);

/** @brief Reallocate memory
 * @param ptr Pointer to existing allocation or NULL
 * @param n Number of bytes to allocate
 * @return Pointer to adjusted space
 *
 * Calls fatal() on error.
 */
void *xrealloc(void *ptr, size_t n);

/** @brief Reallocate memory
 * @param ptr Pointer to existing allocation or NULL
 * @param n Number of objects to allocate
 * @param s Size of one object
 * @return Pointer to adjusted space
 *
 * Calls fatal() on error.
 */
void *xrecalloc(void *ptr, size_t n, size_t s);

/** @brief Duplicate a string
 * @param s String to duplicate
 * @return Copy of @p s
 *
 * Calls fatal() on error.
 */
char *xstrdup(const char *s);

/** @brief Report a fatal error
 * @param errno_value Error code or 0
 * @param fmt Format string
 * @param ... Arguments for format string
 *
 * The format string and arguments are interpreted as per printf().
 * This function never returns; it always calls exit().
 */
void fatal(int errno_value, const char *fmt, ...)
  attribute((noreturn))
  attribute((format (printf, 2, 3)));

/** @brief Function to call before issuing error message */
extern int (*onfatal)(void);

// ----------------------------------------------------------------------------

#endif /* MEMORY_H */

