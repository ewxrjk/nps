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
#ifndef SELECTORS_H
#define SELECTORS_H

/** @file selectors.h
 * @brief Selecting tasks
 */

#include "tasks.h"

#include <sys/types.h>
#include <regex.h>

struct taskinfo;

/** @brief Argument type for selectors
 *
 * Use split_arg() to split an argument string into argument objects.
 */
union arg {
  uid_t uid;                    /**< @brief User ID
                                 * Set by arg_user(). */

  gid_t gid;                    /**< @brief Group ID
                                 * Set by arg_group(). */

  pid_t pid;                    /**< @brief Process ID
                                 * Set by arg_process(). */

  int tty;                      /**< @brief Terminal device number
                                 * Set by arg_tty(). */

  char *string;                 /**< @brief String value */

  regex_t regex;                /**< @brief Compiled regexp */

  int operator;                 /**< @brief Comparison operator */
};

/** @brief Parse a user name or UID
 * @param s Input string
 * @return User ID
 *
 * Sets @c uid field of @ref arg.
 */
union arg arg_user(const char *s);

/** @brief Parse a group name or GID
 * @param s Input string
 * @return Group ID
 *
 * Sets @c gid field of @ref arg.
 */
union arg arg_group(const char *s);

/** @brief Parse a process ID
 * @param s Input string
 * @return UID
 *
 * Sets @c pid field of @ref arg.
 */
union arg arg_process(const char *s);

/** @brief Parse a terminal name
 * @param s Input string
 * @return UID
 *
 * Terminal names may be abbreviated by leaving out the initial @c
 * /dev or @c /dev/tty .
 *
 * Sets @c tty field of @ref arg.
 */
union arg arg_tty(const char *s);

/** @brief Split an argument string
 * @param arg Argument string (modified!)
 * @param type Type-specific convertor
 * @param lenp Where to store argument count
 * @return Pointer to first argument
 *
 * Use arg_user(), arg_group(), arg_process() or arg_tty() as the @p
 * type argument to control parsing of each element.
 */
union arg *split_arg(char *arg,
                     union arg (*type)(const char *s),
                     size_t *lenp);

// ----------------------------------------------------------------------------

/** @brief Signature of a selector function
 * @param ti Pointer to task information
 * @param pid Process ID
 * @param args Selector argument as passed to @ref select_add()
 * @param nargs Argument cout as passed to @ref select_add()
 * @return Nonzero to select @p pid
 */
typedef int select_function(struct taskinfo *ti, taskident task,
                            union arg *args, size_t nargs);

/** @brief Register a selector function
 * @param sfn Selector function
 * @param args Selector argument
 * @param nargs Argument count
 */
void select_add(select_function *sfn, union arg *args, size_t nargs);

/** @brief Register a string or regexp match selector function
 * @param expr Match expression
 *
 * Match expressions take the form PROPERTY=VALUE or PROPERTY~REGEXP.
 */
void select_match(const char *expr);

/** @brief Test whether a task should be selected
 * @param ti Pointer to task information
 * @param pident Process/thread identifier
 * @return Nonzero to select @p pid
 *
 * If at least one reistered selector function returns nonzero then
 * the return value is 0.  If all return zero then the return value is
 * zero.
 */
int select_test(struct taskinfo *ti, taskident pident);

/** @brief Set the default selector
 * @param sfn Selector function
 * @param args Selector argument
 * @param nargs Argument cout as passed to @ref select_add()
 *
 * Equivalent to add_select() unless any selectors have been
 * registered, when it does nothing.
 */
void select_default(select_function *sfn, union arg *args, size_t nargs);

/** @brief Clear all selectors */
void select_clear(void);

// ---------------------------------------------------------------------------

/** @brief Select processes that have a controlling terminal
 * @param ti Pointer to task information
 * @param task Process/thread ID
 * @param args Selector argument as passed to @ref select_add()
 * @param nargs Argument cout as passed to @ref select_add()
 * @return Nonzero to select @p pid
 *
 * @p args and @p nargs are not used.
 */
int select_has_terminal(struct taskinfo *ti, taskident task,
                        union arg *args, size_t nargs);

/** @brief Select all processes
 * @param ti Pointer to task information
 * @param task Process/thread ID
 * @param args Selector argument as passed to @ref select_add()
 * @param nargs Argument cout as passed to @ref select_add()
 * @return Nonzero to select @p pid
 *
 * @p args and @p nargs are not used.
 */
int select_all(struct taskinfo *ti, taskident task, union arg *args, size_t nargs);

/** @brief Select processes that are not session leaders
 * @param ti Pointer to task information
 * @param task Process/thread ID
 * @param args Selector argument as passed to @ref select_add()
 * @param nargs Argument cout as passed to @ref select_add()
 * @return Nonzero to select @p pid
 *
 * @p args and @p nargs are not used.
 */
int select_not_session_leader(struct taskinfo *ti,
                              taskident task, union arg *args, size_t nargs);

/** @brief Select specific processes
 * @param ti Pointer to task information
 * @param task Process/thread ID
 * @param args Selector argument as passed to @ref select_add()
 * @param nargs Argument cout as passed to @ref select_add()
 * @return Nonzero to select @p pid
 *
 * Parse the argument string with @ref arg_process().
 */
int select_pid(struct taskinfo *ti, taskident task, union arg *args, size_t nargs);

/** @brief Select processes by parent process ID
 * @param ti Pointer to task information
 * @param task Process/thread ID
 * @param args Selector argument as passed to @ref select_add()
 * @param nargs Argument cout as passed to @ref select_add()
 * @return Nonzero to select @p pid
 *
 * Parse the argument string with @ref arg_process().
 */
int select_ppid(struct taskinfo *ti, taskident task, union arg *args, size_t nargs);

/** @brief Select processes by ancestor process ID
 * @param ti Pointer to task information
 * @param task Process/thread ID
 * @param args Selector argument as passed to @ref select_add()
 * @param nargs Argument cout as passed to @ref select_add()
 * @return Nonzero to select @p pid
 *
 * Parse the argument string with @ref arg_process().
 */
int select_apid(struct taskinfo *ti, taskident task, union arg *args, size_t nargs);

/** @brief Select processes by controlling terminal
 * @param ti Pointer to task information
 * @param task Process/thread ID
 * @param args Selector argument as passed to @ref select_add()
 * @param nargs Argument cout as passed to @ref select_add()
 * @return Nonzero to select @p pid
 *
 * Parse the argument string with @ref arg_tty().
 */
int select_terminal(struct taskinfo *ti, taskident task,
                    union arg *args, size_t nargs);

/** @brief Select processes by session leader
 * @param ti Pointer to task information
 * @param task Process/thread ID
 * @param args Selector argument as passed to @ref select_add()
 * @param nargs Argument cout as passed to @ref select_add()
 * @return Nonzero to select @p pid
 *
 * Parse the argument string with @ref arg_process().
 */
int select_leader(struct taskinfo *ti, taskident task,
                  union arg *args, size_t nargs);

/** @brief Select processes by real GID
 * @param ti Pointer to task information
 * @param task Process/thread ID
 * @param args Selector argument as passed to @ref select_add()
 * @param nargs Argument cout as passed to @ref select_add()
 * @return Nonzero to select @p pid
 *
 * Parse the argument string with @ref arg_group().
 */
int select_rgid(struct taskinfo *ti, taskident task, union arg *args, size_t nargs);

/** @brief Select processes by effective GID
 * @param ti Pointer to task information
 * @param task Process/thread ID
 * @param args Selector argument as passed to @ref select_add()
 * @param nargs Argument cout as passed to @ref select_add()
 * @return Nonzero to select @p pid
 *
 * Parse the argument string with @ref arg_group().
 */
int select_egid(struct taskinfo *ti, taskident task, union arg *args, size_t nargs);

/** @brief Select processes by real UID
 * @param ti Pointer to task information
 * @param task Process/thread ID
 * @param args Selector argument as passed to @ref select_add()
 * @param nargs Argument cout as passed to @ref select_add()
 * @return Nonzero to select @p pid
 *
 * Parse the argument string with @ref arg_user().
 */
int select_euid(struct taskinfo *ti, taskident task, union arg *args, size_t nargs);

/** @brief Select processes by effective UID
 * @param ti Pointer to task information
 * @param task Process/thread ID
 * @param args Selector argument as passed to @ref select_add()
 * @param nargs Argument cout as passed to @ref select_add()
 * @return Nonzero to select @p pid
 *
 * Parse the argument string with @ref arg_user().
 */
int select_ruid(struct taskinfo *ti, taskident task,
                union arg *args, size_t nargs);

/** @brief Select processes with current effective UID and terminal
 * @param ti Pointer to task information
 * @param task Process/thread ID
 * @param args Selector argument as passed to @ref select_add()
 * @param nargs Argument cout as passed to @ref select_add()
 * @return Nonzero to select @p pid
 *
 * @p args and @p nargs are not used.
 */
int select_uid_tty(struct taskinfo *ti, taskident task, union arg *args,
                   size_t nargs);

/** @brief Select non-idle processes
 * @param ti Pointer to task information
 * @param task Process/thread ID
 * @param args Selector argument as passed to @ref select_add()
 * @param nargs Argument cout as passed to @ref select_add()
 * @return Nonzero to select @p pid
 *
 * @p args and @p nargs are not used.
 */
int select_nonidle(struct taskinfo *ti, taskident task, union arg *args,
                   size_t nargs);

/** @brief Select by string match
 * @param ti Pointer to task information
 * @param task Process/thread ID
 * @param args Selector argument as passed to @ref select_add()
 * @param nargs Argument cout as passed to @ref select_add()
 * @return Nonzero to select @p pid
 *
 * @p nargs must be 2.  The first argument should be a property name
 * and the second a value
 */
int select_string_match(struct taskinfo *ti, taskident task, union arg *args,
                        size_t nargs);

/** @brief Select by regular expression match
 * @param ti Pointer to task information
 * @param task Process/thread ID
 * @param args Selector argument as passed to @ref select_add()
 * @param nargs Argument cout as passed to @ref select_add()
 * @return Nonzero to select @p pid
 *
 * @p nargs must be 2.  The first argument should be a property name
 * and the second a compiled regexp.
 */
int select_regex_match(struct taskinfo *ti, taskident task, union arg *args,
                       size_t nargs);

/** @brief Select by comparison
 * @param ti Pointer to task information
 * @param task Process/thread ID
 * @param args Selector argument as passed to @ref select_add()
 * @param nargs Argument cout as passed to @ref select_add()
 * @return Nonzero to select @p pid
 *
 * @p nargs must be 3.  The first argument should be a property name;
 * the second a comparison operator; and the third a value to compare
 * against.
 *
 * The possible comparions operators are:
 * - '<'
 * - '='
 * - @ref NE
 * - '>'
 * - @ref LE
 * - @ref GE
 *
 * For @ref IDENTICAL, use select_string_match() instead.  (Perhaps
 * this should be made more uniform?)
 */
int select_compare(struct taskinfo *ti, taskident task, union arg *args,
                   size_t nargs);

/** @brief String identity comparison operator */
#define IDENTICAL 0x2261

/** @brief Inequality comparison operator */
#define NE 0x2260

/** @brief "Less than or equal to" comparison operator */
#define LE 0x2264

/** @brief "Greater than or equal to" comparison operator */
#define GE 0x2265

/** @brief Override own UID */
extern uid_t forceuid;

#endif /* SELECTORS_H */

