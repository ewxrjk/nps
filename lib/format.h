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
#ifndef FORMAT_H
#define FORMAT_H

/** @file format.h
 * @brief Formatting task information
 *
 * There are two format string syntaxes.  The first is argument
 * syntax, which follows the SUSv4 rules.  In this format any column
 * with an assigned heading ends the list.
 *
 * The second is quoted syntax.  Here headings can be quoted, and must
 * be if they contain separator characters (space, comma, quote,
 * backslash).  In this syntax assigning a heading is possible even
 * for non-final columns.
 */

#include "tasks.h"

#include <stdint.h>
#include <sys/types.h>

struct buffer;

/** @brief Format string is in argument syntax */
#define FORMAT_ARGUMENT 0x0000

/** @brief Format string is in quoted syntax */
#define FORMAT_QUOTED 0x0001

/** @brief Check only, do not assign */
#define FORMAT_CHECK 0x0002

/** @brief Add to the list rather than resetting it */
#define FORMAT_ADD 0x0004

/** @brief Allow internal-only formats */
#define FORMAT_INTERNAL 0x0008

/** @brief Suppress K, M, etc */
#define FORMAT_RAW 0x0010

/** @brief Parse a size specification */
#define FORMAT_SIZE 0x0020

/** @brief Parse a heading */
#define FORMAT_HEADING 0x0040

/** @brief Parse an argument */
#define FORMAT_ARG 0x0080

/** @brief Parse a sign specification */
#define FORMAT_SIGN 0x0100

/** @brief Possible output syntaxes */
enum format_syntax {
  /** @brief Normal syntax
   *
   * This produces traditional 'ps' output.
   */
  syntax_normal,

  /** @brief Comma-separated value syntax */
  syntax_csv
};

/** @brief Set the high-level syntax
 * @param syntax Chosen syntax
 *
 * Possible syntax values are @ref syntax_normal (which is the
 * default) and @ref syntax_csv.
 */
void format_syntax(enum format_syntax syntax);

/** @brief Add to the format list
 * @param f Format string
 * @param flags Flags
 * @return Nonzero on success, 0 if @p f is not valid
 *
 * The format list is cumulative.
 *
 * @p flags should be a combination of the following values:
 * - @ref FORMAT_ARGUMENT if @p f is in argument syntax
 * - @ref FORMAT_QUOTED if @p f is in quoted syntax
 * - @ref FORMAT_CHECK to check @p f rather than act on it
 * - @ref FORMAT_ADD to add to the list rather than replacing it
 *
 * If @ref FORMAT_CHECK is specified then any errors cause a 0 return.
 * If it is not specified then errors are either ignored or cause a
 * call to fatal().
 */
int format_set(const char *f, unsigned flags);

/** @brief Clear the format list
 */
void format_clear(void);

/** @brief Set internal column sizes
 * @param ti Pointer to task information
 * @param tasks Process or thread IDs
 * @param ntasks Number of tasks
 */
void format_columns(struct taskinfo *ti, const taskident *tasks, size_t ntasks);

/** @brief Construct the heading
 * @param ti Pointer to task information
 * @param b Where to put heading string
 *
 * format_columns() must have been called.
 *
 * Any existing contents of @p b will be overwritten.  It will be
 * 0-terminated.
 */
void format_heading(struct taskinfo *ti, struct buffer *b);

/** @brief Construct the output for one task
 * @param ti Pointer to task information
 * @param task Process or thread ID
 * @param b Where to store output
 *
 * format_columns() must have been called.
 *
 * Any existing contents of @p b will be overwritten.  It will be
 * 0-terminated.
 */
void format_task(struct taskinfo *ti, taskident task, struct buffer *b);

/** @brief Format a single property
 * @param ti Pointer to task information
 * @param task Process or thread ID
 * @param property Property name
 * @param b Where to store output
 * @param flags Flags
 *
 * @p flags should be a combination of:
 * - @ref FORMAT_RAW to suppress use of K, M, etc
 *
 * Any existing contents of @p b will be overwritten.  It will be
 * 0-terminated.
 */
void format_value(struct taskinfo *ti, taskident task,
                  const char *property,
                  struct buffer *b,
                  unsigned flags);

/** @brief Set the task ordering
 * @param ordering Ordering specification
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
int format_ordering(const char *ordering, unsigned flags);

/** @brief Compare task properties
 * @param ti Pointer to task information
 * @param a First task ID
 * @param b Second task ID
 * @return -1, 0 or -1
 */
int format_compare(struct taskinfo *ti, taskident a, taskident b);

/** @brief Return formatting help
 * @return NULL-terminated list of strings
 *
 * Caller is responsible for freeing the returned array including all
 * the strings in it.
 */
char **format_help(void);

/** @brief Retrieve the format list as a single string in quoted syntax
 * @return Format list
 *
 * Caller is responsible for freeing the returned string.
 */
char *format_get(void);

/** @brief Retrieve the task ordering as a single string
 * @return Orderinf specification
 *
 * Caller is responsible for freeing the returned string.
 */
char *format_get_ordering(void);

/** @brief Format a value in human-friendly units
 * @param n Integer to format
 * @param fieldwidth Field width (as per printf)
 * @param ch Size code
 * @param buffer Output buffer
 * @param bufsize Output buffer size
 * @param cutoff Multiplier for automatic size choice
 * @return @p buffer
 *
 * Supported size codes are:
 * - @c 0 to pick one automatically
 * - @c b, @c K, @c G, @c M, @c T for bytes up to terabytes
 * - @c p for pages
 *
 * When picking a size automatically (i.e. when @p ch is 0), @p cutoff
 * determines the minimum whole number of kilobytes that will be
 * displayed as bytes, of megabytes that will be displayed as
 * kilobytes, etc.  A value of 1 will always use the smallest unit
 * that can possibly fit.  A @p cutoff of 0 will be upgraded to 1.
 *
 * In addition a negative size code means that the units should be
 * included in the output (also true for 0).
 */
char *bytes(uintmax_t n,
            int fieldwidth,
            int ch,
            char buffer[],
            size_t bufsize,
            unsigned cutoff);

/** @brief Parse an argument string for use with bytes()
 * @param arg Argment string (or NULL)
 * @param cutoff Where to store @c cutoff value
 * @param flags
 * @return @c ch value
 *
 * @p flags may contain:
 * - @ref FORMAT_RAW for force a return of 'b'.
 */
int parse_byte_arg(const char *arg, unsigned *cutoff, unsigned flags);

/** @brief Format an argument string
 * @param b String buffer for output
 * @param arg Argument string
 * @param quote True to force quoting
 */
void format_get_arg(struct buffer *b, const char *arg, int quote);

/** @brief Return true if there are any rate properties
 * @param ti Pointer to task information
 * @param procflags Flags
 * @return Nonzero iff there are any rate properties
 *
 * This function not only checks whether there are any rate properties
 * but also ensures that their baseline values have been fetched.  It
 * is used in conjunction with a brief sleep and a second sample to
 * ensure that rate properties represent recent information rather
 * than lifetime values.
 *
 * @p flags should be a combination of:
 * - @ref TASK_PROCESSES to include processes
 * - @ref TASK_THREADS to include threads
 */
int format_rate(struct taskinfo *ti, unsigned procflags);

/** @brief Format an integer value
 * @param im Value to format
 * @param b String buffer for output
 * @param base Format specifier (o/u/x/X/d)
 *
 * In CSV mode, the base is forced to decimal.
 */
void format_integer(intmax_t im, struct buffer *b, int base);

/** @brief Format an address value
 * @param im Value to format
 * @param b String buffer for output
 *
 * Normally the address is rendered as 8, 12 or 16 hex digits.
 * In CSV mode, decimal is used.
 */
void format_addr(uintmax_t im, struct buffer *b);

/** @brief Format a time interval
 * @param seconds Number of seconds
 * @param b String buffer for output
 * @param always_hours Always include the hours
 * @param columnsize The column size
 * @param format The format string or a null pointer
 * @param flags Flags
 *
 * In CSV mode, or if @p flags includes @ref FORMAT_RAW, a decimal
 * second count is written regardless of other parameters.
 *
 * Otherwise, if a non-NULL format string is supplied then that is
 * used as per @ref strfelapsed.
 *
 * Otherwise, the format [[D-]HH:]MM:SS is used (with @p always_hours
 * being nonzero forcing the hours to always be listed), unless the
 * result exceeds @p columnsize, in which case one of DdHH (for a day
 * or longer), HHhMM (for an hour or longer) or MMmSS (for less than
 * an hour) are used.  Here, D means the number of days in decimal,
 * and HH, MM and SS the number of hours, minutes an seconds in
 * decimal and occupying at least two digits.
 */
void format_interval(long seconds, struct buffer *b,
                     int always_hours, size_t columnsize,
                     const char *format,
                     unsigned flags);

/** @brief Format a time
 * @param when Timestamp
 * @param b String buffer for output
 * @param columnsize The column size
 * @param format The format string or a null pointer
 * @param flags Flags
 *
 * If flags includes @ref FORMAT_RAW, the time is always written as a
 * decimal second count.
 *
 * Otherwise, if a format string is specified, that is used as per
 * strftime().
 *
 * Otherwise, if it will fit in the column size and the column size is
 * not @c SIZE_MAX, ISO format (YYYY-MM-DDTHH:MM:SS) is used.
 *
 * Otherwise, if the timestamp is from today and then HH:MM:SS is used
 * unless it will not fit in the column size, in which case HH:MM is
 * used.
 *
 * Otherwise, YYYY-MM-DD is used, unless the timestamp is from this
 * year and it won't fit, in which case MM-DD is used.
 *
 * Local time is always used.
 */
void format_time(time_t when, struct buffer *b, size_t columnsize,
                 const char *format, unsigned flags);

/** @brief Include hierarchy spacing in comm/args */
extern int format_hierarchy;

#endif
