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
#include "parse.h"
#include "user.h"
#include <string.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include <linux/sched.h>        /* we want the kernel's SCHED_... not glibc's */
#include <signal.h>

#ifndef SCHED_RESET_ON_FORK
/* Not in older kernels, but we'd like binaries built on old systems
 * to work on newer ones */
# define SCHED_RESET_ON_FORK     0x40000000
#endif

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
  unsigned flags;
  formatfn *format;
  comparefn *compare;
  union {
    int (*fetch_int)(struct procinfo *, taskident);
    intmax_t (*fetch_intmax)(struct procinfo *, taskident);
    uintmax_t (*fetch_uintmax)(struct procinfo *, taskident);
    pid_t (*fetch_pid)(struct procinfo *, taskident);
    uid_t (*fetch_uid)(struct procinfo *, taskident);
    gid_t (*fetch_gid)(struct procinfo *, taskident);
    const gid_t *(*fetch_gids)(struct procinfo *, taskident, size_t *);
    double (*fetch_double)(struct procinfo *, taskident);
    const char *(*fetch_string)(struct procinfo *, taskident);
    void (*fetch_sigset)(struct procinfo *, taskident, sigset_t *);
  } fetch;
};
#define PROP_TEXT 1
#define PROP_NUMERIC 2

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

static enum format_syntax syntax;

void format_syntax(enum format_syntax s) {
  syntax = s;
}

// ----------------------------------------------------------------------------

void format_integer(intmax_t im, struct buffer *b, int base) {
  // For CSV output force decimal
  if(syntax == syntax_csv) {
    switch(base) {
    case 'o': case 'x': case 'X': base = 'u';
    }
  }
  switch(base) {
  case 'o': buffer_printf(b, "%jo", im); break;
  case 'x': buffer_printf(b, "%jx", im); break;
  case 'X': buffer_printf(b, "%jX", im); break;
  case 'u': buffer_printf(b, "%ju", im); break;
  default: buffer_printf(b, "%jd", im); break;
  }
}

void format_addr(uintmax_t im, struct buffer *b) {
  // For CSV output fore decimal
  if(syntax == syntax_csv)
    buffer_printf(b, "%ju", im);
  else {
    if(im > 0xFFFFFFFFFFFFLL)
      buffer_printf(b, "%016jx", im);
    else if(im > 0xFFFFFFFF)
      buffer_printf(b, "%012jx", im);
    else
      buffer_printf(b, "%08jx", im);
  }
}

static void format_usergroup(intmax_t id, struct buffer *b, size_t columnsize,
                             const char *name) {
  if(name && strlen(name) <= columnsize)
    buffer_append(b, name);
  else
    format_integer(id, b,'d');
}

static void format_user(uid_t uid, struct buffer *b, size_t columnsize) {
  format_usergroup(uid, b, columnsize, lookup_user_by_id(uid));
}

static void format_group(gid_t gid, struct buffer *b, size_t columnsize) {
  format_usergroup(gid, b, columnsize, lookup_group_by_id(gid));
}

void format_interval(long seconds, struct buffer *b,
                     int always_hours, size_t columnsize,
                     const char *format,
                     unsigned flags) {
  size_t startpos;
  if((flags & FORMAT_RAW) || syntax == syntax_csv) {
    format_integer(seconds, b, 'd');
    return;
  }
  if(format)
    strfelapsed(b, format, seconds);
  else {
    startpos = b->pos;
    if(always_hours || seconds >= 86400)
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

void format_time(time_t when, struct buffer *b, size_t columnsize,
                 const char *format, unsigned flags) {
  time_t now;
  struct tm when_tm, now_tm;

  if(flags & FORMAT_RAW) {
    format_integer(when, b, 'd');
    return;
  }
  now = timespec_now(NULL);
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

static void property_udecimal(const struct column *col, struct buffer *b,
                             size_t attribute((unused)) columnsize,
                             struct procinfo *pi, taskident task,
                             unsigned attribute((unused)) flags) {
  return format_integer(col->prop->fetch.fetch_intmax(pi, task), b, 'u');
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
  if(pid > 0)
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

static void property_gids(const struct column *col, struct buffer *b,
                          size_t attribute((unused)) columnsize,
                          struct procinfo *pi, taskident task,
                          unsigned attribute((unused)) flags) {
  size_t ngids, n;
  const gid_t *gids = col->prop->fetch.fetch_gids(pi, task, &ngids);
  for(n = 0; n < ngids; ++n) {
    if(n)
      buffer_putc(b, ' ');
    format_integer(gids[n], b, 'd');
  }
}

static void property_groups(const struct column *col, struct buffer *b,
                            size_t attribute((unused)) columnsize,
                            struct procinfo *pi, taskident task,
                            unsigned attribute((unused)) flags) {
  size_t ngids, n;
  const gid_t *gids = col->prop->fetch.fetch_gids(pi, task, &ngids);
  for(n = 0; n < ngids; ++n) {
    if(n)
      buffer_putc(b, ' ');
    format_group(gids[n], b, SIZE_MAX);
  }
}

static void property_char(const struct column *col, struct buffer *b,
                          size_t attribute((unused)) columnsize,
                          struct procinfo *pi, taskident task,
                          unsigned attribute((unused)) flags) {
  buffer_putc(b, col->prop->fetch.fetch_int(pi, task));
}

static void property_sigset(const struct column *col, struct buffer *b,
                            size_t columnsize,
                            struct procinfo *pi, taskident task,
                            unsigned flags) {
  int sig, first;
  size_t start;
  char namebuf[64];
  sigset_t ss;
  if(!(flags & FORMAT_RAW)) {
    start = b->pos;
    sig = first = 1;
    col->prop->fetch.fetch_sigset(pi, task, &ss);
    while(!sigisemptyset(&ss)) {
      if(sigismember(&ss, sig)) {
        if(!first)
          buffer_putc(b, ',');
        else
          first = 0;
        buffer_append(b, signame(sig, namebuf, sizeof namebuf));
        sigdelset(&ss, sig);
      }
      ++sig;
    }
    if(first)
      buffer_putc(b, '-');
    if(b->pos - start <= columnsize)
      return;
    b->pos = start;
  }
  sig = first = 1;
  col->prop->fetch.fetch_sigset(pi, task, &ss);
  while(!sigisemptyset(&ss)) {
    if(sigismember(&ss, sig)) {
      if(!first)
        buffer_putc(b, ',');
      else
        first = 0;
      format_integer(sig, b, 'd');
      sigdelset(&ss, sig);
      if(!(flags & FORMAT_RAW)) {
        if(sigismember(&ss, sig + 1)
           && sigismember(&ss, sig + 2)) {
          while(sigismember(&ss, sig + 1)) {
            ++sig;
            sigdelset(&ss, sig);
          }
          buffer_putc(b, '-');
          format_integer(sig, b, 'd');
        }
      }
    }
    ++sig;
  }
  if(first)
    buffer_putc(b, '-');
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

static const char *shim_get_pcomm(struct procinfo *pi, taskident task) {
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
                          unsigned flags) {
  double pcpu = 100 * col->prop->fetch.fetch_double(pi, task);
  int prec;
  if((flags & FORMAT_RAW) || syntax == syntax_csv)
    buffer_printf(b, "%g", pcpu);
  else if(col->arg && *col->arg) {
    prec = atoi(col->arg);
    buffer_printf(b, "%.*f", prec, pcpu);
  } else
    format_integer(pcpu, b, 'd');
}

static void property_mem(const struct column *col, struct buffer *b,
                         size_t attribute((unused)) columnsize,
                         struct procinfo *pi, taskident task,
                         unsigned flags) {
  char buffer[64];
  unsigned cutoff = 0;
  int ch = (syntax == syntax_csv ? 'b' : parse_byte_arg(col->arg, &cutoff, flags));
  buffer_append(b,
                bytes(col->prop->fetch.fetch_uintmax(pi, task),
                      0, ch, buffer, sizeof buffer, cutoff));
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
  unsigned cutoff = 0;
  int ch = (syntax == syntax_csv ? 'b' : parse_byte_arg(col->arg, &cutoff, flags));
  buffer_append(b,
                bytes(col->prop->fetch.fetch_double(pi, task),
                      0, ch, buffer, sizeof buffer, cutoff));
}

static void property_sched(const struct column *col, struct buffer *b,
                           size_t columnsize,
                           struct procinfo *pi, taskident task,
                           unsigned flags) {
  unsigned policy = col->prop->fetch.fetch_int(pi, task);
  const char *name;
  int reset;
  
  if(policy & SCHED_RESET_ON_FORK) {
    reset = 1;
    policy ^= SCHED_RESET_ON_FORK;
  } else
    reset = 0;
  switch(policy) {
  case SCHED_NORMAL: name = "-"; break;
  case SCHED_BATCH: name = "BATCH"; break;
  case SCHED_IDLE: name = "IDLE"; break;
  case SCHED_FIFO: name = "FIFO"; break;
  case SCHED_RR: name = "RR"; break;
  default: name = NULL; break;
  }
  if(name && strlen(name) <= columnsize && !(flags & FORMAT_RAW))
    buffer_append(b, name);
  else
    format_integer(policy, b, 'd');
  if(reset)
    buffer_append(b, "/-");
}

static intmax_t shim_get_time(struct procinfo *pi,
                              taskident attribute((unused)) task) {
  struct timespec ts;
  proc_time(pi, &ts);
  return ts.tv_sec;
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
  const char *ua = lookup_user_by_id(av), *ub;
  ua = xstrdup(ua ? ua : "");
  int rc;
  ub = lookup_user_by_id(bv);
  rc = strcmp(ua, ub ? ub : "");
  free((char *)ua);
  return rc;
}

static int compare_gid(const struct propinfo *prop, struct procinfo *pi,
                       taskident a, taskident b) {
  gid_t av = prop->fetch.fetch_gid(pi, a);
  gid_t bv = prop->fetch.fetch_gid(pi, b);
  return av < bv ? -1 : av > bv ? 1 : 0;
}

static int compare_gids(const struct propinfo *prop, struct procinfo *pi,
                        taskident a, taskident b) {
  size_t an, bn;
  const gid_t *av = prop->fetch.fetch_gids(pi, a, &an);
  const gid_t *bv = prop->fetch.fetch_gids(pi, b, &bn);
  while(an && bn) {
    if(*av < *bv)
      return -1;
    else if(*av > *bv)
      return 1;
    ++av;
    ++bv;
    --an;
    --bn;
  }
  if(an)
    return 1;
  else if(bn)
    return -1;
  else
    return 0;
}

static int compare_group(const struct propinfo *prop, struct procinfo *pi,
                       taskident a, taskident b) {
  uid_t av = prop->fetch.fetch_uid(pi, a);
  uid_t bv = prop->fetch.fetch_uid(pi, b);
  const char *ga = lookup_group_by_id(av), *gb;
  ga = xstrdup(ga ? ga : "");
  int rc;
  gb = lookup_group_by_id(bv);
  rc = strcmp(ga, gb ? gb : "");
  free((char *)ga);
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
  if(!av)
    av = "";
  if(!bv)
    bv = "";
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

static int compare_sigset(const struct propinfo *prop, struct procinfo *pi,
                          taskident a, taskident b) {
  sigset_t sa, sb;
  int sig, d;

  prop->fetch.fetch_sigset(pi, a, &sa);
  prop->fetch.fetch_sigset(pi, b, &sb);
  sig = 1;
  while(!sigisemptyset(&sa) && !sigisemptyset(&sb)) {
    if((d = sigismember(&sa, sig) - sigismember(&sb, sig)))
      return d;
    sigdelset(&sa, sig);
    sigdelset(&sb, sig);
    ++sig;
  }
  return sigismember(&sa, sig) - sigismember(&sb, sig);
}

// ----------------------------------------------------------------------------

static const struct propinfo properties[] = {
  {
    "%cpu", NULL, "=pcpu", 0, NULL, NULL, {}
  },
  {
    "_hier", NULL, NULL,
    0,
    NULL, compare_hier, { }
  },
  {
    "addr", "ADDR", "Instruction pointer address (hex)",
    PROP_NUMERIC,
    property_address, compare_uintmax, { .fetch_uintmax = proc_get_insn_pointer }
  },
  {
    "args", "COMMAND", "Command with arguments (but path removed)",
    PROP_TEXT,
    property_command_brief, compare_string, { .fetch_string = proc_get_cmdline }
  },
  {
    "argsfull", "COMMAND", "Command with arguments",
    PROP_TEXT,
    property_command, compare_string, { .fetch_string = proc_get_cmdline }
  },
  {
    "cmd", NULL, "=args", 0, NULL, NULL, {}
  },
  {
    "comm", "COMMAND", "Command",
    PROP_TEXT,
    property_command, compare_string, { .fetch_string = proc_get_comm }
  },
  {
    "command", NULL, "=args", 0, NULL, NULL, {}
  },
  {
    "cputime", NULL, "=time", 0, NULL, NULL, {}
  },
  {
    "egid", NULL, "=gid", 0, NULL, NULL, {}
  },
  {
    "egroup", NULL, "=group", 0, NULL, NULL, {}
  },
  {
    "etime", "ELAPSED", "Elapsed time (argument: format string)",
    PROP_NUMERIC,
    property_etime, compare_intmax, { .fetch_intmax = proc_get_elapsed_time }
  },
  {
    "euid", NULL, "=uid", 0, NULL, NULL, {}
  },
  {
    "euser", NULL, "=user", 0, NULL, NULL, {}
  },
  {
    "f", NULL, "=flags", 0, NULL, NULL, {}
  },
  {
    "flag", NULL, "=flags", 0, NULL, NULL, {}
  },
  {
    "flags", "F", "Flags (octal; argument o/d/x/X)",
    PROP_NUMERIC,
    property_uoctal, compare_uintmax, { .fetch_uintmax = proc_get_flags }
  },
  {
    "fsgid", "FSGID", "Filesystem group ID (decimal)",
    PROP_NUMERIC,
    property_gid, compare_gid, { .fetch_gid = proc_get_fsgid }
  },
  {
    "fsgroup", "FSGROUP", "Filesystem group ID (name)",
    PROP_TEXT,
    property_group, compare_group, { .fetch_gid = proc_get_fsgid }
  },
  {
    "fsuid", "FSUID", "Filesysem user ID (decimal)",
    PROP_NUMERIC,
    property_uid, compare_uid, { .fetch_uid = proc_get_fsuid }
  },
  {
    "fsuser", "FSUSER", "Filesystem user ID (name)",
    PROP_NUMERIC,
    property_user, compare_user, { .fetch_uid = proc_get_fsuid }
  },
  {
    "gid", "GID","Effective group ID (decimal)",
    PROP_NUMERIC,
    property_gid, compare_gid, { .fetch_gid = proc_get_egid }
  },
  {
    "group", "GROUP", "Effective group ID (name)",
    PROP_TEXT,
    property_group, compare_group, { .fetch_gid = proc_get_egid }
  },
  {
    "io", "IO", "Recent read+write rate (argument: K/M/G/T/P/p)",
    PROP_NUMERIC,
    property_iorate, compare_double, { .fetch_double = proc_get_rw_bytes }
  },
  {
    "localtime", "LTIME", "Timestamp (argument: strftime format string)",
    PROP_TEXT,
    property_stime, compare_intmax, { .fetch_intmax = shim_get_time },
  },
  {
    "locked", "LCK", "Locked memory (argument: K/M/G/T/P/p)",
    PROP_NUMERIC,
    property_mem, compare_uintmax, { .fetch_uintmax = proc_get_locked }
  },    
  {
    "lwp", NULL, "=tid", 0, NULL, NULL, {}
  },
  {
    "nlwp", NULL, "=threads", 0, NULL, NULL, {}
  },
  {
    "majflt", "+FLT", "Major fault rate (argument: K/M/G/T/P/p)",
    PROP_NUMERIC,
    property_iorate, compare_double, { .fetch_double = proc_get_majflt }
  },
  {
    "mem", "MEM", "Memory usage (argument: K/M/G/T/P/p) ",
    PROP_NUMERIC,
    property_mem, compare_uintmax, { .fetch_uintmax = proc_get_mem }
  },
  {
    "minflt", "-FLT", "Minor fault rate (argument: K/M/G/T/P/p)",
    PROP_NUMERIC,
    property_iorate, compare_double, { .fetch_double = proc_get_minflt }
  },
  {
    "ni", NULL, "=ni", 0, NULL, NULL, {}
  },
  {
    "nice", "NI", "Nice value",
    PROP_NUMERIC,
    property_decimal, compare_intmax, { .fetch_intmax = proc_get_nice }
  },
  {
    "oom", "OOM", "OOM score",
    PROP_NUMERIC,
    property_decimal, compare_intmax, { .fetch_intmax = proc_get_oom_score }
  },
  {
    "pcomm", "PCMD", "Parent command name",
    PROP_TEXT,
    property_command, compare_string, { .fetch_string = shim_get_pcomm },
  },
  {
    "pcpu", "%CPU", "%age CPU used (argument: precision)",
    PROP_NUMERIC,
    property_pcpu, compare_double, { .fetch_double = proc_get_pcpu }
  },
  {
    "pgrp", "PGRP", "Process group ID",
    PROP_NUMERIC,
    property_pid, compare_pid, { .fetch_pid = proc_get_pgrp }
  },
  {
    "pgrp", NULL, "=pgid", 0, NULL, NULL, {}
  },
  {
    "pid", "PID", "Process ID",
    PROP_NUMERIC,
    property_pid, compare_pid, { .fetch_pid = proc_get_pid }
  },
  {
    "pinned", "PIN", "Pinned memory (argument: K/M/G/T/P/p)",
    PROP_NUMERIC,
    property_mem, compare_uintmax, { .fetch_uintmax = proc_get_pinned }
  },    
  {
    "pmem", "PMEM", "Proportional memory usage (argument: K/M/G/T/P/p)",
    PROP_NUMERIC,
    property_mem, compare_uintmax, { .fetch_uintmax = proc_get_pmem }
  },
  {
    "ppid", "PPID", "Parent process ID",
    PROP_NUMERIC,
    property_pid, compare_pid, { .fetch_pid = proc_get_ppid }
  },
  {
    "pri", "PRI", "Priority",
    PROP_NUMERIC,
    property_decimal, compare_intmax, { .fetch_intmax = proc_get_priority }
  },
  {
    "pss", "PSS", "Proportional resident set size (argument: K/M/G/T/P/p)",
    PROP_NUMERIC,
    property_mem, compare_uintmax, { .fetch_uintmax = proc_get_pss }
  },
  {
    "pte", "PTE", "Page table memory (argument: K/M/G/T/P/p)",
    PROP_NUMERIC,
    property_mem, compare_uintmax, { .fetch_uintmax = proc_get_pte }
  },    
  {
    "read", "RD", "Recent read rate (argument: K/M/G/T/P/p)",
    PROP_NUMERIC,
    property_iorate, compare_double, { .fetch_double = proc_get_read_bytes }
  },
  {
    "rgid", "RGID", "Real group ID (decimal)",
    PROP_NUMERIC,
    property_gid, compare_gid, { .fetch_gid = proc_get_rgid }
  },
  {
    "rgroup", "RGROUP", "Real group ID (name)",
    PROP_TEXT,
    property_group, compare_group, { .fetch_gid = proc_get_rgid }
  },
  {
    "rss", "RSS", "Resident set size (argument: K/M/G/T/P/p)",
    PROP_NUMERIC,
    property_mem, compare_uintmax, { .fetch_uintmax = proc_get_rss }
  },
  {
    "rsspk", "RSSPK", "Peak resident set size (argument: K/M/G/T/P/p)",
    PROP_NUMERIC,
    property_mem, compare_uintmax, { .fetch_uintmax = proc_get_peak_rss }
  },
  {
    "rssize", NULL, "=rss", 0, NULL, NULL, {},
  },
  {
    "rsz", NULL, "=rss", 0, NULL, NULL, {},
  },
  {
    "rtprio", "RTPRI", "Realtime scheduling priority",
    PROP_NUMERIC,
    property_udecimal, compare_uintmax, { .fetch_uintmax = proc_get_rtprio }
  },
  {
    "ruid", "RUID", "Real user ID (decimal)",
    PROP_NUMERIC,
    property_uid, compare_uid, { .fetch_uid = proc_get_ruid }
  },
  {
    "ruser", "RUSER", "Real user ID (name)",
    PROP_TEXT,
    property_user, compare_user, { .fetch_uid = proc_get_ruid }
  },
  {
    "sched", "SCHED", "Scheduling policy",
    PROP_NUMERIC,
    property_sched, compare_int, { .fetch_int = proc_get_sched_policy }
  },
  {
    "sess", NULL, "=sid", 0, NULL, NULL, {},
  },
  {
    "session", NULL, "=sid", 0, NULL, NULL, {},
  },
  {
    "sgid", "SGID", "Saved group ID (decimal)",
    PROP_NUMERIC,
    property_gid, compare_gid, { .fetch_gid = proc_get_sgid }
  },
  {
    "sgroup", "SGROUP", "Saved group ID (name)",
    PROP_TEXT,
    property_group, compare_group, { .fetch_gid = proc_get_sgid }
  },
  {
    "sid", "SID", "Session ID",
    PROP_NUMERIC,
    property_pid, compare_pid, { .fetch_pid = proc_get_session }
  },
  {
    "sigblocked", "BLOCKED", "Blocked signals",
    PROP_TEXT,
    property_sigset, compare_sigset, { .fetch_sigset = proc_get_sig_blocked },
  },
  {
    "sigcaught", "CAUGHT", "Caught signals",
    PROP_TEXT,
    property_sigset, compare_sigset, { .fetch_sigset = proc_get_sig_caught },
  },
  {
    "sigignored", "IGNORED", "Ignored signals",
    PROP_TEXT,
    property_sigset, compare_sigset, { .fetch_sigset = proc_get_sig_ignored },
  },
  {
    "sigpending", "PENDING", "Pending signals",
    PROP_TEXT,
    property_sigset, compare_sigset, { .fetch_sigset = proc_get_sig_pending },
  },
  {
    "stack", "STK", "Stack size (argument: K/M/G/T/P/p)",
    PROP_NUMERIC,
    property_mem, compare_uintmax, { .fetch_uintmax = proc_get_stack }
  },    
  {
    "state", "S", "Process state",
    PROP_TEXT,
    property_char, compare_int, { .fetch_int = proc_get_state }
  },
  {
    "stime", "STIME", "Start time (argument: strftime format string)",
    PROP_TEXT,
    property_stime, compare_intmax, { .fetch_intmax = proc_get_start_time }
  },
  {
    "suid", "SUID", "Saved user ID (decimal)",
    PROP_NUMERIC,
    property_uid, compare_uid, { .fetch_uid = proc_get_suid }
  },
  {
    "supgid", "SUPGID", "Supplementary group IDs (decimal)",
    PROP_TEXT,
    property_gids, compare_gids, { .fetch_gids = proc_get_supgids }
  },
  {
    "supgrp", "SUPGRP", "Supplementary group IDs (names)",
    PROP_TEXT,
    property_groups, compare_gids, { .fetch_gids = proc_get_supgids }
  },
  {
    "suser", "SUSER", "Saved user ID (name)",
    PROP_TEXT,
    property_user, compare_user, { .fetch_uid = proc_get_suid }
  },
  {
    "swap", "SWAP", "Swap usage (argument: K/M/G/T/P/p)",
    PROP_NUMERIC,
    property_mem, compare_uintmax, { .fetch_uintmax = proc_get_swap }
  },
  {
    "thcount", NULL, "=threads", 0, NULL, NULL, {}
  },
  {
    "threads", "T", "Number of threads",
    PROP_NUMERIC,
    property_num_threads, compare_pid, { .fetch_int = proc_get_num_threads }
  },
  {
    "tid", "TID", "Thread ID",
    PROP_NUMERIC,
    property_pid, compare_pid, { .fetch_pid = proc_get_tid }
  },
  {
    "time", "TIME", "Scheduled time (argument: format string)",
    PROP_TEXT,
    property_time, compare_intmax, { .fetch_intmax = proc_get_scheduled_time }
  },
  {
    "tname", NULL, "=tty", 0, NULL, NULL, {}
  },
  {
    "tpgid", "TPGID", "Foreground progress group on controlling terminal",
    PROP_NUMERIC,
    property_pid, compare_pid, { .fetch_pid = proc_get_tpgid }
  },
  {
    "tt", NULL, "=tty", 0, NULL, NULL, {}
  },
  {
    "tty", "TT", "Terminal",
    PROP_TEXT,
    property_tty, compare_int, { .fetch_int = proc_get_tty }
  },
  {
    "uid", "UID", "Effective user ID (decimal)",
    PROP_NUMERIC,
    property_uid, compare_uid, { .fetch_uid = proc_get_euid }
  },
  {
    "user", "USER", "Effective user ID (name)",
    PROP_TEXT,
    property_user, compare_user, { .fetch_uid = proc_get_euid }
  },
  {
    "vsize", NULL, "=vsz", 0, NULL, NULL, {}
  },
  {
    "vsz", "VSZ", "Virtual memory used (argument: K/M/G/T/P/p)",
    PROP_NUMERIC,
    property_mem, compare_uintmax, { .fetch_uintmax = proc_get_vsize }
  },
  {
    "vszpk", "VSZPK", "Peak virtual memory used (argument: K/M/G/T/P/p)",
    PROP_NUMERIC,
    property_mem, compare_uintmax, { .fetch_uintmax = proc_get_peak_vsize }
  },
  {
    "wchan", "WCHAN", "Wait channel (hex)",
    PROP_NUMERIC,
    property_address, compare_uintmax, { .fetch_uintmax = proc_get_wchan }
  },
  {
    "write", "WR", "Recent write rate (argument: K/M/G/T/P/p)",
    PROP_NUMERIC,
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
  char *name, *heading, *arg;
  const struct propinfo *prop;
  size_t reqwidth;
  enum parse_status ps;
  if(!(flags & (FORMAT_CHECK|FORMAT_ADD)))
    format_clear();
  while((reqwidth = SIZE_MAX),
        !(ps = parse_element(&f, NULL, &name, &reqwidth, &heading, &arg,
                             flags|FORMAT_SIZE|FORMAT_HEADING|FORMAT_ARG))) {
    prop = find_property(name, flags);
    if(flags & FORMAT_CHECK) {
      free(name);
      free(heading);
      free(arg);
      if(!prop)
        return 0;
    } else {
      if(!prop)
        fatal(0, "unknown process property '%s'", name);
      if((ssize_t)(ncolumns + 1) < 0)
        fatal(0, "too many columns");
      columns = xrecalloc(columns, ncolumns + 1, sizeof *columns);
      memset(&columns[ncolumns], 0, sizeof *columns);
      columns[ncolumns].prop = prop;
      columns[ncolumns].heading = heading ? heading 
                                     : xstrdup(columns[ncolumns].prop->heading);
      columns[ncolumns].arg = arg ? arg : NULL;
      columns[ncolumns].reqwidth = reqwidth;
      ++ncolumns;
      free(name);
    }
  }
  return ps == parse_eof;
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
  struct buffer bb[1];
  size_t i, c;
  ssize_t left;
  int ch;
  b->pos = 0;
  buffer_init(bb);
  for(c = 0; c < ncolumns; ++c) {
    /* Render the value or heading */
    bb->pos = 0;
    /* Emit it in the chosen syntax */
    if(task.pid == -1)
      buffer_append(bb, columns[c].heading);
    else
      columns[c].prop->format(&columns[c], bb, columns[c].width, pi, task, 0);
    switch(syntax) {
    case syntax_normal:
      buffer_append_n(b, bb->base, bb->pos);
      /* For non-final columns, pad to the column width and one more for
       * the column separator */
      if(c + 1 < ncolumns) {
        left = 1 + columns[c].width - bb->pos;
        while(left-- > 0)
          buffer_putc(b, ' ');
      }
      break;
    case syntax_csv:
      if(c > 0)
        buffer_putc(b, ',');
      if(columns[c].prop->flags & PROP_TEXT) {
        buffer_putc(b, '"');
        for(i = 0; i < bb->pos; ++i) {
          ch = bb->base[i];
          if(ch == '"')
            buffer_putc(b, '"');
          buffer_putc(b, ch);
        }
        buffer_putc(b, '"');
      } else {
        buffer_append_n(b, bb->base, bb->pos);
      }
      break;
    }
  }
  buffer_terminate(b);
  free(bb->base);
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
  char *name;
  const struct propinfo *prop;
  int sign;
  enum parse_status ps;
  if(!(flags & (FORMAT_CHECK|FORMAT_ADD))) {
    free(orders);
    orders = NULL;
    norders = 0;
  }
  while(!(ps = parse_element(&ordering, &sign, &name, NULL, NULL, NULL,
                             flags|FORMAT_QUOTED|FORMAT_SIGN))) {
    switch(sign) {
    case '+': sign = -1; break;
    case '-': sign = 1; break;
    case 0: sign = -1; break;
    }
    prop = find_property(name, flags);
    if(flags & FORMAT_CHECK) {
      free(name);
      if(!prop)
        return 0;
    } else {
      if(!prop)
        fatal(0, "unknown process property '%s'", name);
      if((ssize_t)(norders + 1) < 0)
        fatal(0, "too many columns");
      orders = xrecalloc(orders, norders + 1, sizeof *orders);
      orders[norders].prop = prop;
      orders[norders].sign = sign;
      ++norders;
      free(name);
    }
  }
  return ps == parse_eof;
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
      xasprintf(&ptr, "  %-9s  %-7s  %s",
                properties[n].name,
                properties[n].heading,
                properties[n].description);
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

int format_rate(struct procinfo *pi, unsigned procflags) {
  size_t n, i;
  int rate = 1;
  taskident *tasks = NULL;
  size_t ntasks;
  struct buffer b[1];
  
  buffer_init(b);
  for(n = 0; n < ncolumns; ++n) {
    if(columns[n].prop->format == property_pcpu
       || columns[n].prop->format == property_iorate) {
      rate = 1;
      if(!tasks)
        tasks = proc_get_all(pi, &ntasks, procflags);
      for(i = 0; i < ntasks; ++i) {
        b->pos = 0;
        columns[n].prop->format(&columns[n], b, SIZE_MAX, pi, tasks[i], 
                                FORMAT_RAW);
      }
    }
  }
  free(b->base);
  free(tasks);
  return rate;
}

int format_hierarchy;
