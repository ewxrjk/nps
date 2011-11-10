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
#ifndef PROCESS_H
#define PROCESS_H

/** @file process.h
 * @brief Retreiving process information
 */

#include <sys/types.h>
#include <inttypes.h>

/** @brief (Re-)enumerate all processes
 *
 * Retrieves a list of processes.  If it has already been called, all
 * existing data is discarded.
 */
void proc_enumerate(void);

/** @brief Retrieve the session ID of a process
 * @param pid Process ID
 * @return Session ID or -1
 *
 * proc_enumerate() must have been called first.
 */
pid_t proc_get_session(pid_t pid);

/** @brief Retrieve the real user ID of a process
 * @param pid Process ID
 * @return User ID or -1
 *
 * proc_enumerate() must have been called first.
 */
uid_t proc_get_ruid(pid_t pid);

/** @brief Retrieve the effective user ID of a process
 * @param pid Process ID
 * @return User ID or -1
 *
 * proc_enumerate() must have been called first.
 */
uid_t proc_get_euid(pid_t pid);

/** @brief Retrieve the real group ID of a process
 * @param pid Process ID
 * @return Group ID or -1
 *
 * proc_enumerate() must have been called first.
 */
gid_t proc_get_rgid(pid_t pid);

/** @brief Retrieve the effective group ID of a process
 * @param pid Process ID
 * @return Group ID or -1
 *
 * proc_enumerate() must have been called first.
 */
gid_t proc_get_egid(pid_t pid);

/** @brief Retrieve the parent process ID of a process
 * @param pid Process ID
 * @return Parent process ID
 *
 * proc_enumerate() must have been called first.
 */
pid_t proc_get_ppid(pid_t pid);

/** @brief Retrieve the process group ID of a process
 * @param pid Process ID
 * @return Process group ID
 *
 * proc_enumerate() must have been called first.
 */
pid_t proc_get_pgid(pid_t pid);

/** @brief Retrieve the terminal number of a process
 * @param pid Process ID
 * @return Terminal number or -1
 *
 * proc_enumerate() must have been called first.
 */
int proc_get_tty(pid_t pid);

/** @brief Retrieve the command name of a process
 * @param pid Process ID
 * @return Command name
 *
 * proc_enumerate() must have been called first.
 */
const char *proc_get_comm(pid_t pid);

/** @brief Retrieve the command line of a process
 * @param pid Process ID
 * @return Command line
 *
 * proc_enumerate() must have been called first.
 */
const char *proc_get_cmdline(pid_t pid);

/** @brief Retrieve the total schedule time of a process
 * @param pid Process ID
 * @return Scheduled time in seconds
 *
 * Scheduled time means both user and kernel time.
 *
 * proc_enumerate() must have been called first.
 */
intmax_t proc_get_scheduled_time(pid_t pid);

/** @brief Retrieve the total elapsed time of a process
 * @param pid Process ID
 * @return Elapsed time in seconds
 *
 * Elapsed time means the time since the process was started.
 *
 * proc_enumerate() must have been called first.
 */
intmax_t proc_get_elapsed_time(pid_t pid);

/** @brief Retrieve the start time of a process
 * @param pid Process ID
 * @return Start time
 *
 * proc_enumerate() must have been called first.
 */
time_t proc_get_start_time(pid_t pid);

/** @brief Retrieve the kernel flags of a process
 * @param pid Process ID
 * @return Flags word
 *
 * proc_enumerate() must have been called first.
 */
uintmax_t proc_get_flags(pid_t pid);

/** @brief Retrieve the nice level of a process
 * @param pid Process ID
 * @return Nice level
 *
 * proc_enumerate() must have been called first.
 */
intmax_t proc_get_nice(pid_t pid);

/** @brief Retrieve the priority of a process
 * @param pid Process ID
 * @return Priority
 *
 * proc_enumerate() must have been called first.
 */
intmax_t proc_get_priority(pid_t pid);

/** @brief Retrieve the state of a process
 * @param pid Process ID
 * @return State
 *
 * proc_enumerate() must have been called first.
 */
int proc_get_state(pid_t pid);

/** @brief Retrieve recent CPU utilization of a process
 * @param pid Process ID
 * @return CPU utilization
 *
 * proc_enumerate() must have been called first.
 */
int proc_get_pcpu(pid_t pid);

/** @brief Retrieve virtual memory size of a process
 * @param pid Process ID
 * @return Number of bytes mapped
 *
 * Note that (traditionally) this includes @c PROT_NONE maps.
 *
 * proc_enumerate() must have been called first.
 */
uintmax_t proc_get_vsize(pid_t pid);

/** @brief Retrieve resident set size of a process
 * @param pid Process ID
 * @return Number of bytes resident
 *
 * proc_enumerate() must have been called first.
 */
uintmax_t proc_get_rss(pid_t pid);

/** @brief Retrieve current execution address of a process
 * @param pid Process ID
 * @return Current instruction pointer
 *
 * proc_enumerate() must have been called first.
 */
uintmax_t proc_get_insn_pointer(pid_t pid);

/** @brief Retrieve current wait channel of a process
 * @param pid Process ID
 * @return Current wait channel
 *
 * proc_enumerate() must have been called first.
 */
uintmax_t proc_get_wchan(pid_t pid);

/** @brief Retrieve the list of selected processes
 * @param npids Where to store number of processes
 * @return Pointer to first process; owned by caller.
 *
 * Processes are sorted by ascending process ID.
 */
pid_t *proc_get_selected(size_t *npids);

#endif
