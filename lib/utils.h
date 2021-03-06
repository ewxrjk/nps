/*
 * This file is part of nps.
 * Copyright (C) 2011, 12, 13 Richard Kettlewell
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
#include <inttypes.h>

struct buffer;

/** @brief Convert a ticks-since-boot count into a timestamp
 * @param ticks Ticks since boot
 * @return Equivalent timestamp
 */
double clock_to_time(unsigned long long ticks);

/** @brief Convert a count of ticks into a number of seconds
 * @param ticks Ticks since boot
 * @return Equivalent number of seconds
 */
double clock_to_seconds(unsigned long long ticks);

/** @brief Return the current time
 * @return Timestamp
 */
double clock_now(void);

/** @brief Get the current time
 * @param ts Where to put time or NULL
 * @return Timestamp in seconds
 */
time_t timespec_now(struct timespec *ts);

/** @brief Override for time */
extern struct timespec forcetime;

/** @brief Format elapsed time
 * @param b String buffer for output
 * @param format Format string
 * @param seconds Number of seconds
 *
 * The format string consists of ordinary characters and format specifiers.
 *
 * A format specifiers always starts with "%".
 *
 * This is followed by an optional minimum field width, as a decimal
 * integer.  If this starts with a "0" then the field will be padded
 * with zeroes, otherwise with spaces.  The default minimum field
 * width is 0.  The field width includes the sign if the value is
 * negative.
 *
 * Next is an optional precision, being a "." followed by a decimal
 * integer.  This is the minimum number of digits to produce.  The
 * default is 1, ensuring that 0 doesn't format to an empty string.
 *
 * Next is an optional "?".  If this is present then the value will be
 * skipped entirely if it is 0.
 *
 * Next is an optional "+" and a follower character.  If this is
 * present it will be written after the converted value (if it is not
 * skipped).  The follower character is not counted towards the field
 * width.
 *
 * Finally there is a conversion specifier:
 * - @c % to write a '%' (in which case all the other details are ignored)
 * - @c d to write the day count
 * - @c h to write the number of hours
 * - @c H to write the number of hours, modulo one day
 * - @c m to write the number of minutes
 * - @c M to write the number of minutes, modulo one hour
 * - @c s to write the number of seconds
 * - @c S to write the number of seconds, modulo one minute
 *
 */
void strfelapsed(struct buffer *b, const char *format, intmax_t seconds);

// ----------------------------------------------------------------------------

/** @brief Compute the path of a device given its device number
 * @param type 1 for block devices and 0 for character devices
 * @param dev Device number
 * @return Pointer to path or @c NULL if not found
 *
 * Don't free the return value.
 */
const char *device_path(int type, dev_t dev);

/** @brief Return the device ID of a terminal
 * @param name (Possibly abbreviated) terminal name
 * @return Device ID or -1
 */
dev_t tty_id(const char *name);

/** @brief Override for device list */
extern const char *forcedev;

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

/** @brief Duplicate a string
 * @param s String to duplicate
 * @param n Length of string
 * @return Copy of @p s
 *
 * Calls fatal() on error.
 */
char *xstrndup(const char *s, size_t n);

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

/** @brief Function to call before issuing error message
 * @return Ignored
 */
extern int (*onfatal)(void);

/** @brief Function to call to exit
 * @param status Exit status
 */
extern void (*terminate)(int status)
  attribute((noreturn))
;

/** @brief Free a list of strings
 * @param strings NULL-terminated list of strings
 *
 * @p strings and all its members are freed.
 */
void free_strings(char **strings);

// ----------------------------------------------------------------------------

/** @brief Find the minimum
 * @param x Value to compare
 * @param y Value to compare
 * @return Least of @p x and @p y
 *
 * Note that whichever is less of @p x and @p y is evaluated twice.
 */
#define min(x,y) ((x) < (y) ? (x) : (y))

/** @brief Find the maximum
 * @param x Value to compare
 * @param y Value to compare
 * @return Greatest of @p x and @p y
 *
 * Note that whichever is greatest of @p x and @p y is evaluated twice.
 */
#define max(x,y) ((x) > (y) ? (x) : (y))

// ----------------------------------------------------------------------------

/** @brief Return the name of a signal
 * @param sig Signal number
 * @param buffer Buffer for signal name
 * @param bufsize Size of @p buffer
 * @return Pointer to signal name
 *
 * The return value need not be @p buffer - it may be a pointer to a
 * string literal instead.
 */
const char *signame(int sig, char buffer[], size_t bufsize);

// ----------------------------------------------------------------------------

/** @brief Parse a time interval
 * @param s Time interval
 * @return Number of seconds
 *
 * Currently time intervals are just counts of seconds.
 * This could be improved.
 */
double parse_interval(const char *s);

// ----------------------------------------------------------------------------

/** @brief Return the number of seconds the system has been up */
double uptime_up(void);

/** @brief Return the time at which the system was booted */
double uptime_booted(void);

// ----------------------------------------------------------------------------

/** @brief Error-checking asprintf() wrapper
 * @param sp Where to put result
 * @param format Format string
 * @param ... Arguments
 * @return Number of characters stored
 *
 * Calls fatal() on error.
 */
int xasprintf(char **sp, const char *format, ...)
  attribute((format (printf, 2, 3)));

/** @brief Ensure FPU is in 64-bit mode
 *
 * This ensures consistent operation between architectures.
 */
static inline void fpfixup(void) {
#if __i386__
  uint16_t fpucontrol;
  __asm__("fstcw %0" : "=m"(fpucontrol) : );
  fpucontrol = (fpucontrol & 0xFCFF) | 0x0200;
  __asm__("fldcw %0" : : "m" (fpucontrol));
#endif
}

#endif /* UTILS_H */

