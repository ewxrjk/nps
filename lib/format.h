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
#ifndef FORMAT_H
#define FORMAT_H

/** @file format.h
 * @brief Formatting process information
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

#include "process.h"

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
 * @param pi Pointer to process information
 * @param tasks Process or thread IDs
 * @param ntasks Number of processes
 */
void format_columns(struct procinfo *pi, const taskident *tasks, size_t ntasks);

/** @brief Construct the heading
 * @param pi Pointer to process information
 * @param b Where to put heading string
 *
 * format_columns() must have been called.
 *
 * Any existing contents of @p b will be overwritten.  It will be
 * 0-terminated.
 */
void format_heading(struct procinfo *pi, struct buffer *b);

/** @brief Construct the output for one process
 * @param pi Pointer to process information
 * @param task Process or thread ID
 * @param b Where to store output
 *
 * format_columns() must have been called.
 *
 * Any existing contents of @p b will be overwritten.  It will be
 * 0-terminated.
 */
void format_process(struct procinfo *pi, taskident task, struct buffer *b);

/** @brief Format a single property
 * @param pi Pointer to process information
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
void format_value(struct procinfo *pi, taskident pid,
                  const char *property,
                  struct buffer *b,
                  unsigned flags);

/** @brief Set the process ordering
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

/** @brief Compare process properties
 * @param pi Pointer to process information
 * @param a First process ID
 * @param b Second process ID
 * @return -1, 0 or -1
 */
int format_compare(struct procinfo *pi, taskident a, taskident b);

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

/** @brief Retrieve the process ordering as a single string
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
 * @return @p buffer
 *
 * Supported size codes are:
 * - @c 0 to pick one automatically
 * - @c b, @c K, @c G, @c M, @c T for bytes up to terabytes
 * - @c p for pages
 *
 * In addition a negative size code means that the units should be
 * included in the output (also true for 0).
 */
char *bytes(uintmax_t n,
            int fieldwidth,
            int ch,
            char buffer[],
            size_t bufsize);

/** @brief Parse an argument string
 * @param ptr Pointer to input string
 * @param buffer Buffer for argument value
 * @param bufsize Size of buffer
 * @param flags Flags
 * @return Pointer after parsed argument
 *
 * @p flags should be a combination of the following values:
 * - @ref FORMAT_ARGUMENT if @p f is in argument syntax
 * - @ref FORMAT_QUOTED if @p f is in quoted syntax
 * - @ref FORMAT_CHECK to check @p f rather than act on it
 *
 * If @ref FORMAT_CHECK is specified then any errors cause a 0 return.
 * If it is not specified then errors are ignored.
 */
const char *format_parse_arg(const char *ptr,
                             char buffer[],
                             size_t bufsize,
                             unsigned flags);

/** @brief Format an argument string
 * @param b String buffer for output
 * @param arg Argument string
 * @param quote True to force quoting
 */
void format_get_arg(struct buffer *b, const char *arg, int quote);

/** @brief Return true if there are any rate properties
 * @param pi Process information
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
 * - @ref PROC_PROCESSES to include processes
 * - @ref PROC_THREADS to include threads
 */
int format_rate(struct procinfo *pi, unsigned procflags);

/** @brief Include hierarchy spacing in comm/args */
extern int format_hierarchy;

#endif
