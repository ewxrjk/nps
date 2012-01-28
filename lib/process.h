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

/** @brief Identifier for a process or thread */
typedef struct {
  /** @brief Process ID */
  pid_t pid;
  /** @brief Thread ID, or -1 to refer to the whole thread */
  pid_t tid;
} taskident;

/** @brief Enumerate threads */
#define PROC_THREADS 0x0001

/** @brief Enumerate processes */
#define PROC_PROCESSES 0x0002

/** @brief (Re-)enumerate all processes or threads
 * @param last Previous process list or NULL
 * @param flags Flags
 * @return Pointer to process information structure
 *
 * Retrieves a list of processes and possibly threads.
 *
 * @p flags should be a combination of:
 * - @ref PROC_THREADS to retrieve information about threads
 * - @ref PROC_PROCESSES (ignored)
 *
 * Processes are always enumerated.
 */
struct procinfo *proc_enumerate(struct procinfo *last,
                                unsigned flags);

/** @brief Re-run process selection
 *
 * proc_enumerate() does this automatically, but if you change the
 * selection then you must call this function. */
void proc_reselect(struct procinfo *pi);

/** @brief Free process information
 * @param pi Pointer to process information
 */
void proc_free(struct procinfo *pi);

/** @brief Count the number of processes
 * @param pi Pointer to process information
 * @return Number of processes
 */
int proc_count(struct procinfo *pi);

/** @brief Retrieve the time that process information was gathered
 * @param pi Pointer to process information
 * @param ts Timestamp of last proc_enumerate() call
 */
void proc_time(struct procinfo *pi, struct timespec *ts);

/** @brief Retrieve the ID of a process
 * @param pi Pointer to process information
 * @param taskid Process or thread ID
 * @return Process ID
 */
pid_t proc_get_pid(struct procinfo *pi, taskident taskid);

/** @brief Retrieve the thread ID of a task
 * @param pi Pointer to process information
 * @param taskid Process or thread ID
 * @return Thread ID (-1 if this is actually a process)
 */
pid_t proc_get_tid(struct procinfo *pi, taskident taskid);

/** @brief Retrieve the session ID of a process
 * @param pi Pointer to process information
 * @param taskid Process or thread ID
 * @return Session ID or -1
 */
pid_t proc_get_session(struct procinfo *pi, taskident taskid);

/** @brief Retrieve the real user ID of a process
 * @param pi Pointer to process information
 * @param taskid Process or thread ID
 * @return User ID or -1
 */
uid_t proc_get_ruid(struct procinfo *pi, taskident taskid);

/** @brief Retrieve the effective user ID of a process
 * @param pi Pointer to process information
 * @param taskid Process or thread ID
 * @return User ID or -1
 */
uid_t proc_get_euid(struct procinfo *pi, taskident taskid);

/** @brief Retrieve the saved user ID of a process
 * @param pi Pointer to process information
 * @param taskid Process or thread ID
 * @return User ID or -1
 */
uid_t proc_get_suid(struct procinfo *pi, taskident taskid);

/** @brief Retrieve the filesystem user ID of a process
 * @param pi Pointer to process information
 * @param taskid Process or thread ID
 * @return User ID or -1
 */
uid_t proc_get_fsuid(struct procinfo *pi, taskident taskid);

/** @brief Retrieve the real group ID of a process
 * @param pi Pointer to process information
 * @param taskid Process or thread ID
 * @return Group ID or -1
 */
gid_t proc_get_rgid(struct procinfo *pi, taskident taskid);

/** @brief Retrieve the effective group ID of a process
 * @param pi Pointer to process information
 * @param taskid Process or thread ID
 * @return Group ID or -1
 */
gid_t proc_get_egid(struct procinfo *pi, taskident taskid);

/** @brief Retrieve the saved group ID of a process
 * @param pi Pointer to process information
 * @param taskid Process or thread ID
 * @return Group ID or -1
 */
gid_t proc_get_sgid(struct procinfo *pi, taskident taskid);

/** @brief Retrieve the filesystem group ID of a process
 * @param pi Pointer to process information
 * @param taskid Process or thread ID
 * @return Group ID or -1
 */
gid_t proc_get_fsgid(struct procinfo *pi, taskident taskid);

/** @brief Retrieve the parent process ID of a process
 * @param pi Pointer to process information
 * @param taskid Process or thread ID
 * @return Parent process ID
 */
pid_t proc_get_ppid(struct procinfo *pi, taskident taskid);

/** @brief Retrieve the process group ID of a process
 * @param pi Pointer to process information
 * @param taskid Process or thread ID
 * @return Process group ID
 */
pid_t proc_get_pgrp(struct procinfo *pi, taskident taskid);

/** @brief Retrieve foreground process group ID on controllering terminal
 * @param pi Pointer to process information
 * @param taskid Process or thread ID
 * @return Process group ID
 */
pid_t proc_get_tpgid(struct procinfo *pi, taskident taskid);

/** @brief Retrieve the terminal number of a process
 * @param pi Pointer to process information
 * @param taskid Process or thread ID
 * @return Terminal number or -1
 */
int proc_get_tty(struct procinfo *pi, taskident taskid);

/** @brief Retrieve the command name of a process
 * @param pi Pointer to process information
 * @param taskid Process or thread ID
 * @return Command name
 */
const char *proc_get_comm(struct procinfo *pi, taskident taskid);

/** @brief Retrieve the command line of a process
 * @param pi Pointer to process information
 * @param taskid Process or thread ID
 * @return Command line
 */
const char *proc_get_cmdline(struct procinfo *pi, taskident taskid);

/** @brief Retrieve the total schedule time of a process
 * @param pi Pointer to process information
 * @param taskid Process or thread ID
 * @return Scheduled time in seconds
 *
 * Scheduled time means both user and kernel time.
 */
intmax_t proc_get_scheduled_time(struct procinfo *pi, taskident taskid);

/** @brief Retrieve the total elapsed time of a process
 * @param pi Pointer to process information
 * @param taskid Process or thread ID
 * @return Elapsed time in seconds
 *
 * Elapsed time means the time since the process was started.
 */
intmax_t proc_get_elapsed_time(struct procinfo *pi, taskident taskid);

/** @brief Retrieve the start time of a process
 * @param pi Pointer to process information
 * @param taskid Process or thread ID
 * @return Start time
 */
intmax_t proc_get_start_time(struct procinfo *pi, taskident taskid);

/** @brief Retrieve the kernel flags of a process
 * @param pi Pointer to process information
 * @param taskid Process or thread ID
 * @return Flags word
 */
uintmax_t proc_get_flags(struct procinfo *pi, taskident taskid);

/** @brief Retrieve the nice level of a process
 * @param pi Pointer to process information
 * @param taskid Process or thread ID
 * @return Nice level
 */
intmax_t proc_get_nice(struct procinfo *pi, taskident taskid);

/** @brief Retrieve the priority of a process
 * @param pi Pointer to process information
 * @param taskid Process or thread ID
 * @return Priority
 */
intmax_t proc_get_priority(struct procinfo *pi, taskident taskid);

/** @brief Retrieve the state of a process
 * @param pi Pointer to process information
 * @param taskid Process or thread ID
 * @return State
 */
int proc_get_state(struct procinfo *pi, taskident taskid);

/** @brief Retrieve recent CPU utilization of a process
 * @param pi Pointer to process information
 * @param taskid Process or thread ID
 * @return CPU utilization
 */
double proc_get_pcpu(struct procinfo *pi, taskident taskid);

/** @brief Retrieve virtual memory size of a process
 * @param pi Pointer to process information
 * @param taskid Process or thread ID
 * @return Number of bytes mapped
 *
 * Note that (traditionally) this includes @c PROT_NONE maps.
 */
uintmax_t proc_get_vsize(struct procinfo *pi, taskident taskid);

/** @brief Retrieve peak virtual memory size of a process
 * @param pi Pointer to process information
 * @param taskid Process or thread ID
 * @return Number of bytes mapped
 *
 * Note that (traditionally) this includes @c PROT_NONE maps.
 */
uintmax_t proc_get_peak_vsize(struct procinfo *pi, taskident taskid);

/** @brief Retrieve resident set size of a process
 * @param pi Pointer to process information
 * @param taskid Process or thread ID
 * @return Number of bytes resident
 */
uintmax_t proc_get_rss(struct procinfo *pi, taskident taskid);

/** @brief Retrieve peak resident set size of a process
 * @param pi Pointer to process information
 * @param taskid Process or thread ID
 * @return Number of bytes resident
 */
uintmax_t proc_get_peak_rss(struct procinfo *pi, taskident taskid);

/** @brief Retrieve current execution address of a process
 * @param pi Pointer to process information
 * @param taskid Process or thread ID
 * @return Current instruction pointer
 */
uintmax_t proc_get_insn_pointer(struct procinfo *pi, taskident taskid);

/** @brief Retrieve current wait channel of a process
 * @param pi Pointer to process information
 * @param taskid Process or thread ID
 * @return Current wait channel
 */
uintmax_t proc_get_wchan(struct procinfo *pi, taskident taskid);

/** @brief Retrieve rate at which bytes have been read recently
 * @param pi Pointer to process information
 * @param taskid Process or thread ID
 * @return Read rate
 *
 * This is measured at the read() call, and so includes e.g. tty IO.
 */
double proc_get_rchar(struct procinfo *pi, taskident taskid);

/** @brief Retrieve rate at which bytes have been written recently
 * @param pi Pointer to process information
 * @param taskid Process or thread ID
 * @return Write rate
 *
 * This is measured at the write() call, and so includes e.g. tty IO.
 */
double proc_get_wchar(struct procinfo *pi, taskident taskid);

/** @brief Retrieve rate at which bytes have been read recently
 * @param pi Pointer to process information
 * @param taskid Process or thread ID
 * @return Read rate
 *
 * This is measured at the block IO level.
 */
double proc_get_read_bytes(struct procinfo *pi, taskident taskid);

/** @brief Retrieve rate at which bytes have been written recently
 * @param pi Pointer to process information
 * @param taskid Process or thread ID
 * @return Write rate
 *
 * This is measured at the block IO level.
 */
double proc_get_write_bytes(struct procinfo *pi, taskident taskid);

/** @brief Retrieve rate at which bytes have been read and written recently
 * @param pi Pointer to process information
 * @param taskid Process or thread ID
 * @return Combined read + write rate
 *
 * This is measured at the block IO level.
 */
double proc_get_rw_bytes(struct procinfo *pi, taskident taskid);

/** @brief Retrieve OOM score
 * @param pi Pointer to process information
 * @param taskid Process or thread ID
 * @return OOM score
 */
intmax_t proc_get_oom_score(struct procinfo *pi, taskident taskid);

/** @brief Retrieve major fault rate
 * @param pi Pointer to process information
 * @param taskid Process or thread ID
 * @return Bytes per second subject to major faults
 *
 * Major faults require a page to be loaded from disk.
 */
double proc_get_majflt(struct procinfo *pi, taskident taskid);

/** @brief Retrieve minor fault rate
 * @param pi Pointer to process information
 * @param taskid Process or thread ID
 * @return Bytes per second subject to minor faults
 *
 * Minor faults do not require a page to be loaded from disk.
 */
double proc_get_minflt(struct procinfo *pi, taskident taskid);

/** @brief Retrieve process depth in hierarchy
 * @param pi Pointer to process information
 * @param taskid Process or thread ID
 * @return Depth of process in tree
 */
int proc_get_depth(struct procinfo *pi, taskident taskid);

/** @brief Retrieve PSS
 * @param pi Pointer to process information
 * @param taskid Process or thread ID
 * @return PSS in bytes
 *
 * PSS is the "proportional (resident) set size"; each page's
 * contribution is divided by the number of processes sharing it.
 *
 * See @c fs/proc/task_mmu.c in the Linux kernel.
 */
uintmax_t proc_get_pss(struct procinfo *pi, taskident taskid);

/** @brief Retrieve swap usage
 * @param pi Pointer to process information
 * @param taskid Process or thread ID
 * @return Swap usage in bytes
 */
uintmax_t proc_get_swap(struct procinfo *pi, taskident taskid);

/** @brief Retrieve total memory usage
 * @param pi Pointer to process information
 * @param taskid Process or thread ID
 * @return Memory usage in bytes
 *
 * This is RSS+swap.
 */
uintmax_t proc_get_mem(struct procinfo *pi, taskident taskid);

/** @brief Retrieve total proportional memory usage
 * @param pi Pointer to process information
 * @param taskid Process or thread ID
 * @return Memory usage in bytes
 *
 * This is PSS+swap.
 */
uintmax_t proc_get_pmem(struct procinfo *pi, taskident taskid);

/** @brief Retrieve the number of threads in the process
 * @param pi Pointer to process information
 * @param taskid Process or thread ID
 * @return Number of threads, or -1 if @p taskid is a thread.
 */
int proc_get_num_threads(struct procinfo *pi, taskident taskid);

/** @brief Retrieve the supplementary group IDs of the process
 * @param pi Pointer to process information
 * @param taskid Process or thread ID
 * @param countp Where to store number of groups
 * @return Pointer to list of group IDs
 *
 * The return value points into the internals of @p pi, so it need not
 * be freed by the caller, and is invalidated by proc_free().
 */
const gid_t *proc_get_supgids(struct procinfo *pi, taskident taskid,
                              size_t *countp);

/** @brief Retrieve the realtime scheduling priority of the process
 * @param pi Pointer to process information
 * @param taskid Process or thread ID
 * @return Priority
 */
uintmax_t proc_get_rtprio(struct procinfo *pi, taskident taskid);

/** @brief Retrieve the scheduling policy of the process
 * @param pi Pointer to process information
 * @param taskid Process or thread ID
 * @return Priority
 */
int proc_get_sched_policy(struct procinfo *pi, taskident taskid);

/** @brief Retrieve the set of pending signals
 * @param pi Pointer to process information
 * @param taskid Process or thread ID
 * @param signals Where to store signal set
 */
void proc_get_sig_pending(struct procinfo *pi, taskident taskid,
                          sigset_t *signals);

/** @brief Retrieve the set of blocked signals
 * @param pi Pointer to process information
 * @param taskid Process or thread ID
 * @param signals Where to store signal set
 */
void proc_get_sig_blocked(struct procinfo *pi, taskident taskid,
                          sigset_t *signals);

/** @brief Retrieve the set of ignored signals
 * @param pi Pointer to process information
 * @param taskid Process or thread ID
 * @param signals Where to store signal set
 */
void proc_get_sig_ignored(struct procinfo *pi, taskident taskid,
                          sigset_t *signals);

/** @brief Retrieve the set of caught signals
 * @param pi Pointer to process information
 * @param taskid Process or thread ID
 * @param signals Where to store signal set
 */
void proc_get_sig_caught(struct procinfo *pi, taskident taskid,
                         sigset_t *signals);

/** @brief Retrieve stack size
 * @param pi Pointer to process information
 * @param taskid Process or thread ID
 * @return Number of bytes resident
 */
uintmax_t proc_get_stack(struct procinfo *pi, taskident taskid);

/** @brief Retrieve locked memory
 * @param pi Pointer to process information
 * @param taskid Process or thread ID
 * @return Number of bytes resident
 */
uintmax_t proc_get_locked(struct procinfo *pi, taskident taskid);

/** @brief Retrieve pinned memory
 * @param pi Pointer to process information
 * @param taskid Process or thread ID
 * @return Number of bytes resident
 */
uintmax_t proc_get_pinned(struct procinfo *pi, taskident taskid);

/** @brief Retrieve page table entry memory usage
 * @param pi Pointer to process information
 * @param taskid Process or thread ID
 * @return Number of bytes resident
 */
uintmax_t proc_get_pte(struct procinfo *pi, taskident taskid);

// ----------------------------------------------------------------------------

/** @brief Return true if @p a is an ancestor of, or equal to, @p b
 * @param pi Pointer to process information
 * @param a Process or thread ID
 * @param b Process or thread ID
 * @return Nonzero iff @p a is an ancestor of, or equal to, @p b
 *
 * Ancestry applies to whole processes; thread IDs are disregarded.
 */
int proc_is_ancestor(struct procinfo *pi, taskident a, taskident b);

/** @brief Retrieve the list of selected tasks
 * @param pi Pointer to task information
 * @param ntasks Where to store number of tasks
 * @param flags Flags
 * @return Pointer to first task; owned by caller.
 *
 * @p flags should be a combination of:
 * - @ref PROC_PROCESSES to include processes
 * - @ref PROC_THREADS to include threads
 */
taskident *proc_get_selected(struct procinfo *pi, size_t *ntasks,
                             unsigned flags);

/** @brief Retrieve list of all tasks
 * @param pi Pointer to task information
 * @param ntasks Where to store number of tasks
 * @param flags Flags
 * @return Pointer to first task; owned by caller.
 *
 * @p flags should be a combination of:
 * - @ref PROC_PROCESSES to include processes
 * - @ref PROC_THREADS to include threads
 */
taskident *proc_get_all(struct procinfo *pi, size_t *ntasks,
                        unsigned flags);
#endif
