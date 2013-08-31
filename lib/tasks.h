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
#ifndef TASKS_H
#define TASKS_H

/** @file tasks.h
 * @brief Retrieving task information
 */

#include <sys/types.h>
#include <inttypes.h>

/** @brief Opaque task information structure */
struct taskinfo;

/** @brief Path to /proc */
extern const char *proc;

/** @brief PID to pretend to */
extern pid_t selfpid;

/** @brief Identifier for a process or thread */
typedef struct {
  /** @brief Process ID */
  pid_t pid;
  /** @brief Thread ID, or -1 to refer to the whole thread */
  pid_t tid;
} taskident;

/** @brief Enumerate threads */
#define TASK_THREADS 0x0001

/** @brief Enumerate processes */
#define TASK_PROCESSES 0x0002

/** @brief (Re-)enumerate all processes or threads
 * @param last Previous task list or NULL
 * @param flags Flags
 * @return Pointer to task information structure
 *
 * Retrieves a list of processes and possibly threads.
 *
 * @p flags should be a combination of:
 * - @ref TASK_THREADS to retrieve information about threads
 * - @ref TASK_PROCESSES (ignored)
 *
 * Processes are always enumerated.
 */
struct taskinfo *task_enumerate(struct taskinfo *last,
                                unsigned flags);

/** @brief Re-run task selection
 *
 * task_enumerate() does this automatically, but if you change the
 * selection then you must call this function. */
void task_reselect(struct taskinfo *ti);

/** @brief Free task information
 * @param ti Pointer to task information
 */
void task_free(struct taskinfo *ti);

/** @brief Count the number of processes
 * @param ti Pointer to task information
 * @return Number of processes
 */
int task_processes(struct taskinfo *ti);

/** @brief Count the number of threads
 * @param ti Pointer to task information
 * @return Number of threads
 */
int task_threads(struct taskinfo *ti);

/** @brief Retrieve the time that process information was gathered
 * @param ti Pointer to task information
 * @param ts Timestamp of last task_enumerate() call
 */
void task_time(struct taskinfo *ti, struct timespec *ts);

/** @brief Retrieve the ID of a process
 * @param ti Pointer to task information
 * @param taskid Process or thread ID
 * @return Process ID
 */
pid_t task_get_pid(struct taskinfo *ti, taskident taskid);

/** @brief Retrieve the thread ID of a task
 * @param ti Pointer to task information
 * @param taskid Process or thread ID
 * @return Thread ID (-1 if this is actually a process)
 */
pid_t task_get_tid(struct taskinfo *ti, taskident taskid);

/** @brief Retrieve the session ID of a process
 * @param ti Pointer to task information
 * @param taskid Process or thread ID
 * @return Session ID or -1
 */
pid_t task_get_session(struct taskinfo *ti, taskident taskid);

/** @brief Retrieve the real user ID of a process
 * @param ti Pointer to task information
 * @param taskid Process or thread ID
 * @return User ID or -1
 */
uid_t task_get_ruid(struct taskinfo *ti, taskident taskid);

/** @brief Retrieve the effective user ID of a process
 * @param ti Pointer to task information
 * @param taskid Process or thread ID
 * @return User ID or -1
 */
uid_t task_get_euid(struct taskinfo *ti, taskident taskid);

/** @brief Retrieve the saved user ID of a process
 * @param ti Pointer to task information
 * @param taskid Process or thread ID
 * @return User ID or -1
 */
uid_t task_get_suid(struct taskinfo *ti, taskident taskid);

/** @brief Retrieve the filesystem user ID of a process
 * @param ti Pointer to task information
 * @param taskid Process or thread ID
 * @return User ID or -1
 */
uid_t task_get_fsuid(struct taskinfo *ti, taskident taskid);

/** @brief Retrieve the real group ID of a process
 * @param ti Pointer to task information
 * @param taskid Process or thread ID
 * @return Group ID or -1
 */
gid_t task_get_rgid(struct taskinfo *ti, taskident taskid);

/** @brief Retrieve the effective group ID of a process
 * @param ti Pointer to task information
 * @param taskid Process or thread ID
 * @return Group ID or -1
 */
gid_t task_get_egid(struct taskinfo *ti, taskident taskid);

/** @brief Retrieve the saved group ID of a process
 * @param ti Pointer to task information
 * @param taskid Process or thread ID
 * @return Group ID or -1
 */
gid_t task_get_sgid(struct taskinfo *ti, taskident taskid);

/** @brief Retrieve the filesystem group ID of a process
 * @param ti Pointer to task information
 * @param taskid Process or thread ID
 * @return Group ID or -1
 */
gid_t task_get_fsgid(struct taskinfo *ti, taskident taskid);

/** @brief Retrieve the parent process ID of a process
 * @param ti Pointer to task information
 * @param taskid Process or thread ID
 * @return Parent process ID
 */
pid_t task_get_ppid(struct taskinfo *ti, taskident taskid);

/** @brief Retrieve the process group ID of a process
 * @param ti Pointer to task information
 * @param taskid Process or thread ID
 * @return Process group ID
 */
pid_t task_get_pgrp(struct taskinfo *ti, taskident taskid);

/** @brief Retrieve foreground process group ID on controllering terminal
 * @param ti Pointer to task information
 * @param taskid Process or thread ID
 * @return Process group ID
 */
pid_t task_get_tpgid(struct taskinfo *ti, taskident taskid);

/** @brief Retrieve the terminal number of a process
 * @param ti Pointer to task information
 * @param taskid Process or thread ID
 * @return Terminal number or -1
 */
int task_get_tty(struct taskinfo *ti, taskident taskid);

/** @brief Retrieve the command name of a process
 * @param ti Pointer to task information
 * @param taskid Process or thread ID
 * @return Command name
 */
const char *task_get_comm(struct taskinfo *ti, taskident taskid);

/** @brief Retrieve the command line of a process
 * @param ti Pointer to task information
 * @param taskid Process or thread ID
 * @return Command line
 */
const char *task_get_cmdline(struct taskinfo *ti, taskident taskid);

/** @brief Retrieve the total schedule time of a process
 * @param ti Pointer to task information
 * @param taskid Process or thread ID
 * @return Scheduled time in seconds
 *
 * Scheduled time means both user and kernel time.
 */
intmax_t task_get_scheduled_time(struct taskinfo *ti, taskident taskid);

/** @brief Retrieve the total elapsed time of a process
 * @param ti Pointer to task information
 * @param taskid Process or thread ID
 * @return Elapsed time in seconds
 *
 * Elapsed time means the time since the process was started.
 */
intmax_t task_get_elapsed_time(struct taskinfo *ti, taskident taskid);

/** @brief Retrieve the start time of a process
 * @param ti Pointer to task information
 * @param taskid Process or thread ID
 * @return Start time
 */
intmax_t task_get_start_time(struct taskinfo *ti, taskident taskid);

/** @brief Retrieve the kernel flags of a process
 * @param ti Pointer to task information
 * @param taskid Process or thread ID
 * @return Flags word
 */
uintmax_t task_get_flags(struct taskinfo *ti, taskident taskid);

/** @brief Retrieve the nice level of a process
 * @param ti Pointer to task information
 * @param taskid Process or thread ID
 * @return Nice level
 */
intmax_t task_get_nice(struct taskinfo *ti, taskident taskid);

/** @brief Retrieve the priority of a process
 * @param ti Pointer to task information
 * @param taskid Process or thread ID
 * @return Priority
 */
intmax_t task_get_priority(struct taskinfo *ti, taskident taskid);

/** @brief Retrieve the state of a process
 * @param ti Pointer to task information
 * @param taskid Process or thread ID
 * @return State
 */
int task_get_state(struct taskinfo *ti, taskident taskid);

/** @brief Retrieve recent CPU utilization of a process
 * @param ti Pointer to task information
 * @param taskid Process or thread ID
 * @return CPU utilization
 */
double task_get_pcpu(struct taskinfo *ti, taskident taskid);

/** @brief Retrieve virtual memory size of a process
 * @param ti Pointer to task information
 * @param taskid Process or thread ID
 * @return Number of bytes mapped
 *
 * Note that (traditionally) this includes @c PROT_NONE maps.
 */
uintmax_t task_get_vsize(struct taskinfo *ti, taskident taskid);

/** @brief Retrieve peak virtual memory size of a process
 * @param ti Pointer to task information
 * @param taskid Process or thread ID
 * @return Number of bytes mapped
 *
 * Note that (traditionally) this includes @c PROT_NONE maps.
 */
uintmax_t task_get_peak_vsize(struct taskinfo *ti, taskident taskid);

/** @brief Retrieve resident set size of a process
 * @param ti Pointer to task information
 * @param taskid Process or thread ID
 * @return Number of bytes resident
 */
uintmax_t task_get_rss(struct taskinfo *ti, taskident taskid);

/** @brief Retrieve peak resident set size of a process
 * @param ti Pointer to task information
 * @param taskid Process or thread ID
 * @return Number of bytes resident
 */
uintmax_t task_get_peak_rss(struct taskinfo *ti, taskident taskid);

/** @brief Retrieve current execution address of a process
 * @param ti Pointer to task information
 * @param taskid Process or thread ID
 * @return Current instruction pointer
 */
uintmax_t task_get_insn_pointer(struct taskinfo *ti, taskident taskid);

/** @brief Retrieve current wait channel of a process
 * @param ti Pointer to task information
 * @param taskid Process or thread ID
 * @return Current wait channel
 */
uintmax_t task_get_wchan(struct taskinfo *ti, taskident taskid);

/** @brief Retrieve rate at which bytes have been read recently
 * @param ti Pointer to task information
 * @param taskid Process or thread ID
 * @return Read rate
 *
 * This is measured at the read() call, and so includes e.g. tty IO.
 */
double task_get_rchar(struct taskinfo *ti, taskident taskid);

/** @brief Retrieve rate at which bytes have been written recently
 * @param ti Pointer to task information
 * @param taskid Process or thread ID
 * @return Write rate
 *
 * This is measured at the write() call, and so includes e.g. tty IO.
 */
double task_get_wchar(struct taskinfo *ti, taskident taskid);

/** @brief Retrieve rate at which bytes have been read recently
 * @param ti Pointer to task information
 * @param taskid Process or thread ID
 * @return Read rate
 *
 * This is measured at the block IO level.
 */
double task_get_read_bytes(struct taskinfo *ti, taskident taskid);

/** @brief Retrieve rate at which bytes have been written recently
 * @param ti Pointer to task information
 * @param taskid Process or thread ID
 * @return Write rate
 *
 * This is measured at the block IO level.
 */
double task_get_write_bytes(struct taskinfo *ti, taskident taskid);

/** @brief Retrieve rate at which bytes have been read and written recently
 * @param ti Pointer to task information
 * @param taskid Process or thread ID
 * @return Combined read + write rate
 *
 * This is measured at the block IO level.
 */
double task_get_rw_bytes(struct taskinfo *ti, taskident taskid);

/** @brief Retrieve OOM score
 * @param ti Pointer to task information
 * @param taskid Process or thread ID
 * @return OOM score
 */
intmax_t task_get_oom_score(struct taskinfo *ti, taskident taskid);

/** @brief Retrieve major fault rate
 * @param ti Pointer to task information
 * @param taskid Process or thread ID
 * @return Bytes per second subject to major faults
 *
 * Major faults require a page to be loaded from disk.
 */
double task_get_majflt(struct taskinfo *ti, taskident taskid);

/** @brief Retrieve minor fault rate
 * @param ti Pointer to task information
 * @param taskid Process or thread ID
 * @return Bytes per second subject to minor faults
 *
 * Minor faults do not require a page to be loaded from disk.
 */
double task_get_minflt(struct taskinfo *ti, taskident taskid);

/** @brief Retrieve process depth in hierarchy
 * @param ti Pointer to task information
 * @param taskid Process or thread ID
 * @return Depth of process in tree
 */
int task_get_depth(struct taskinfo *ti, taskident taskid);

/** @brief Retrieve PSS
 * @param ti Pointer to task information
 * @param taskid Process or thread ID
 * @return PSS in bytes
 *
 * PSS is the "proportional (resident) set size"; each page's
 * contribution is divided by the number of processes sharing it.
 *
 * See @c fs/proc/task_mmu.c in the Linux kernel.
 */
uintmax_t task_get_pss(struct taskinfo *ti, taskident taskid);

/** @brief Retrieve swap usage
 * @param ti Pointer to task information
 * @param taskid Process or thread ID
 * @return Swap usage in bytes
 */
uintmax_t task_get_swap(struct taskinfo *ti, taskident taskid);

/** @brief Retrieve total memory usage
 * @param ti Pointer to task information
 * @param taskid Process or thread ID
 * @return Memory usage in bytes
 *
 * This is RSS+swap.
 */
uintmax_t task_get_mem(struct taskinfo *ti, taskident taskid);

/** @brief Retrieve total proportional memory usage
 * @param ti Pointer to task information
 * @param taskid Process or thread ID
 * @return Memory usage in bytes
 *
 * This is PSS+swap.
 */
uintmax_t task_get_pmem(struct taskinfo *ti, taskident taskid);

/** @brief Retrieve the number of threads in the process
 * @param ti Pointer to task information
 * @param taskid Process or thread ID
 * @return Number of threads, or -1 if @p taskid is a thread.
 */
int task_get_num_threads(struct taskinfo *ti, taskident taskid);

/** @brief Retrieve the supplementary group IDs of the process
 * @param ti Pointer to task information
 * @param taskid Process or thread ID
 * @param countp Where to store number of groups
 * @return Pointer to list of group IDs
 *
 * The return value points into the internals of @p ti, so it need not
 * be freed by the caller, and is invalidated by task_free().
 */
const gid_t *task_get_supgids(struct taskinfo *ti, taskident taskid,
                              size_t *countp);

/** @brief Retrieve the realtime scheduling priority of the process
 * @param ti Pointer to task information
 * @param taskid Process or thread ID
 * @return Priority
 */
uintmax_t task_get_rtprio(struct taskinfo *ti, taskident taskid);

/** @brief Retrieve the scheduling policy of the process
 * @param ti Pointer to task information
 * @param taskid Process or thread ID
 * @return Priority
 */
int task_get_sched_policy(struct taskinfo *ti, taskident taskid);

/** @brief Retrieve the set of pending signals
 * @param ti Pointer to task information
 * @param taskid Process or thread ID
 * @param signals Where to store signal set
 */
void task_get_sig_pending(struct taskinfo *ti, taskident taskid,
                          sigset_t *signals);

/** @brief Retrieve the set of blocked signals
 * @param ti Pointer to task information
 * @param taskid Process or thread ID
 * @param signals Where to store signal set
 */
void task_get_sig_blocked(struct taskinfo *ti, taskident taskid,
                          sigset_t *signals);

/** @brief Retrieve the set of ignored signals
 * @param ti Pointer to task information
 * @param taskid Process or thread ID
 * @param signals Where to store signal set
 */
void task_get_sig_ignored(struct taskinfo *ti, taskident taskid,
                          sigset_t *signals);

/** @brief Retrieve the set of caught signals
 * @param ti Pointer to task information
 * @param taskid Process or thread ID
 * @param signals Where to store signal set
 */
void task_get_sig_caught(struct taskinfo *ti, taskident taskid,
                         sigset_t *signals);

/** @brief Retrieve stack size
 * @param ti Pointer to task information
 * @param taskid Process or thread ID
 * @return Number of bytes resident
 */
uintmax_t task_get_stack(struct taskinfo *ti, taskident taskid);

/** @brief Retrieve locked memory
 * @param ti Pointer to task information
 * @param taskid Process or thread ID
 * @return Number of bytes resident
 */
uintmax_t task_get_locked(struct taskinfo *ti, taskident taskid);

/** @brief Retrieve pinned memory
 * @param ti Pointer to task information
 * @param taskid Process or thread ID
 * @return Number of bytes resident
 */
uintmax_t task_get_pinned(struct taskinfo *ti, taskident taskid);

/** @brief Retrieve page table entry memory usage
 * @param ti Pointer to task information
 * @param taskid Process or thread ID
 * @return Number of bytes resident
 */
uintmax_t task_get_pte(struct taskinfo *ti, taskident taskid);

// ----------------------------------------------------------------------------

/** @brief Return true if @p a is an ancestor of, or equal to, @p b
 * @param ti Pointer to task information
 * @param a Process or thread ID
 * @param b Process or thread ID
 * @return Nonzero iff @p a is an ancestor of, or equal to, @p b
 *
 * Ancestry applies to whole processes; thread IDs are disregarded.
 */
int task_is_ancestor(struct taskinfo *ti, taskident a, taskident b);

/** @brief Retrieve the list of selected tasks
 * @param ti Pointer to task information
 * @param ntasks Where to store number of tasks
 * @param flags Flags
 * @return Pointer to first task; owned by caller.
 *
 * @p flags should be a combination of:
 * - @ref TASK_PROCESSES to include processes
 * - @ref TASK_THREADS to include threads
 */
taskident *task_get_selected(struct taskinfo *ti, size_t *ntasks,
                             unsigned flags);

/** @brief Retrieve list of all tasks
 * @param ti Pointer to task information
 * @param ntasks Where to store number of tasks
 * @param flags Flags
 * @return Pointer to first task; owned by caller.
 *
 * @p flags should be a combination of:
 * - @ref TASK_PROCESSES to include processes
 * - @ref TASK_THREADS to include threads
 */
taskident *task_get_all(struct taskinfo *ti, size_t *ntasks,
                        unsigned flags);

/** @brief Return the current process's controlling terminal
 * @param ti Pointer to task information
 * @return Terminal number or -1
 */
int self_tty(struct taskinfo *ti);

#endif
