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
#include <config.h>
#include "format.h"
#include "buffer.h"
#include "process.h"
#include "utils.h"
#include <string.h>
#include <inttypes.h>
#include <stdio.h>
#include <pwd.h>
#include <grp.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>

// ----------------------------------------------------------------------------

struct propinfo;
struct column;

typedef void formatfn(const struct column *col, struct buffer *b,
                      size_t columnsize,
                      struct procinfo *pi, taskident taskid,
                      unsigned flags);

typedef int comparefn(const struct propinfo *prop, struct procinfo *pi, 
                      taskident a, taskident b);

struct propinfo {
  const char *name;
  const char *heading;
  const char *description;
  formatfn *format;
  comparefn *compare;
  union {
    int (*fetch_int)(struct procinfo *, taskident);
    intmax_t (*fetch_intmax)(struct procinfo *, taskident);
    uintmax_t (*fetch_uintmax)(struct procinfo *, taskident);
    pid_t (*fetch_pid)(struct procinfo *, taskident);
    uid_t (*fetch_uid)(struct procinfo *, taskident);
    gid_t (*fetch_gid)(struct procinfo *, taskident);
    double (*fetch_double)(struct procinfo *, taskident);
    const char *(*fetch_string)(struct procinfo *, taskident);
  } fetch;
};

#define ANTIWOBBLE 16           /* size of anti-wobble ring buffer */

struct column {
  const struct propinfo *prop;
  size_t reqwidth;
  size_t width;
  size_t oldwidths[ANTIWOBBLE]; /* ring buffer */
  size_t oldwidthind;           /* next slot to write in oldwidths */
  char *heading;
  char *arg;
};

static size_t ncolumns;
static struct column *columns;

struct order {
  const struct propinfo *prop;
  int sign;
};

static size_t norders;
static struct order *orders;

// ----------------------------------------------------------------------------

static void format_integer(intmax_t im, struct buffer *b, int base) {
  buffer_printf(b, (base == 'o' ? "%jo" :
                    base == 'x' ? "%jx" :
                    base == 'X' ? "%jX" :
                    "%jd"), im);
}

static void format_addr(uintmax_t im, struct buffer *b) {
  if(im > 0xFFFFFFFFFFFFLL)
    buffer_printf(b, "%016jx", im);
  else if(im > 0xFFFFFFFF)
    buffer_printf(b, "%012jx", im);
  else
    buffer_printf(b, "%08jx", im);
}

static void format_usergroup(intmax_t id, struct buffer *b, size_t columnsize,
                             const char *name) {
  if(name && strlen(name) <= columnsize)
    buffer_append(b, name);
  else
    format_integer(id, b,'d');
}

static void format_user(uid_t uid, struct buffer *b, size_t columnsize) {
  struct passwd *pw = getpwuid(uid);
  format_usergroup(uid, b, columnsize, pw ? pw->pw_name : NULL);
}

static void format_group(gid_t gid, struct buffer *b, size_t columnsize) {
  struct group *gr = getgrgid(gid);
  format_usergroup(gid, b, columnsize, gr ? gr->gr_name : NULL);
}

static void format_interval(long seconds, struct buffer *b,
                            int always_hours, size_t columnsize,
                            const char *format,
                            unsigned flags) {
  size_t startpos;
  if(flags & FORMAT_RAW) {
    format_integer(seconds, b, 'd');
    return;
  }
  if(format)
    strfelapsed(b, format, seconds);
  else {
    startpos = b->pos;
    if(always_hours)
      strfelapsed(b, "%?+-d%02H:%02M:%02S", seconds);
    else
      strfelapsed(b, "%?+-d%02?+:H%02M:%02S", seconds);
    /* If a column size was specified and we're too big, try more
     * compact forms. */
    if(b->pos - startpos > columnsize) {
      b->pos = startpos;        /* wind back */
      if(seconds >= 86400)
        strfelapsed(b, "%dd%02H", seconds);
      else if(seconds >= 3600)
        strfelapsed(b, "%02hh%02M", seconds);
      else
        strfelapsed(b, "%02mm%02S", seconds);
    }
  }
}

static void format_time(time_t when, struct buffer *b, size_t columnsize,
                        const char *format, unsigned flags) {
  time_t now;
  struct tm when_tm, now_tm;

  if(flags & FORMAT_RAW) {
    format_integer(when, b, 'd');
    return;
  }
  time(&now);
  localtime_r(&when, &when_tm);
  localtime_r(&now, &now_tm);
  if(format)
    buffer_strftime(b, format, &when_tm);
  else if(columnsize != SIZE_MAX && columnsize >= 19)
    buffer_strftime(b, "%Y-%m-%dT%H:%M:%S", &when_tm);
  else if(now_tm.tm_year == when_tm.tm_year
          && now_tm.tm_mon == when_tm.tm_mon
          && now_tm.tm_mday == when_tm.tm_mday) {
    if(columnsize < 8)
      buffer_strftime(b, "%H:%M", &when_tm);
    else
      buffer_strftime(b, "%H:%M:%S", &when_tm);
  } else if(columnsize < 10 && now_tm.tm_year == when_tm.tm_year)
    buffer_strftime(b, "%m-%d", &when_tm);
  else
    buffer_strftime(b, "%Y-%m-%d", &when_tm);
}

// ----------------------------------------------------------------------------

/* generic properties */

static void property_decimal(const struct column *col, struct buffer *b,
                             size_t attribute((unused)) columnsize,
                             struct procinfo *pi, taskident task,
                             unsigned attribute((unused)) flags) {
  return format_integer(col->prop->fetch.fetch_intmax(pi, task), b, 'd');
}

static void property_uoctal(const struct column *col, struct buffer *b,
                            size_t attribute((unused)) columnsize,
                            struct procinfo *pi, taskident task,
                            unsigned attribute((unused)) flags) {
  return format_integer(col->prop->fetch.fetch_uintmax(pi, task), b,
                        col->arg ? *col->arg : 'o');
}

static void property_pid(const struct column *col, struct buffer *b,
                         size_t attribute((unused)) columnsize, 
                         struct procinfo *pi, taskident task,
                         unsigned attribute((unused)) flags) {
  pid_t pid = col->prop->fetch.fetch_pid(pi, task);
  if(pid >= 0)
    format_integer(pid, b, 'd');
  else
    buffer_putc(b, '-');
}

static void property_num_threads(const struct column *col, struct buffer *b,
                                 size_t attribute((unused)) columnsize, 
                                 struct procinfo *pi, taskident task,
                                 unsigned attribute((unused)) flags) {
  int count = col->prop->fetch.fetch_int(pi, task);
  if(count >= 0)
    format_integer(count, b, 'd');
  else
    buffer_putc(b, '-');
}

static void property_uid(const struct column *col, struct buffer *b,
                         size_t attribute((unused)) columnsize,
                         struct procinfo *pi, taskident task,
                         unsigned attribute((unused)) flags) {
  return format_integer(col->prop->fetch.fetch_uid(pi, task), b, 'd');
}

static void property_user(const struct column *col, struct buffer *b,
                          size_t columnsize, struct procinfo *pi, taskident task,
                          unsigned attribute((unused)) flags) {
  return format_user(col->prop->fetch.fetch_uid(pi, task), b, columnsize);
}

static void property_gid(const struct column *col, struct buffer *b,
                         size_t attribute((unused)) columnsize,
                         struct procinfo *pi, taskident task,
                         unsigned attribute((unused)) flags) {
  return format_integer(col->prop->fetch.fetch_gid(pi, task), b,'d');
}

static void property_group(const struct column *col, struct buffer *b,
                           size_t columnsize, struct procinfo *pi, taskident task,
                           unsigned attribute((unused)) flags) {
  return format_group(col->prop->fetch.fetch_gid(pi, task), b, columnsize);
}

static void property_char(const struct column *col, struct buffer *b,
                          size_t attribute((unused)) columnsize,
                          struct procinfo *pi, taskident task,
                          unsigned attribute((unused)) flags) {
  buffer_putc(b, col->prop->fetch.fetch_int(pi, task));
}

/* time properties */

static void property_time(const struct column *col, struct buffer *b,
                          size_t columnsize,
                          struct procinfo *pi, taskident task,
                          unsigned flags) {
  /* time wants [dd-]hh:mm:ss */
  return format_interval(col->prop->fetch.fetch_intmax(pi, task), b, 1, columnsize,
                         col->arg, flags);
}

static void property_etime(const struct column *col, struct buffer *b,
                           size_t attribute((unused)) columnsize,
                           struct procinfo *pi, taskident task,
                           unsigned flags) {
  /* etime wants [[dd-]hh:]mm:ss */
  return format_interval(col->prop->fetch.fetch_intmax(pi, task), b, 0, columnsize,
                         col->arg, flags);
}

static void property_stime(const struct column *col, struct buffer *b,
                           size_t columnsize,
                           struct procinfo *pi, taskident task,
                           unsigned flags) {
  return format_time(col->prop->fetch.fetch_intmax(pi, task), b, columnsize,
                     col->arg, flags);
}

/* specific properties */

static void property_tty(const struct column *col, struct buffer *b,
                         size_t attribute((unused)) columnsize,
                         struct procinfo *pi, taskident task,
                         unsigned flags) {
  const char *path;
  int tty = col->prop->fetch.fetch_int(pi, task);
  if(tty <= 0) {
    buffer_putc(b, '-');
    return;
  }
  path = device_path(0, tty);
  if(!path) {
    format_integer(tty, b, 'x');
    return;
  }
  if(!(flags & FORMAT_RAW)) {
    /* "On XSI-conformant systems, they shall be given in one of two
     * forms: the device's filename (for example, tty04) or, if the
     * device's filename starts with tty, just the identifier following
     * the characters tty (for exameple, "04")" */
    if(!strncmp(path, "/dev/", 5))
      path += 5;
    if(!strncmp(path, "tty", 3))
      path += 3;
  }
  buffer_append(b, path);
}

static const char *property_pcomm(struct procinfo *pi, taskident task) {
  pid_t parent = proc_get_ppid(pi, task);
  if(parent) {
    taskident parent_task = { parent, -1 };
    return proc_get_comm(pi, parent_task);
  } else
    return NULL;
}

static void property_command_general(const struct column *col, 
                                     struct buffer *b,
                                     size_t columnsize,
                                     struct procinfo *pi,
                                     taskident task, int brief) {
  int n;
  size_t start = b->pos;
  const char *comm = col->prop->fetch.fetch_string(pi, task), *ptr;
  if(!comm)
    comm = "";
  if(brief && comm[0] != '[') {
    for(ptr = comm; *ptr && *ptr != ' '; ++ptr)
      if(*ptr == '/')
        comm = ptr + 1;
  }
  if(format_hierarchy) {
    for(n = proc_get_depth(pi, task); n > 0; --n)
      buffer_putc(b,' ');
  }
  /* "A process that has exited and has a parent, but has not yet been
   * waited for by the parent, shall be marked defunct." */
  if(proc_get_state(pi, task) != 'Z')
    buffer_append(b, comm);
  else
    buffer_printf(b, "%s <defunct>", comm);
  /* Truncate to column size */
  if(b->pos - start > columnsize)
    b->pos = start + columnsize;
}

static void property_command(const struct column *col, struct buffer *b,
                             size_t columnsize,
                             struct procinfo *pi, taskident task,
                             unsigned attribute((unused)) flags) {
  return property_command_general(col, b, columnsize, pi, task, 0);
}

static void property_command_brief(const struct column *col,
                                   struct buffer *b, size_t columnsize, 
                                   struct procinfo *pi,
                                   taskident task,
                                   unsigned attribute((unused)) flags) {
  return property_command_general(col, b, columnsize, pi, task, 1);
}

static void property_pcpu(const struct column *col, struct buffer *b,
                          size_t attribute((unused)) columnsize,
                          struct procinfo *pi, taskident task,
                          unsigned attribute((unused)) flags) {
  format_integer(100 * col->prop->fetch.fetch_double(pi, task), b, 'd');
}

static void property_mem(const struct column *col, struct buffer *b,
                         size_t attribute((unused)) columnsize,
                         struct procinfo *pi, taskident task,
                         unsigned flags) {
  char buffer[64];
  buffer_append(b,
                bytes(col->prop->fetch.fetch_uintmax(pi, task),
                      0, (flags & FORMAT_RAW ? 'b' : col->arg ? *col->arg : 0),
                      buffer, sizeof buffer));
}

static void property_address(const struct column *col, struct buffer *b,
                             size_t attribute((unused)) columnsize,
                             struct procinfo *pi, taskident task,
                             unsigned attribute((unused)) flags) {
  unsigned long long addr = col->prop->fetch.fetch_uintmax(pi, task);
  /* 0 and all-bits-1 are not very interesting addresses */
  if(addr && addr + 1 && addr != 0xFFFFFFFF)
    format_addr(addr, b);
  else
    /* suppress pointless values */
    buffer_putc(b, '-');
}

static void property_iorate(const struct column *col, struct buffer *b,
                            size_t attribute((unused)) columnsize,
                            struct procinfo *pi, taskident task,
                            unsigned flags) {
  char buffer[64];
  buffer_append(b,
                bytes(col->prop->fetch.fetch_double(pi, task),
                      0, (flags & FORMAT_RAW ? 'b' : col->arg ? *col->arg : 0),
                      buffer, sizeof buffer));
}

// ----------------------------------------------------------------------------

static int compare_int(const struct propinfo *prop, struct procinfo *pi,
                       taskident a, taskident b) {
  int av = prop->fetch.fetch_int(pi, a);
  int bv = prop->fetch.fetch_int(pi, b);
  return av < bv ? -1 : av > bv ? 1 : 0;
}

static int compare_intmax(const struct propinfo *prop, struct procinfo *pi,
                          taskident a, taskident b) {
  intmax_t av = prop->fetch.fetch_intmax(pi, a);
  intmax_t bv = prop->fetch.fetch_intmax(pi, b);
  return av < bv ? -1 : av > bv ? 1 : 0;
}

static int compare_uintmax(const struct propinfo *prop, struct procinfo *pi,
                           taskident a, taskident b) {
  uintmax_t av = prop->fetch.fetch_uintmax(pi, a);
  uintmax_t bv = prop->fetch.fetch_uintmax(pi, b);
  return av < bv ? -1 : av > bv ? 1 : 0;
}

static int compare_pid(const struct propinfo *prop, struct procinfo *pi,
                          taskident a, taskident b) {
  pid_t av = prop->fetch.fetch_pid(pi, a);
  pid_t bv = prop->fetch.fetch_pid(pi, b);
  return av < bv ? -1 : av > bv ? 1 : 0;
}

static int compare_uid(const struct propinfo *prop, struct procinfo *pi,
                       taskident a, taskident b) {
  uid_t av = prop->fetch.fetch_uid(pi, a);
  uid_t bv = prop->fetch.fetch_uid(pi, b);
  return av < bv ? -1 : av > bv ? 1 : 0;
}

static int compare_user(const struct propinfo *prop, struct procinfo *pi,
                        taskident a, taskident b) {
  uid_t av = prop->fetch.fetch_uid(pi, a);
  uid_t bv = prop->fetch.fetch_uid(pi, b);
  struct passwd *pw = getpwuid(av);
  char *ua = xstrdup(pw ? pw->pw_name : "");
  int rc;
  pw = getpwuid(bv);
  rc = strcmp(ua, pw ? pw->pw_name : "");
  free(ua);
  return rc;
}

static int compare_gid(const struct propinfo *prop, struct procinfo *pi,
                       taskident a, taskident b) {
  gid_t av = prop->fetch.fetch_gid(pi, a);
  gid_t bv = prop->fetch.fetch_gid(pi, b);
  return av < bv ? -1 : av > bv ? 1 : 0;
}

static int compare_group(const struct propinfo *prop, struct procinfo *pi,
                       taskident a, taskident b) {
  uid_t av = prop->fetch.fetch_uid(pi, a);
  uid_t bv = prop->fetch.fetch_uid(pi, b);
  struct group *gr = getgrgid(av);
  char *ga = xstrdup(gr ? gr->gr_name : "");
  int rc;
  gr = getgrgid(bv);
  rc = strcmp(ga, gr ? gr->gr_name : "");
  free(ga);
  return rc;
}

static int compare_double(const struct propinfo *prop, struct procinfo *pi,
                          taskident a, taskident b) {
  double av = prop->fetch.fetch_double(pi, a);
  double bv = prop->fetch.fetch_double(pi, b);
  return av < bv ? -1 : av > bv ? 1 : 0;
}

static int compare_string(const struct propinfo *prop, struct procinfo *pi,
                          taskident a, taskident b) {
  const char *av = prop->fetch.fetch_string(pi, a);
  const char *bv = prop->fetch.fetch_string(pi, b);
  return strcmp(av, bv);
}

static int compare_hier(const struct propinfo *prop, struct procinfo *pi,
                        taskident a, taskident b) {
  pid_t bp;
  taskident bptask;
  int adepth, bdepth;
  /* Deal with equal processes first */
  if(a.pid == b.pid)
    return 0;
  /* Put the depths in order */
  adepth = proc_get_depth(pi, a);
  bdepth = proc_get_depth(pi, b);
  if(adepth > bdepth)
    return -compare_hier(prop, pi, b, a);
  assert(adepth <= bdepth);
  /* Now adepth <= bdepth. */
  /* If A is B's parent, A < B */
  bp = proc_get_ppid(pi, b);
  if(a.pid == bp)
    return -1;
  /* If A and B share a parent, order them by PID */
  if(proc_get_ppid(pi, a) == bp)
    return a.pid < b.pid ? -1 : 1;
  /* Otherwise work back up the tree a bit */
  bptask.pid = bp;
  bptask.tid = -1;
  return compare_hier(prop, pi, a, bptask);
}

// ----------------------------------------------------------------------------

static const struct propinfo properties[] = {
  {
    "%cpu", NULL, "=pcpu", NULL, NULL, {}
  },
  {
    "_hier", NULL, NULL,
    NULL, compare_hier, { }
  },
  {
    "addr", "ADDR", "Instruction pointer address (hex)",
    property_address, compare_uintmax, { .fetch_uintmax = proc_get_insn_pointer }
  },
  {
    "args", "COMMAND", "Command with arguments",
    property_command, compare_string, { .fetch_string = proc_get_cmdline }
  },
  {
    "argsbrief", "COMMAND", "Command with arguments (but path removed)",
    property_command_brief, compare_string, { .fetch_string = proc_get_cmdline }
  },
  {
    "cmd", NULL, "=argsbrief", NULL, NULL, {}
  },
  {
    "comm", "COMMAND", "Command",
    property_command, compare_string, { .fetch_string = proc_get_comm }
  },
  {
    "command", NULL, "=argsbrief", NULL, NULL, {}
  },
  {
    "cputime", NULL, "=time", NULL, NULL, {}
  },
  {
    "egid", NULL, "=gid", NULL, NULL, {}
  },
  {
    "egroup", NULL, "=group", NULL, NULL, {}
  },
  {
    "etime", "ELAPSED", "Elapsed time (argument: format string)",
    property_etime, compare_intmax, { .fetch_intmax = proc_get_elapsed_time }
  },
  {
    "euid", NULL, "=uid", NULL, NULL, {}
  },
  {
    "euser", NULL, "=user", NULL, NULL, {}
  },
  {
    "f", NULL, "=flags", NULL, NULL, {}
  },
  {
    "flag", NULL, "=flags", NULL, NULL, {}
  },
  {
    "flags", "F", "Flags (octal; argument o/d/x/X)",
    property_uoctal, compare_uintmax, { .fetch_uintmax = proc_get_flags }
  },
  {
    "gid", "GID","Effective group ID (decimal)",
    property_gid, compare_gid, { .fetch_gid = proc_get_egid }
  },
  {
    "group", "GROUP", "Effective group ID (name)",
    property_group, compare_group, { .fetch_gid = proc_get_egid }
  },
  {
    "io", "IO", "Recent read+write rate (argument: K/M/G/T/P/p)",
    property_iorate, compare_double, { .fetch_double = proc_get_rw_bytes }
  },
  {
    "lwp", NULL, "=tid", NULL, NULL, {}
  },
  {
    "nlwp", NULL, "=threads", NULL, NULL, {}
  },
  {
    "majflt", "+FLT", "Major fault rate (argument: K/M/G/T/P/p)",
    property_iorate, compare_double, { .fetch_double = proc_get_majflt }
  },
  {
    "mem", "MEM", "Memory usage (argument: K/M/G/T/P/p) ",
    property_mem, compare_uintmax, { .fetch_uintmax = proc_get_mem }
  },
  {
    "minflt", "-FLT", "Minor fault rate (argument: K/M/G/T/P/p)",
    property_iorate, compare_double, { .fetch_double = proc_get_minflt }
  },
  {
    "ni", NULL, "=ni", NULL, NULL, {}
  },
  {
    "nice", "NI", "Nice value",
    property_decimal, compare_intmax, { .fetch_intmax = proc_get_nice }
  },
  {
    "oom", "OOM", "OOM score",
    property_decimal, compare_intmax, { .fetch_intmax = proc_get_oom_score }
  },
  {
    "pcomm", "PCMD", "Parent command name",
    property_command, compare_string, { .fetch_string = property_pcomm },
  },
  {
    "pcpu", "%CPU", "%age CPU used",
    property_pcpu, compare_double, { .fetch_double = proc_get_pcpu }
  },
  {
    "pgid", "PGID", "Process group ID",
    property_pid, compare_pid, { .fetch_pid = proc_get_pgid }
  },
  {
    "pgrp", NULL, "=pgid", NULL, NULL, {}
  },
  {
    "pid", "PID", "Process ID",
    property_pid, compare_pid, { .fetch_pid = proc_get_pid }
  },
  {
    "pmem", "PMEM", "Proportional memory usage (argument: K/M/G/T/P/p)",
    property_mem, compare_uintmax, { .fetch_uintmax = proc_get_pmem }
  },
  {
    "ppid", "PPID", "Parent process ID",
    property_pid, compare_pid, { .fetch_pid = proc_get_ppid }
  },
  {
    "pri", "PRI", "Priority",
    property_decimal, compare_intmax, { .fetch_intmax = proc_get_priority }
  },
  {
    "pss", "PSS", "Proportional resident set size (argument: K/M/G/T/P/p)",
    property_mem, compare_uintmax, { .fetch_uintmax = proc_get_pss }
  },
  {
    "read", "RD", "Recent read rate (argument: K/M/G/T/P/p)",
    property_iorate, compare_double, { .fetch_double = proc_get_read_bytes }
  },
  {
    "rgid", "RGID", "Real group ID (decimal)",
    property_gid, compare_gid, { .fetch_gid = proc_get_rgid }
  },
  {
    "rgroup", "RGROUP", "Real group ID (name)",
    property_group, compare_group, { .fetch_gid = proc_get_rgid }
  },
  {
    "rss", "RSS", "Resident set size (argument: K/M/G/T/P/p)",
    property_mem, compare_uintmax, { .fetch_uintmax = proc_get_rss }
  },
  {
    "rssize", NULL, "=rss", NULL, NULL, {},
  },
  {
    "rsz", NULL, "=rss", NULL, NULL, {},
  },
  {
    "ruid", "RUID", "Real user ID (decimal)",
    property_uid, compare_uid, { .fetch_uid = proc_get_ruid }
  },
  {
    "ruser", "RUSER", "Real user ID (name)",
    property_user, compare_user, { .fetch_uid = proc_get_ruid }
  },
  {
    "sess", NULL, "=sid", NULL, NULL, {},
  },
  {
    "session", NULL, "=sid", NULL, NULL, {},
  },
  {
    "sid", "SID", "Session ID",
    property_pid, compare_pid, { .fetch_pid = proc_get_session }
  },
  {
    "state", "S", "Process state",
    property_char, compare_int, { .fetch_int = proc_get_state }
  },
  {
    "stime", "STIME", "Start time (argument: strftime format string)",
    property_stime, compare_intmax, { .fetch_intmax = proc_get_start_time }
  },
  {
    "swap", "SWAP", "Swap usage (argument: K/M/G/T/P/p)",
    property_mem, compare_uintmax, { .fetch_uintmax = proc_get_swap }
  },
  {
    "thcount", NULL, "=threads", NULL, NULL, {}
  },
  {
    "threads", "T", "Number of threads",
    property_num_threads, compare_pid, { .fetch_int = proc_get_num_threads }
  },
  {
    "tid", "TID", "Thread ID",
    property_pid, compare_pid, { .fetch_pid = proc_get_tid }
  },
  {
    "time", "TIME", "Scheduled time (argument: format string)",
    property_time, compare_intmax, { .fetch_intmax = proc_get_scheduled_time }
  },
  {
    "tname", NULL, "=tty", NULL, NULL, {}
  },
  {
    "tt", NULL, "=tty", NULL, NULL, {}
  },
  {
    "tty", "TT", "Terminal",
    property_tty, compare_int, { .fetch_int = proc_get_tty }
  },
  {
    "uid", "UID", "Effective user ID (decimal)",
    property_uid, compare_uid, { .fetch_uid = proc_get_euid }
  },
  {
    "user", "USER", "Effective user ID (name)",
    property_user, compare_user, { .fetch_uid = proc_get_euid }
  },
  {
    "vsize", NULL, "=vsz", NULL, NULL, {}
  },
  {
    "vsz", "VSZ", "Virtual memory used (argument: K/M/G/T/P/p)",
    property_mem, compare_uintmax, { .fetch_uintmax = proc_get_vsize }
  },
  {
    "wchan", "WCHAN", "Wait channel (hex)",
    property_address, compare_uintmax, { .fetch_uintmax = proc_get_wchan }
  },
  {
    "write", "WR", "Recent write rate (argument: K/M/G/T/P/p)",
    property_iorate, compare_double, { .fetch_double = proc_get_write_bytes }
  },
};
#define NPROPERTIES (sizeof properties / sizeof *properties)

static const struct propinfo *find_property(const char *name, unsigned flags) {
  ssize_t l = 0, r = NPROPERTIES - 1, m;
  int c;
  while(l <= r) {
    m = l + (r - l) / 2;
    c = strcmp(name, properties[m].name);
    if(c < 0)
      r = m - 1;
    else if(c > 0)
      l = m + 1;
    else {
      if(properties[m].description || (flags & FORMAT_INTERNAL)) {
        if(properties[m].description && properties[m].description[0] == '=')
          return find_property(properties[m].description + 1, flags);
        return &properties[m];
      } else
        return NULL;
    }
  }
  return NULL;
}

int format_set(const char *f, unsigned flags) {
  char buffer[128], heading_buffer[128], *heading, arg_buffer[128], *arg, *e;
  size_t i;
  const struct propinfo *prop;
  size_t reqwidth;
  if(!(flags & (FORMAT_CHECK|FORMAT_ADD)))
    format_clear();
  while(*f) {
    if(*f == ' ' || *f == ',') {
      ++f;
      continue;
    }
    i = 0;
    while(*f && (*f != ' ' && *f != ',' && *f != '=' && *f != ':' && *f != '/')) {
      if(i < sizeof buffer - 1)
        buffer[i++] = *f;
      ++f;
    }
    buffer[i] = 0;
    if(*f == ':') {
      ++f;
      errno = 0;
      reqwidth = strtoul(f, &e, 10);
      if(errno || e == f) {
        if(flags & FORMAT_CHECK)
          return 0;
        else
          fatal(errno, "invalid column size");
      }
      f = e;
    } else
      reqwidth = SIZE_MAX;
    if(*f == '=') {
      if(!(f = format_parse_arg(f + 1, heading_buffer, sizeof heading_buffer, flags)))
        return 0;
      heading = heading_buffer;
    } else
      heading = NULL;
    if(*f == '/') {
      if(!(f = format_parse_arg(f + 1, arg_buffer, sizeof arg_buffer,
                                flags|FORMAT_QUOTED)))
        return 0;
      arg = arg_buffer;
    } else
      arg = NULL;
    prop = find_property(buffer, flags);
    if(flags & FORMAT_CHECK) {
      if(!prop)
        return 0;
    } else {
      if(!prop)
        fatal(0, "unknown process property '%s'", buffer);
      if((ssize_t)(ncolumns + 1) < 0)
        fatal(0, "too many columns");
      columns = xrecalloc(columns, ncolumns + 1, sizeof *columns);
      memset(&columns[ncolumns], 0, sizeof *columns);
      columns[ncolumns].prop = prop;
      columns[ncolumns].heading = xstrdup(heading ? heading 
                                          : columns[ncolumns].prop->heading);
      columns[ncolumns].arg = arg ? xstrdup(arg) : NULL;
      columns[ncolumns].reqwidth = reqwidth;
      ++ncolumns;
    }
  }
  return 1;
}

const char *format_parse_arg(const char *ptr,
                             char buffer[],
                             size_t bufsize,
                             unsigned flags) {
  size_t i = 0;
  int q;

  if(flags & FORMAT_QUOTED) {
    /* property=heading extends until we hit a separator
     * property="heading" extends to the close quote; ' is allowed too
     */
    if(*ptr == '"' || *ptr == '\'') {
      q = *ptr++;
      while(*ptr && *ptr != q) {
        /* \ escapes the next character (there must be one) */
        if(*ptr == '\\') {
          if(ptr[1] != 0)
            ++ptr;
          else if(flags & FORMAT_CHECK)
            return 0;
        }
        if(i < bufsize - 1)
          buffer[i++] = *ptr;
        ++ptr;
      }
      /* The close quotes must exist */
      if(*ptr == q)
        ++ptr;
      else
        if(flags & FORMAT_CHECK)
          return 0;
    } else {
      /* unquoted heading */
      while(*ptr && (*ptr != ' ' && *ptr != ',')) {
        if(i < bufsize - 1)
          buffer[i++] = *ptr;
        ++ptr;
      }
    }
    buffer[i] = 0;
  } else {
    /* property=heading extends to the end of the argument, not until
     * the next comma; "The default header can be overridden by
     * appending an <equals-sign> and the new text of the
     * header. The rest of the characters in the argument shall be
     * used as the header text." */
    while(*ptr) {
      if(i < bufsize - 1)
        buffer[i++] = *ptr;
      ++ptr;
    }
  }
  buffer[i] = 0;
  return ptr;
}

void format_clear(void) {
  size_t n;
  for(n = 0; n < ncolumns; ++n) {
    free(columns[n].heading);
    free(columns[n].arg);
  }
  ncolumns = 0;
  free(columns);
  columns = NULL;
}

void format_columns(struct procinfo *pi, const taskident *tasks, size_t ntasks) {
  size_t n = 0, c;

  /* "The field widths shall be selected by the system to be at least
   * as wide as the header text (default or overridden value). If the
   * header text is null, such as -o user=, the field width shall be
   * at least as wide as the default header text." */
  for(c = 0; c < ncolumns; ++c) {
    size_t wmin, w = strlen(*columns[c].heading
                            ? columns[c].heading
                            : columns[c].prop->heading);
    // We make columns wide enough for everything that may be
    // put in them.
    for(n = 0; n < ntasks; ++n) {
      // Render the value to a false buffer to find out how big it is
      struct buffer b[1];
      buffer_init(b);
      columns[c].prop->format(&columns[c], b, columns[c].reqwidth,
                              pi, tasks[n], 0);
      free(b->base);
      if(b->pos > w)
        w = b->pos;
    }
    /* We make the columns as wide as they have needed to be at any
     * point in the recent past, to avoid columns wobbling too
     * much. */
    wmin = w;
    for(n = 0; n < ANTIWOBBLE; ++n)
      if(columns[c].oldwidths[n] > w)
        w = columns[c].oldwidths[n];
    /* Record the width we needed for next time.  Note that this may
     * be less than the width eventually chosen due to the
     * anti-wobble. */
    columns[c].oldwidths[columns[c].oldwidthind++] = wmin;
    columns[c].oldwidthind %= ANTIWOBBLE;
    columns[c].width = w;
  }
}

void format_heading(struct procinfo *pi, struct buffer *b) {
  static const taskident tnone = { -1, -1 };
  size_t c;
  for(c = 0; c < ncolumns && !*columns[c].heading; ++c)
    ;
  if(c < ncolumns)
    format_process(pi, tnone, b);
}

void format_process(struct procinfo *pi, taskident task, struct buffer *b) {
  size_t w, left, c, start;
  b->pos = 0;
  for(c = 0; c < ncolumns; ++c) {
    start = b->pos;
    if(task.pid == -1)
      buffer_append(b, columns[c].heading);
    else
      columns[c].prop->format(&columns[c], b, columns[c].width, pi, task, 0);
    /* Figure out how much we wrote */
    w = b->pos - start;
    /* For non-final columns, pad to the column width and one more for
     * the column separator */
    if(c + 1 < ncolumns) {
      left = 1 + columns[c].width - w;
      while(left-- > 0)
        buffer_putc(b, ' ');
    }
  }
  buffer_terminate(b);
}

void format_value(struct procinfo *pi, taskident task,
                  const char *property,
                  struct buffer *b,
                  unsigned flags) {
  struct column c;              /* stunt column */
  const struct propinfo *prop = find_property(property, 0);
  if(!prop)
    fatal(0, "unknown process property '%s'", property);
  c.prop = prop;
  c.reqwidth = SIZE_MAX;
  c.width = SIZE_MAX;
  c.heading = NULL;
  c.arg = NULL;
  b->pos = 0;
  prop->format(&c, b, SIZE_MAX, pi, task, flags);
  buffer_terminate(b);
}

int format_ordering(const char *ordering, unsigned flags) {
  char buffer[128], *b;
  size_t i;
  const struct propinfo *prop;
  int sign;
  if(!(flags & (FORMAT_CHECK|FORMAT_ADD))) {
    free(orders);
    orders = NULL;
    norders = 0;
  }
  while(*ordering) {
    if(*ordering == ' ' || *ordering == ',') {
      ++ordering;
      continue;
    }
    i = 0;
    while(*ordering && *ordering != ' ' && *ordering != ',') {
      if(i < sizeof buffer - 1)
        buffer[i++] = *ordering;
      ++ordering;
    }
    buffer[i] = 0;
    b = buffer;
    switch(*b) {
    case '+':
      ++b;
      sign = -1;
      break;
    case '-':
      ++b;
      sign = 1;
      break;
    default:
      sign = -1;
      break;
    }
    prop = find_property(b, flags);
    if(flags & FORMAT_CHECK) {
      if(!prop)
        return 0;
    } else {
      if(!prop)
        fatal(0, "unknown process property '%s'", buffer);
      if((ssize_t)(norders + 1) < 0)
        fatal(0, "too many columns");
      orders = xrecalloc(orders, norders + 1, sizeof *orders);
      orders[norders].prop = prop;
      orders[norders].sign = sign;
      ++norders;
    }
  }
  return 1;
}

char *format_get_ordering(void) {
  size_t n;
  struct buffer b[1];

  buffer_init(b);
  for(n = 0; n < norders; ++n) {
    if(n)
      buffer_putc(b, ' ');
    buffer_putc(b, orders[n].sign < 0 ? '+' : '-');
    buffer_append(b, orders[n].prop->name);
  }
  buffer_terminate(b);
  return b->base;
}

int format_compare(struct procinfo *pi, taskident a, taskident b) {
  size_t n;
  int c;
  for(n = 0; n < norders; ++n)
    if((c = orders[n].prop->compare(orders[n].prop, pi, a, b)))
      return orders[n].sign < 0 ? -c : c;
  /* Default order is by PID */
  if(a.pid < b.pid)
    return -1;
  if(a.pid > b.pid)
    return 1;
  /* Within one process, order by thread ID; real process first. */
  if((unsigned long)a.tid < (unsigned long)b.tid)
    return -1;
  if((unsigned long)a.tid > (unsigned long)b.tid)
    return 1;
  return 0;
}

char **format_help(void) {
  size_t n;
  char *ptr, **result, **next;

  next = result = xrecalloc(NULL, 2 + NPROPERTIES, sizeof (char *));
  *next++ = xstrdup("  Property   Heading  Description");
  for(n = 0; n < NPROPERTIES; ++n) {
    if(properties[n].description && properties[n].description[0] != '=') {
      if(asprintf(&ptr, "  %-9s  %-7s  %s",
                  properties[n].name,
                  properties[n].heading,
                  properties[n].description) < 0)
        fatal(errno, "asprintf");
      *next++ = ptr;
    }
  }
  *next = NULL;
  return result;
}

char *format_get(void) {
  size_t n;
  const char *h;
  struct buffer b[1];
  
  buffer_init(b);
  for(n = 0; n < ncolumns; ++n) {
    if(n)
      buffer_putc(b, ' ');
    buffer_append(b, columns[n].prop->name);
    if(columns[n].reqwidth != SIZE_MAX)
      buffer_printf(b, ":%zu", columns[n].reqwidth);
    h = columns[n].heading;
    if(strcmp(h, columns[n].prop->heading)) {
      buffer_putc(b, '=');
      format_get_arg(b, h, !!columns[n].arg);
    }
    if(columns[n].arg) {
      buffer_putc(b, '/');
      format_get_arg(b, columns[n].arg, 0);
    }
  }
  buffer_terminate(b);
  return b->base;
}

void format_get_arg(struct buffer *b, const char *arg, int quote) {
  if(quote
     || strchr(arg, ' ')
     || strchr(arg, '"')
     || strchr(arg, '\\')
     || strchr(arg, ',')) {
    buffer_putc(b, '"');
    while(*arg) {
      if(*arg == '"' || *arg == '\\')
        buffer_putc(b, '\\');
      buffer_putc(b, *arg++);
    }
    buffer_putc(b, '"');
  } else
    buffer_append(b, arg);
}

int format_hierarchy;
