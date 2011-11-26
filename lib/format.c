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

typedef void formatfn(const struct propinfo *prop, struct buffer *b,
                      size_t columnsize,
                      struct procinfo *pi, pid_t pid);

typedef int comparefn(const struct propinfo *prop, struct procinfo *pi, 
                      pid_t a, pid_t b);

struct propinfo {
  const char *name;
  const char *heading;
  const char *description;
  formatfn *format;
  comparefn *compare;
  union {
    int (*fetch_int)(struct procinfo *, pid_t);
    intmax_t (*fetch_intmax)(struct procinfo *, pid_t);
    uintmax_t (*fetch_uintmax)(struct procinfo *, pid_t);
    pid_t (*fetch_pid)(struct procinfo *, pid_t);
    uid_t (*fetch_uid)(struct procinfo *, pid_t);
    gid_t (*fetch_gid)(struct procinfo *, pid_t);
    double (*fetch_double)(struct procinfo *, pid_t);
    const char *(*fetch_string)(struct procinfo *, pid_t);
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

static void format_decimal(intmax_t im, struct buffer *b) {
  char t[64];
  snprintf(t, sizeof t, "%jd", im);
  buffer_append(b, t);
}

static void format_octal(uintmax_t im, struct buffer *b) {
  char t[64];
  snprintf(t, sizeof t, "%jo", im);
  buffer_append(b, t);
}

static void format_hex(uintmax_t im, struct buffer *b) {
  char t[64];
  if(im > 0xFFFFFFFFFFFFLL)
    snprintf(t, sizeof t, "%016jx", im);
  else if(im > 0xFFFFFFFF)
    snprintf(t, sizeof t, "%012jx", im);
  else
    snprintf(t, sizeof t, "%08jx", im);
  buffer_append(b, t);
}

static void format_usergroup(intmax_t id, struct buffer *b, size_t columnsize,
                             const char *name) {
  if(name && strlen(name) <= columnsize)
    buffer_append(b, name);
  else
    format_decimal(id, b);
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
                            int always_hours, size_t columnsize) {
  char t[64];
  if(seconds >= 86400) {
    snprintf(t, sizeof t, "%ld-%02ld:%02ld:%02ld",
             seconds / 86400,
             (seconds % 86400) / 3600,
             (seconds % 3600) / 60,
             (seconds % 60));
    if(strlen(t) > columnsize)
      snprintf(t, sizeof t, "%ld-%02ld", 
               seconds / 86400,
               (seconds % 86400) / 3600);
    if(strlen(t) > columnsize)
      snprintf(t, sizeof t, "%ldd", 
               seconds / 86400);
  } else if(seconds >= 3600 || (always_hours && columnsize > 5)) {
    snprintf(t, sizeof t, "%02ld:%02ld:%02ld",
             seconds / 3600,
             (seconds % 3600) / 60,
             (seconds % 60));
  } else {
    snprintf(t, sizeof t, "%02ld:%02ld",
             seconds / 60,
             (seconds % 60));
  }
  buffer_append(b, t);
}

static void format_time(time_t when, struct buffer *b, size_t columnsize) {
  char t[64];
  time_t now;
  struct tm when_tm, now_tm;

  time(&now);
  localtime_r(&when, &when_tm);
  localtime_r(&now, &now_tm);
  if(columnsize != SIZE_MAX && columnsize >= 19)
    strftime(t, sizeof t, "%Y-%m-%dT%H:%M:%S", &when_tm);
  else if(now_tm.tm_year == when_tm.tm_year
          && now_tm.tm_mon == when_tm.tm_mon
          && now_tm.tm_mday == when_tm.tm_mday) {
    if(columnsize < 8)
      strftime(t, sizeof t, "%H:%M", &when_tm);
    else
      strftime(t, sizeof t, "%H:%M:%S", &when_tm);
  } else if(columnsize < 10 && now_tm.tm_year == when_tm.tm_year)
    strftime(t, sizeof t, "%m-%d", &when_tm);
  else
    strftime(t, sizeof t, "%Y-%m-%d", &when_tm);
  buffer_append(b, t);
}

// ----------------------------------------------------------------------------

/* generic properties */

static void property_decimal(const struct propinfo *prop, struct buffer *b,
                             size_t attribute((unused)) columnsize,
                             struct procinfo *pi, pid_t pid) {
  return format_decimal(prop->fetch.fetch_intmax(pi, pid), b);
}

static void property_uoctal(const struct propinfo *prop, struct buffer *b,
                            size_t attribute((unused)) columnsize,
                            struct procinfo *pi, pid_t pid) {
  return format_octal(prop->fetch.fetch_uintmax(pi, pid), b);
}

static void property_uhex(const struct propinfo *prop, struct buffer *b,
                          size_t attribute((unused)) columnsize,
                          struct procinfo *pi, pid_t pid) {
  format_hex(prop->fetch.fetch_uintmax(pi, pid), b);
}

static void property_pid(const struct propinfo *prop, struct buffer *b,
                         size_t attribute((unused)) columnsize, 
                         struct procinfo *pi, pid_t pid) {
  return format_decimal(prop->fetch.fetch_pid(pi, pid), b);
}

static void property_uid(const struct propinfo *prop, struct buffer *b,
                         size_t attribute((unused)) columnsize,
                         struct procinfo *pi, pid_t pid) {
  return format_decimal(prop->fetch.fetch_uid(pi, pid), b);
}

static void property_user(const struct propinfo *prop, struct buffer *b,
                          size_t columnsize, struct procinfo *pi, pid_t pid) {
  return format_user(prop->fetch.fetch_uid(pi, pid), b, columnsize);
}

static void property_gid(const struct propinfo *prop, struct buffer *b,
                         size_t attribute((unused)) columnsize,
                         struct procinfo *pi, pid_t pid) {
  return format_decimal(prop->fetch.fetch_gid(pi, pid), b);
}

static void property_group(const struct propinfo *prop, struct buffer *b,
                           size_t columnsize, struct procinfo *pi, pid_t pid) {
  return format_group(prop->fetch.fetch_gid(pi, pid), b, columnsize);
}

static void property_char(const struct propinfo *prop, struct buffer *b,
                          size_t attribute((unused)) columnsize,
                          struct procinfo *pi, pid_t pid) {
  buffer_putc(b, prop->fetch.fetch_int(pi, pid));
}

/* time properties */

static void property_time(const struct propinfo *prop, struct buffer *b,
                          size_t columnsize,
                          struct procinfo *pi, pid_t pid) {
  /* time wants [dd-]hh:mm:ss */
  return format_interval(prop->fetch.fetch_intmax(pi, pid), b, 1, columnsize);
}

static void property_etime(const struct propinfo *prop, struct buffer *b,
                           size_t attribute((unused)) columnsize,
                           struct procinfo *pi, pid_t pid) {
  /* etime wants [[dd-]hh:]mm:ss */
  return format_interval(prop->fetch.fetch_intmax(pi, pid), b, 0, columnsize);
}

static void property_stime(const struct propinfo *prop, struct buffer *b,
                           size_t columnsize,
                           struct procinfo *pi, pid_t pid) {
  return format_time(prop->fetch.fetch_intmax(pi, pid), b, columnsize);
}

/* specific properties */

static void property_tty(const struct propinfo *prop, struct buffer *b,
                         size_t attribute((unused)) columnsize,
                         struct procinfo *pi, pid_t pid) {
  const char *path;
  int tty = prop->fetch.fetch_int(pi, pid);
  if(tty <= 0) {
    buffer_putc(b, '-');
    return;
  }
  path = device_path(0, tty);
  if(!path) {
    format_hex(tty, b);
    return;
  }
  /* "On XSI-conformant systems, they shall be given in one of two
   * forms: the device's filename (for example, tty04) or, if the
   * device's filename starts with tty, just the identifier following
   * the characters tty (for example, "04")" */
  if(!strncmp(path, "/dev/", 5))
    path += 5;
  if(!strncmp(path, "tty", 3))
    path += 3;
  buffer_append(b, path);
}

static void property_command_general(const struct propinfo *prop, 
                                     struct buffer *b,
                                     size_t columnsize,
                                     struct procinfo *pi,
                                     pid_t pid, int brief) {
  int n;
  char *t;
  size_t start = b->pos;
  const char *comm = prop->fetch.fetch_string(pi, pid), *ptr;
  if(brief && comm[0] != '[') {
    for(ptr = comm; *ptr && *ptr != ' '; ++ptr)
      if(*ptr == '/')
        comm = ptr + 1;
  }
  if(format_hierarchy) {
    for(n = proc_get_depth(pi, pid); n > 0; --n)
      buffer_putc(b,' ');
  }
  /* "A process that has exited and has a parent, but has not yet been
   * waited for by the parent, shall be marked defunct." */
  if(proc_get_state(pi, pid) != 'Z') {
    buffer_append(b, comm);
  } else {
    if(asprintf(&t, "%s <defunct>", comm) < 0)
      fatal(errno, "asprintf");
    buffer_append(b, t);
    free(t);
  }
  /* Truncate to column size */
  if(b->pos - start > columnsize)
    b->pos = start + columnsize;
}

static void property_command(const struct propinfo *prop, struct buffer *b,
                             size_t columnsize,
                             struct procinfo *pi, pid_t pid) {
  return property_command_general(prop, b, columnsize, pi, pid, 0);
}

static void property_command_brief(const struct propinfo *prop,
                                   struct buffer *b, size_t columnsize, 
                                   struct procinfo *pi,
                                   pid_t pid) {
  return property_command_general(prop, b, columnsize, pi, pid, 1);
}

static void property_pcpu(const struct propinfo *prop, struct buffer *b,
                          size_t attribute((unused)) columnsize,
                          struct procinfo *pi, pid_t pid) {
  format_decimal(100 * prop->fetch.fetch_double(pi, pid), b);
}

static void property_mem(const struct propinfo *prop, struct buffer *b,
                         size_t attribute((unused)) columnsize,
                         struct procinfo *pi, pid_t pid) {
  char buffer[64];
  buffer_append(b,
                bytes(prop->fetch.fetch_uintmax(pi, pid), 0,
                      buffer, sizeof buffer));
}

static void property_memK(const struct propinfo *prop, struct buffer *b,
                         size_t attribute((unused)) columnsize,
                         struct procinfo *pi, pid_t pid) {
  format_decimal(prop->fetch.fetch_uintmax(pi, pid) / 1024, b);
}

static void property_memM(const struct propinfo *prop, struct buffer *b,
                          size_t attribute((unused)) columnsize,
                          struct procinfo *pi, pid_t pid) {
  format_decimal(prop->fetch.fetch_uintmax(pi, pid) / 1048476, b);
}

static void property_address(const struct propinfo *prop, struct buffer *b,
                             size_t attribute((unused)) columnsize,
                             struct procinfo *pi, pid_t pid) {
  unsigned long long addr = prop->fetch.fetch_uintmax(pi, pid);
  /* 0 and all-bits-1 are not very interesting addresses */
  if(addr && addr + 1 && addr != 0xFFFFFFFF)
    format_hex(addr, b);
  else
    /* suppress pointless values */
    buffer_putc(b, '-');
}

static void property_iorate(const struct propinfo *prop, struct buffer *b,
                            size_t attribute((unused)) columnsize,
                            struct procinfo *pi, pid_t pid) {
  char buffer[64];
  buffer_append(b,
                bytes(prop->fetch.fetch_double(pi, pid), 0,
                      buffer, sizeof buffer));
}

static void property_iorateK(const struct propinfo *prop, struct buffer *b,
                             size_t attribute((unused)) columnsize,
                             struct procinfo *pi, pid_t pid) {
  format_decimal(prop->fetch.fetch_double(pi, pid) / 1024, b);
}

static void property_iorateM(const struct propinfo *prop, struct buffer *b,
                             size_t attribute((unused)) columnsize,
                             struct procinfo *pi, pid_t pid) {
  format_decimal(prop->fetch.fetch_double(pi, pid) / 1048476, b);
}

static void property_iorateP(const struct propinfo *prop, struct buffer *b,
                             size_t attribute((unused)) columnsize,
                             struct procinfo *pi, pid_t pid) {
  format_decimal(prop->fetch.fetch_double(pi, pid) / sysconf(_SC_PAGESIZE), b);
}

// ----------------------------------------------------------------------------

static int compare_int(const struct propinfo *prop, struct procinfo *pi,
                           pid_t a, pid_t b) {
  int av = prop->fetch.fetch_int(pi, a);
  int bv = prop->fetch.fetch_int(pi, b);
  return av < bv ? -1 : av > bv ? 1 : 0;
}

static int compare_intmax(const struct propinfo *prop, struct procinfo *pi,
                          pid_t a, pid_t b) {
  intmax_t av = prop->fetch.fetch_intmax(pi, a);
  intmax_t bv = prop->fetch.fetch_intmax(pi, b);
  return av < bv ? -1 : av > bv ? 1 : 0;
}

static int compare_uintmax(const struct propinfo *prop, struct procinfo *pi,
                           pid_t a, pid_t b) {
  uintmax_t av = prop->fetch.fetch_uintmax(pi, a);
  uintmax_t bv = prop->fetch.fetch_uintmax(pi, b);
  return av < bv ? -1 : av > bv ? 1 : 0;
}

static int compare_pid(const struct propinfo *prop, struct procinfo *pi,
                          pid_t a, pid_t b) {
  pid_t av = prop->fetch.fetch_pid(pi, a);
  pid_t bv = prop->fetch.fetch_pid(pi, b);
  return av < bv ? -1 : av > bv ? 1 : 0;
}

static int compare_uid(const struct propinfo *prop, struct procinfo *pi,
                       pid_t a, pid_t b) {
  uid_t av = prop->fetch.fetch_uid(pi, a);
  uid_t bv = prop->fetch.fetch_uid(pi, b);
  return av < bv ? -1 : av > bv ? 1 : 0;
}

static int compare_user(const struct propinfo *prop, struct procinfo *pi,
                        pid_t a, pid_t b) {
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
                       pid_t a, pid_t b) {
  gid_t av = prop->fetch.fetch_gid(pi, a);
  gid_t bv = prop->fetch.fetch_gid(pi, b);
  return av < bv ? -1 : av > bv ? 1 : 0;
}

static int compare_group(const struct propinfo *prop, struct procinfo *pi,
                       pid_t a, pid_t b) {
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
                          pid_t a, pid_t b) {
  double av = prop->fetch.fetch_double(pi, a);
  double bv = prop->fetch.fetch_double(pi, b);
  return av < bv ? -1 : av > bv ? 1 : 0;
}

static int compare_string(const struct propinfo *prop, struct procinfo *pi,
                          pid_t a, pid_t b) {
  const char *av = prop->fetch.fetch_string(pi, a);
  const char *bv = prop->fetch.fetch_string(pi, b);
  return strcmp(av, bv);
}

static int compare_hier(const struct propinfo *prop, struct procinfo *pi,
                        pid_t a, pid_t b) {
  pid_t bp;
  int adepth, bdepth;
  /* Deal with equal processes first */
  if(a == b)
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
  if(a == bp)
    return -1;
  /* If A and B share a parent, order them by PID */
  if(proc_get_ppid(pi, a) == bp)
    return a < b ? -1 : 1;
  /* Otherwise work back up the tree a bit */
  return compare_hier(prop, pi, a, bp);
}

// ----------------------------------------------------------------------------

static const struct propinfo properties[] = {
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
    "comm", "COMMAND", "Command",
    property_command, compare_string, { .fetch_string = proc_get_comm }
  },
  {
    "etime", "ELAPSED", "Elapsed time",
    property_etime, compare_intmax, { .fetch_intmax = proc_get_elapsed_time }
  },
  {
    "flags", "F", "Flags (octal)",
    property_uoctal, compare_uintmax, { .fetch_uintmax = proc_get_flags }
  },
  {
    "flagsx", "F", "Flags (hex)",
    property_uhex, compare_uintmax, { .fetch_uintmax = proc_get_flags }
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
    "io", "IO", "Recent read+write rate",
    property_iorate, compare_double, { .fetch_double = proc_get_rw_bytes }
  },
  {
    "ioK", "IO", "Recent read+write rate (1024 byte/s)",
    property_iorateK, compare_double, { .fetch_double = proc_get_rw_bytes }
  },
  {
    "ioM", "IO", "Recent read+write rate (megabytes/s)",
    property_iorateM, compare_double, { .fetch_double = proc_get_rw_bytes }
  },
  {
    "majflt", "+FLT", "Major fault rate",
    property_iorate, compare_double, { .fetch_double = proc_get_majflt }
  },
  {
    "majfltK", "+FLT", "Major fault rate (1024 byte/s)",
    property_iorateK, compare_double, { .fetch_double = proc_get_majflt }
  },
  {
    "majfltM", "+FLT", "Major fault rate (megabytes/s)",
    property_iorateM, compare_double, { .fetch_double = proc_get_majflt }
  },
  {
    "majfltP", "+FLT", "Major fault rate (pages/s)",
    property_iorateP, compare_double, { .fetch_double = proc_get_majflt }
  },
  {
    "mem", "MEM", "Memory usage",
    property_mem, compare_uintmax, { .fetch_uintmax = proc_get_mem }
  },
  {
    "memK", "MEM", "Memory usage (megabytes)",
    property_memK, compare_uintmax, { .fetch_uintmax = proc_get_mem }
  },
  {
    "memM", "MEM", "Memory usage (megabytes)",
    property_memM, compare_uintmax, { .fetch_uintmax = proc_get_mem }
  },
  {
    "minflt", "-FLT", "Minor fault rate",
    property_iorate, compare_double, { .fetch_double = proc_get_minflt }
  },
  {
    "minfltK", "-FLT", "Minor fault rate (1024 bytes/s)",
    property_iorateK, compare_double, { .fetch_double = proc_get_minflt }
  },
  {
    "minfltM", "-FLT", "Minor fault rate (megabytes/s)",
    property_iorateM, compare_double, { .fetch_double = proc_get_minflt }
  },
  {
    "minfltP", "-FLT", "Minor fault rate (pages/s)",
    property_iorateP, compare_double, { .fetch_double = proc_get_minflt }
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
    "pcpu", "%CPU", "%age CPU used",
    property_pcpu, compare_double, { .fetch_double = proc_get_pcpu }
  },
  {
    "pgid", "PGID", "Process group ID",
    property_pid, compare_pid, { .fetch_pid = proc_get_pgid }
  },
  {
    "pid", "PID", "Process ID",
    property_pid, compare_pid, { .fetch_pid = proc_get_pid }
  },
  {
    "pmem", "PMEM", "Proportional memory usage",
    property_mem, compare_uintmax, { .fetch_uintmax = proc_get_pmem }
  },
  {
    "pmemK", "PMEM", "Proportional memory usage (1024 bytes)",
    property_memK, compare_uintmax, { .fetch_uintmax = proc_get_pmem }
  },
  {
    "pmemM", "PMEM", "Proportional memory usage (megabytes)",
    property_memM, compare_uintmax, { .fetch_uintmax = proc_get_pmem }
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
    "pss", "PSS", "Proportional resident set size",
    property_mem, compare_uintmax, { .fetch_uintmax = proc_get_pss }
  },
  {
    "pssK", "PSS", "Proportional resident set size (1024 bytes)",
    property_memK, compare_uintmax, { .fetch_uintmax = proc_get_pss }
  },
  {
    "pssM", "PSS", "Proportional resident set size (megabytes)",
    property_memM, compare_uintmax, { .fetch_uintmax = proc_get_pss }
  },
  {
    "read", "RD", "Recent read rate (1024 bytes/s)",
    property_iorate, compare_double, { .fetch_double = proc_get_read_bytes }
  },
  {
    "readK", "RD", "Recent read rate (1024 byte/s)",
    property_iorateK, compare_double, { .fetch_double = proc_get_read_bytes }
  },
  {
    "readM", "RD", "Recent read rate (megabyte/s)",
    property_iorateM, compare_double, { .fetch_double = proc_get_read_bytes }
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
    "rss", "RSS", "Resident set size",
    property_mem, compare_uintmax, { .fetch_uintmax = proc_get_rss }
  },
  {
    "rssK", "RSS", "Resident set size (kilobytes)",
    property_memK, compare_uintmax, { .fetch_uintmax = proc_get_rss }
  },
  {
    "rssM", "RSS", "Resident set size (megabytes)",
    property_memM, compare_uintmax, { .fetch_uintmax = proc_get_rss }
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
    "state", "S", "Process state",
    property_char, compare_int, { .fetch_int = proc_get_state }
  },
  {
    "stime", "STIME", "Start time",
    property_stime, compare_intmax, { .fetch_intmax = proc_get_start_time }
  },
  {
    "swap", "SWAP", "Swap usage",
    property_mem, compare_uintmax, { .fetch_uintmax = proc_get_swap }
  },
  {
    "swapK", "SWAP", "Swap usage (1024 bytes)",
    property_memK, compare_uintmax, { .fetch_uintmax = proc_get_swap }
  },
  {
    "swapM", "SWAP", "Swap usage (megabytes)",
    property_memM, compare_uintmax, { .fetch_uintmax = proc_get_swap }
  },
  {
    "time", "TIME", "Scheduled time",
    property_time, compare_intmax, { .fetch_intmax = proc_get_scheduled_time }
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
    "vsz", "VSZ", "Virtual memory used",
    property_mem, compare_uintmax, { .fetch_uintmax = proc_get_vsize }
  },
  {
    "vszK", "VSZ", "Virtual memory used (1024 bytes)",
    property_memK, compare_uintmax, { .fetch_uintmax = proc_get_vsize }
  },
  {
    "vszM", "VSZ", "Virtual memory used (1024 bytes)",
    property_memM, compare_uintmax, { .fetch_uintmax = proc_get_vsize }
  },
  {
    "wchan", "WCHAN", "Wait channel (hex)",
    property_address, compare_uintmax, { .fetch_uintmax = proc_get_wchan }
  },
  {
    "write", "WR", "Recent write rate",
    property_iorate, compare_double, { .fetch_double = proc_get_write_bytes }
  },
  {
    "writeK", "WR", "Recent write rate (1024 bytes/s)",
    property_iorateK, compare_double, { .fetch_double = proc_get_write_bytes }
  },
  {
    "writeM", "WR", "Recent write rate (megabytes/s)",
    property_iorateM, compare_double, { .fetch_double = proc_get_write_bytes }
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
      if(properties[m].description || (flags & FORMAT_INTERNAL))
        return &properties[m];
      else
        return NULL;
    }
  }
  return NULL;
}

int format_set(const char *f, unsigned flags) {
  char buffer[128], heading_buffer[128], *e;
  const char *heading;
  size_t i;
  int q;
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
    while(*f && (*f != ' ' && *f != ',' && *f != '=' && *f != ':')) {
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
      if(flags & FORMAT_QUOTED) {
        /* property=heading extends until we hit a separator
         * property="heading" extends to the close quote; ' is allowed too
         */
        i = 0;
        ++f;
        if(*f == '"' || *f == '\'') {
          q = *f++;
          while(*f && *f != q) {
            /* \ escapes the next character (there must be one) */
            if(*f == '\\') {
              if(f[1] != 0)
                ++f;
              else if(flags & FORMAT_CHECK)
                return 0;
            }
            if(i < sizeof heading_buffer - 1)
              heading_buffer[i++] = *f;
            ++f;
          }
          /* The close quotes must exist */
          if(*f == q)
            ++f;
          else
            if(flags & FORMAT_CHECK)
              return 0;
        } else {
          /* unquoted heading */
          while(*f && (*f != ' ' && *f != ',')) {
            if(i < sizeof heading_buffer - 1)
              heading_buffer[i++] = *f;
            ++f;
          }
        }
        heading_buffer[i] = 0;
        heading = heading_buffer;
      } else {
        /* property=heading extends to the end of the argument, not until
         * the next comma; "The default header can be overridden by
         * appending an <equals-sign> and the new text of the
         * header. The rest of the characters in the argument shall be
         * used as the header text." */
        heading = f + 1;
        f += strlen(f);
      }
    } else
      heading = NULL;
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
      columns[ncolumns].reqwidth = reqwidth;
      ++ncolumns;
    }
  }
  return 1;
}

void format_clear(void) {
  size_t n;
  for(n = 0; n < ncolumns; ++n)
    free(columns[n].heading);
  ncolumns = 0;
  free(columns);
  columns = NULL;
}

void format_columns(struct procinfo *pi, const pid_t *pids, size_t npids) {
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
    for(n = 0; n < npids; ++n) {
      // Render the value to a false buffer to find out how big it is
      struct buffer b[1];
      b->base = 0;
      b->pos = 0;
      b->size = 0;
      columns[c].prop->format(columns[c].prop, b, columns[c].reqwidth,
                              pi, pids[n]);
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

void format_heading(struct procinfo *pi, char *buffer, size_t bufsize) {
  size_t c;
  for(c = 0; c < ncolumns && !*columns[c].heading; ++c)
    ;
  if(c < ncolumns)
    format_process(pi, -1, buffer, bufsize);
  else
    *buffer = 0;
}

void format_process(struct procinfo *pi, pid_t pid,
                    char *buffer, size_t bufsize) {
  size_t w, left, c, start;
  struct buffer b[1];
  b->base = buffer;
  b->pos = 0;
  b->size = bufsize;
  for(c = 0; c < ncolumns; ++c) {
    start = b->pos;
    if(pid == -1)
      buffer_append(b, columns[c].heading);
    else
      columns[c].prop->format(columns[c].prop, b, columns[c].width, pi, pid);
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

void format_value(struct procinfo *pi, pid_t pid,
                  const char *property,
                  char *buffer, size_t bufsize) {
  struct buffer b[1];
  const struct propinfo *prop = find_property(property, 0);
  if(!prop)
    fatal(0, "unknown process property '%s'", property);
  b->base = buffer;
  b->pos = 0;
  b->size = bufsize;
  prop->format(prop, b, SIZE_MAX, pi, pid);
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
  size_t size = 10;
  char *buffer, *ptr;

  for(n = 0; n < norders; ++n)
    size += strlen(orders[n].prop->name) + 5;
  ptr = buffer = xmalloc(size);
  for(n = 0; n < norders; ++n) {
    if(n)
      *ptr++ = ' ';
    *ptr++ = orders[n].sign < 0 ? '+' : '-';
    strcpy(ptr, orders[n].prop->name);
    ptr += strlen(ptr);
  }
  *ptr = 0;
  return buffer;
}

int format_compare(struct procinfo *pi, pid_t a, pid_t b) {
  size_t n;
  int c;
  for(n = 0; n < norders; ++n)
    if((c = orders[n].prop->compare(orders[n].prop, pi, a, b)))
      return orders[n].sign < 0 ? -c : c;
  return 0;
}

char **format_help(void) {
  size_t n = 0, size = 128;
  char *ptr, **result, **next;

  for(n = 0; n < NPROPERTIES; ++n)
    if(properties[n].description)
      size += max(strlen(properties[n].name), 9)
        + max(strlen(properties[n].heading), 7)
        + strlen(properties[n].description)
        + 10;
  next = result = xmalloc(sizeof (char *) * (2 + NPROPERTIES) + size);
  ptr = (char *)(result + 2 + NPROPERTIES);
  *next++ = strcpy(ptr, "  Property   Heading  Description");
  ptr += strlen(ptr) + 1;
  for(n = 0; n < NPROPERTIES; ++n) {
    if(properties[n].description) {
      *next++ = ptr;
      ptr += 1 + sprintf(ptr, "  %-9s  %-7s  %s",
                         properties[n].name,
                         properties[n].heading,
                         properties[n].description);
    }
  }
  *next = NULL;
  return result;
}

char *format_get(void) {
  size_t n;
  size_t size = 10;
  char *buffer, *ptr;
  const char *h;

  for(n = 0; n < ncolumns; ++n) {
    size += strlen(columns[n].prop->name);
    size += strlen(columns[n].prop->heading) * 2;
    size += 20;
  }
  ptr = buffer = xmalloc(size);
  for(n = 0; n < ncolumns; ++n) {
    if(n)
      *ptr++ = ' ';
    strcpy(ptr, columns[n].prop->name);
    ptr += strlen(ptr);
    if(columns[n].reqwidth != SIZE_MAX) {
      ptr += sprintf(ptr, ":%zu", columns[n].reqwidth);
    }
    h = columns[n].heading;
    if(strcmp(h, columns[n].prop->heading)) {
      *ptr++ = '=';
      if(strchr(h, ' ')
         || strchr(h, '"')
         || strchr(h, '\\')
         || strchr(h, ',')) {
        *ptr++ = '"';
        while(*h) {
          if(*h == '"' || *h == '\\')
            *ptr++ = '\\';
          *ptr++ = *h++;
        }
        *ptr++ = '"';
      } else {
        strcpy(ptr, h);
        ptr += strlen(ptr);
      }
    }
  }
  *ptr = 0;
  return buffer;
}

int format_hierarchy;
