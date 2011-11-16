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

/** @brief Opaque process information structure */
struct procinfo;

/** @brief (Re-)enumerate all processes
 * @param last Previous process list or NULL
 * @return Pointer to process information structure
 *
 * Retrieves a list of processes.
 */
struct procinfo *proc_enumerate(struct procinfo *last);

/** @brief Free process information
 * @param pi Pointer to process information
 */
void proc_free(struct procinfo *pi);

/** @brief Count the number of processes
 * @param pi Pointer to process information
 * @return Number of processes
 */
int proc_count(struct procinfo *pi);

/** @brief Retrieve the ID of a process
 * @param pi Pointer to process information
 * @param pid Process ID
 * @return Process ID
 */
pid_t proc_get_pid(struct procinfo *pi, pid_t pid);

/** @brief Retrieve the session ID of a process
 * @param pi Pointer to process information
 * @param pid Process ID
 * @return Session ID or -1
 */
pid_t proc_get_session(struct procinfo *pi, pid_t pid);

/** @brief Retrieve the real user ID of a process
 * @param pi Pointer to process information
 * @param pid Process ID
 * @return User ID or -1
 */
uid_t proc_get_ruid(struct procinfo *pi, pid_t pid);

/** @brief Retrieve the effective user ID of a process
 * @param pi Pointer to process information
 * @param pid Process ID
 * @return User ID or -1
 */
uid_t proc_get_euid(struct procinfo *pi, pid_t pid);

/** @brief Retrieve the real group ID of a process
 * @param pi Pointer to process information
 * @param pid Process ID
 * @return Group ID or -1
 */
gid_t proc_get_rgid(struct procinfo *pi, pid_t pid);

/** @brief Retrieve the effective group ID of a process
 * @param pi Pointer to process information
 * @param pid Process ID
 * @return Group ID or -1
 */
gid_t proc_get_egid(struct procinfo *pi, pid_t pid);

/** @brief Retrieve the parent process ID of a process
 * @param pi Pointer to process information
 * @param pid Process ID
 * @return Parent process ID
 */
pid_t proc_get_ppid(struct procinfo *pi, pid_t pid);

/** @brief Retrieve the process group ID of a process
 * @param pi Pointer to process information
 * @param pid Process ID
 * @return Process group ID
 */
pid_t proc_get_pgid(struct procinfo *pi, pid_t pid);

/** @brief Retrieve the terminal number of a process
 * @param pi Pointer to process information
 * @param pid Process ID
 * @return Terminal number or -1
 */
int proc_get_tty(struct procinfo *pi, pid_t pid);

/** @brief Retrieve the command name of a process
 * @param pi Pointer to process information
 * @param pid Process ID
 * @return Command name
 */
const char *proc_get_comm(struct procinfo *pi, pid_t pid);

/** @brief Retrieve the command line of a process
 * @param pi Pointer to process information
 * @param pid Process ID
 * @return Command line
 */
const char *proc_get_cmdline(struct procinfo *pi, pid_t pid);

/** @brief Retrieve the total schedule time of a process
 * @param pi Pointer to process information
 * @param pid Process ID
 * @return Scheduled time in seconds
 *
 * Scheduled time means both user and kernel time.
 */
intmax_t proc_get_scheduled_time(struct procinfo *pi, pid_t pid);

/** @brief Retrieve the total elapsed time of a process
 * @param pi Pointer to process information
 * @param pid Process ID
 * @return Elapsed time in seconds
 *
 * Elapsed time means the time since the process was started.
 */
intmax_t proc_get_elapsed_time(struct procinfo *pi, pid_t pid);

/** @brief Retrieve the start time of a process
 * @param pi Pointer to process information
 * @param pid Process ID
 * @return Start time
 */
time_t proc_get_start_time(struct procinfo *pi, pid_t pid);

/** @brief Retrieve the kernel flags of a process
 * @param pi Pointer to process information
 * @param pid Process ID
 * @return Flags word
 */
uintmax_t proc_get_flags(struct procinfo *pi, pid_t pid);

/** @brief Retrieve the nice level of a process
 * @param pi Pointer to process information
 * @param pid Process ID
 * @return Nice level
 */
intmax_t proc_get_nice(struct procinfo *pi, pid_t pid);

/** @brief Retrieve the priority of a process
 * @param pi Pointer to process information
 * @param pid Process ID
 * @return Priority
 */
intmax_t proc_get_priority(struct procinfo *pi, pid_t pid);

/** @brief Retrieve the state of a process
 * @param pi Pointer to process information
 * @param pid Process ID
 * @return State
 */
int proc_get_state(struct procinfo *pi, pid_t pid);

/** @brief Retrieve recent CPU utilization of a process
 * @param pi Pointer to process information
 * @param pid Process ID
 * @return CPU utilization
 */
double proc_get_pcpu(struct procinfo *pi, pid_t pid);

/** @brief Retrieve virtual memory size of a process
 * @param pi Pointer to process information
 * @param pid Process ID
 * @return Number of bytes mapped
 *
 * Note that (traditionally) this includes @c PROT_NONE maps.
 */
uintmax_t proc_get_vsize(struct procinfo *pi, pid_t pid);

/** @brief Retrieve resident set size of a process
 * @param pi Pointer to process information
 * @param pid Process ID
 * @return Number of bytes resident
 */
uintmax_t proc_get_rss(struct procinfo *pi, pid_t pid);

/** @brief Retrieve current execution address of a process
 * @param pi Pointer to process information
 * @param pid Process ID
 * @return Current instruction pointer
 */
uintmax_t proc_get_insn_pointer(struct procinfo *pi, pid_t pid);

/** @brief Retrieve current wait channel of a process
 * @param pi Pointer to process information
 * @param pid Process ID
 * @return Current wait channel
 */
uintmax_t proc_get_wchan(struct procinfo *pi, pid_t pid);

/** @brief Retrieve rate at which bytes have been read recently
 * @param pi Pointer to process information
 * @param pid Process ID
 * @return Read rate
 *
 * This is measured at the read() call, and so includes e.g. tty IO.
 */
double proc_get_rchar(struct procinfo *pi, pid_t pid);

/** @brief Retrieve rate at which bytes have been written recently
 * @param pi Pointer to process information
 * @param pid Process ID
 * @return Write rate
 *
 * This is measured at the write() call, and so includes e.g. tty IO.
 */
double proc_get_wchar(struct procinfo *pi, pid_t pid);

/** @brief Retrieve rate at which bytes have been read recently
 * @param pi Pointer to process information
 * @param pid Process ID
 * @return Read rate
 *
 * This is measured at the block IO level.
 */
double proc_get_read_bytes(struct procinfo *pi, pid_t pid);

/** @brief Retrieve rate at which bytes have been written recently
 * @param pi Pointer to process information
 * @param pid Process ID
 * @return Write rate
 *
 * This is measured at the block IO level.
 */
double proc_get_write_bytes(struct procinfo *pi, pid_t pid);

/** @brief Retrieve rate at which bytes have been read and written recently
 * @param pi Pointer to process information
 * @param pid Process ID
 * @return Combined read + write rate
 *
 * This is measured at the block IO level.
 */
double proc_get_rw_bytes(struct procinfo *pi, pid_t pid);

/** @brief Retrieve the list of selected processes
 * @param pi Pointer to process information
 * @param npids Where to store number of processes
 * @return Pointer to first process; owned by caller.
 *
 * Processes are sorted by ascending process ID.
 */
pid_t *proc_get_selected(struct procinfo *pi, size_t *npids);

#endif
