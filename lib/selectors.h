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
 * @brief Selecting processes
 */

#include <sys/types.h>

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
 * @param pid Process ID
 * @param args Selector argument as passed to @ref select_add()
 * @param nargs Argument cout as passed to @ref select_add()
 * @return Nonzero to select @p pid
 */
typedef int select_function(pid_t pid, union arg *args, size_t nargs);

/** @brief Register a selector function
 * @param sfn Selector function
 * @param args Selector argument
 * @param nargs Argument count
 */
void select_add(select_function *sfn, union arg *args, size_t nargs);

/** @brief Test whether a proces should be selected
 * @param pid Process ID
 * @return Nonzero to select @p pid
 *
 * If at least one reistered selector function returns nonzero then
 * the return value is 0.  If all return zero then the return value is
 * zero.
 */
int select_test(pid_t pid);

/** @brief Set the default selector
 * @param sfn Selector function
 * @param args Selector argument
 * @param nargs Argument cout as passed to @ref select_add()
 *
 * Equivalent to add_select() unless any selectors have been
 * registered, when it does nothing.
 */
void select_default(select_function *sfn, union arg *args, size_t nargs);

// ---------------------------------------------------------------------------

/** @brief Select processes that have a controlling terminal
 * @param pid Process ID
 * @param args Selector argument as passed to @ref select_add()
 * @param nargs Argument cout as passed to @ref select_add()
 * @return Nonzero to select @p pid
 *
 * @p args and @p nargs are not used.
 */
int select_has_terminal(pid_t pid, union arg *args, size_t nargs);

/** @brief Select all processes
 * @param pid Process ID
 * @param args Selector argument as passed to @ref select_add()
 * @param nargs Argument cout as passed to @ref select_add()
 * @return Nonzero to select @p pid
 *
 * @p args and @p nargs are not used.
 */
int select_all(pid_t pid, union arg *args, size_t nargs);

/** @brief Select processes that are not session leaders
 * @param pid Process ID
 * @param args Selector argument as passed to @ref select_add()
 * @param nargs Argument cout as passed to @ref select_add()
 * @return Nonzero to select @p pid
 *
 * @p args and @p nargs are not used.
 */
int select_not_session_leader(pid_t pid, union arg *args, size_t nargs);

/** @brief Select specific processes
 * @param pid Process ID
 * @param args Selector argument as passed to @ref select_add()
 * @param nargs Argument cout as passed to @ref select_add()
 * @return Nonzero to select @p pid
 *
 * Parse the argument string with @ref arg_process().
 */
int select_pid(pid_t pid, union arg *args, size_t nargs);

/** @brief Select processes by controlling terminal
 * @param pid Process ID
 * @param args Selector argument as passed to @ref select_add()
 * @param nargs Argument cout as passed to @ref select_add()
 * @return Nonzero to select @p pid
 *
 * Parse the argument string with @ref arg_tty().
 */
int select_terminal(pid_t pid, union arg *args, size_t nargs);

/** @brief Select processes by session leader
 * @param pid Process ID
 * @param args Selector argument as passed to @ref select_add()
 * @param nargs Argument cout as passed to @ref select_add()
 * @return Nonzero to select @p pid
 *
 * Parse the argument string with @ref arg_process().
 */
int select_leader(pid_t pid, union arg *args, size_t nargs);

/** @brief Select processes by real GID
 * @param pid Process ID
 * @param args Selector argument as passed to @ref select_add()
 * @param nargs Argument cout as passed to @ref select_add()
 * @return Nonzero to select @p pid
 *
 * Parse the argument string with @ref arg_group().
 */
int select_rgid(pid_t pid, union arg *args, size_t nargs);

/** @brief Select processes by effective GID
 * @param pid Process ID
 * @param args Selector argument as passed to @ref select_add()
 * @param nargs Argument cout as passed to @ref select_add()
 * @return Nonzero to select @p pid
 *
 * Parse the argument string with @ref arg_group().
 */
int select_egid(pid_t pid, union arg *args, size_t nargs);

/** @brief Select processes by real UID
 * @param pid Process ID
 * @param args Selector argument as passed to @ref select_add()
 * @param nargs Argument cout as passed to @ref select_add()
 * @return Nonzero to select @p pid
 *
 * Parse the argument string with @ref arg_user().
 */
int select_euid(pid_t pid, union arg *args, size_t nargs);

/** @brief Select processes by effective UID
 * @param pid Process ID
 * @param args Selector argument as passed to @ref select_add()
 * @param nargs Argument cout as passed to @ref select_add()
 * @return Nonzero to select @p pid
 *
 * Parse the argument string with @ref arg_user().
 */
int select_ruid(pid_t pid, union arg *args, size_t nargs);

/** @brief Select processes with current effective UID and terminal
 * @param pid Process ID
 * @param args Selector argument as passed to @ref select_add()
 * @param nargs Argument cout as passed to @ref select_add()
 * @return Nonzero to select @p pid
 *
 * @p args and @p nargs are not used.
 */
int select_uid_tty(pid_t pid, union arg *args, size_t nargs);

#endif /* SELECTORS_H */

