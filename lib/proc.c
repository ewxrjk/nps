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
#include "process.h"
#include "utils.h"
#include "selectors.h"
#include <stdio.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <inttypes.h>

// TODO the current strategy generates an awful lot of memory
// allocations - perhaps we can do better.

struct property {
  char *name;
  char *value;
};

struct process {
  pid_t pid;                    /* process ID */
  unsigned selected:1;          /* nonzero if selected */
  unsigned stat:1;              /* nonzero if proc_stat() called */
  unsigned status:1;            /* nonzero if proc_status() called */
  unsigned cmdline:1;           /* nonzero if proc_cmdline() called */
  unsigned sorted:1;            /* nonzero if properties sorted */
  unsigned vanished:1;          /* nonzero if process vanished */
  size_t nprops, npropslots;
  struct property *props;
  size_t link;                  /* hash table linkage */
};

#define HASH_SIZE 256           /* hash table size */

static size_t nprocs, nslots;
static struct process *procs;
static size_t lookup[HASH_SIZE];

// ----------------------------------------------------------------------------

static uintmax_t conv(const char *s) {
  return s ? strtoumax(s, NULL, 10) : 0;
}

// ----------------------------------------------------------------------------

#if 0
static int compare_proc(const void *av, const void *bv) {
  const struct process *a = av, *b = bv;
  if(a->pid < b->pid)
    return -1;
  else if(a->pid > b->pid)
    return 1;
  else
    return 0;
}
#endif

static void proc_clear(void) {
  size_t n;
  for(n = 0; n < nprocs; ++n) {
    size_t i;
    for(i = 0; i < procs[n].nprops; ++i) {
      free(procs[n].props[i].name);
      free(procs[n].props[i].value);
    }
    free(procs[n].props);
  }
  nprocs = 0;
}

void proc_enumerate(void) {
  DIR *dp;
  struct dirent *de;
  size_t n;

  /* Ditch any existing data */
  proc_clear();
  /* Look through /proc for process information */
  if(!(dp = opendir("/proc")))
    fatal(errno, "opening /proc");
  errno = 0;
  while((de = readdir(dp))) {
    errno = 0;
    /* Only consider files that look like processes */
    if(strspn(de->d_name, "0123456789") == strlen(de->d_name)) {
      /* Make sure the array is big enough */
      if(nprocs >= nslots) {
        if((ssize_t)(nslots = nslots ? 2 * nslots : 64) <= 0)
          fatal(0, "too many processes");
        procs = xrecalloc(procs, nslots, sizeof *procs);
      }
      memset(&procs[nprocs], 0, sizeof *procs);
      procs[nprocs++].pid = conv(de->d_name);
    }
  }
  if(errno)
    fatal(errno, "reading /proc");
  closedir(dp);
  for(n = 0; n < HASH_SIZE; ++n)
    lookup[n] = SIZE_MAX;
  for(n = 0; n < nprocs; ++n) {
    size_t h = procs[n].pid % HASH_SIZE;
    if(lookup[h] != SIZE_MAX)
      procs[n].link = lookup[h];
    lookup[h] = n;
  }
  for(n = 0; n < nprocs; ++n) {
    procs[n].selected = select_test(procs[n].pid);
  }
}

// ----------------------------------------------------------------------------

static struct process *proc_find(pid_t pid) {
  size_t n = lookup[pid % HASH_SIZE];
  while(n != SIZE_MAX && procs[n].pid != pid)
    n = procs[n].link;
  return n != SIZE_MAX ? &procs[n] : NULL;
}

static void proc_setprop(struct process *p,
                         const char *name,
                         const char *value) {
  if(p->nprops >= p->npropslots) {
    if((ssize_t)(p->npropslots = p->npropslots ? 2 * p->npropslots : 64) <= 0)
      fatal(0, "too many process properties");
    p->props = xrecalloc(p->props, p->npropslots, sizeof *p->props);
  }
  p->props[p->nprops].name = xstrdup(name);
  p->props[p->nprops].value = xstrdup(value);
  ++p->nprops;
  p->sorted = 0;
}

static int proc_delprop(struct process *p,
                        const char *name) {
  size_t n;
  for(n = 0; n < p->nprops && strcmp(name, p->props[n].name); ++n)
    ;
  if(n < p->nprops) {
    free(p->props[n].name);
    free(p->props[n].value);
    memmove(&p->props[n], &p->props[n+1],
            ((p->nprops - n) - 1) * sizeof *p->props);
    --p->nprops;
    return 1;
  }
  return 0;
}

static int compare_prop(const void *av, const void *bv) {
  const struct property *a = av, *b = bv;

  return strcmp(a->name, b->name);
}

static const char *proc_getprop(struct process *p, const char *name) {
  ssize_t l = 0, r = p->nprops - 1, m;
  int c;
  if(!p->sorted) {
    qsort(p->props, p->nprops, sizeof *p->props, compare_prop);
    p->sorted = 1;
  }
  while(l <= r) {
    m = l + (r - l) / 2;
    c = strcmp(name, p->props[m].name);
    if(c < 0)
      r = m - 1;
    else if(c > 0)
      l = m + 1;
    else
      return p->props[m].value;
  }
  return NULL;
}

// ----------------------------------------------------------------------------

static void proc_stat(struct process *p) {
  FILE *fp;
  char buffer[1024];
  int c;
  size_t field, i;

  /* proc/PID/stat fields in order */
  static const char *fields[] = {
    "pid",
    "comm",
    "state",
    "ppid",
    "pgrp",
    "session",
    "tty_nr",
    "tpgid",
    "flags",
    "minflt",
    "cminflt",
    "majflt",
    "cmajflt",
    "utime",
    "stime",
    "cutime",
    "cstime",
    "priority",
    "nice",
    "num_threads",
    "itrealvalue",
    "starttime",
    "vsize",
    "rss",
    "rsslim",
    "startcode",
    "endcode",
    "startstack",
    "kstkesp",
    "kstkeip",
    "signal",
    "blocked",
    "sigignore",
    "sigcatch",
    "wchan",
    "nswap",
    "cnswap",
    "exit_signal",
    "processor",
    "rt_priority",
    "policy",
    "delayacct_blkio_ticks",
  };
  static const size_t nfields = sizeof fields / sizeof *fields;

  if(p->stat || p->vanished)
    return;
  p->stat = 1;
  snprintf(buffer, sizeof buffer, "/proc/%ld/stat", (long)p->pid);
  if(!(fp = fopen(buffer, "r"))) {
    p->vanished = 1;
    return;
  }
  field = i = 0;
  while((c = getc(fp)) != EOF) {
    if(c != ' ' && c != '\n') {
      if(field == 1 && i == 0 && c == '(')
        continue;               /* special-case comm */
      if(i < sizeof buffer - 1)
        buffer[i++] = c;
    } else {
      if(field == 1 && i > 0 && buffer[i - 1] == ')')
        --i;                    /* special-case comm */
      buffer[i] = 0;
      if(field < nfields)
        proc_setprop(p, fields[field], buffer);
      i = 0;
      ++field;
    }
  }
  fclose(fp);
}

static void proc_status(struct process *p) {
  char buffer[1024], *s;
  size_t i;
  FILE *fp;
  int c;

  if(p->status || p->vanished)
    return;
  p->status = 1;
  snprintf(buffer, sizeof buffer, "/proc/%ld/status", (long)p->pid);
  if(!(fp = fopen(buffer, "r"))) {
    p->vanished = 1;
    return;
  }
  i = 0;
  while((c = getc(fp)) != EOF) {
    if(c != '\n') {
      if(i < sizeof buffer - 1)
        buffer[i++] = c;
    } else {
      buffer[i] = 0;
      if((s = strchr(buffer, ':'))) {
        *s++ = 0;
        while(*s && (*s == ' ' || *s == '\t'))
          ++s;
        proc_setprop(p, buffer, s);
      }
      i = 0;
    }
  }
  fclose(fp);
}

static void proc_cmdline(struct process *p) {
  char buffer[1024];
  size_t i;
  FILE *fp;
  int c;

  if(p->cmdline || p->vanished)
    return;
  p->cmdline = 1;
  snprintf(buffer, sizeof buffer, "/proc/%ld/cmdline", (long)p->pid);
  if(!(fp = fopen(buffer, "r"))) {
    p->vanished = 1;
    return;
  }
  i = 0;
  while((c = getc(fp)) != EOF) {
    if(!c)
      c = ' ';
    if(i < sizeof buffer - 1)
      buffer[i++] = c;
  }
  fclose(fp);
  buffer[i] = 0;
  proc_setprop(p, "cmdline", buffer);
}

// ----------------------------------------------------------------------------

pid_t proc_get_session(pid_t pid) {
  struct process *p = proc_find(pid);
  const char *s;

  proc_stat(p);
  if((s = proc_getprop(p, "session")) && *s)
    return conv(s);
  return -1;
}

uid_t proc_get_ruid(pid_t pid) {
  struct process *p = proc_find(pid);
  const char *s;

  proc_status(p);
  if((s = proc_getprop(p, "Uid")) && *s)
    return conv(s);
  return -1;
}

uid_t proc_get_euid(pid_t pid) {
  struct process *p = proc_find(pid);
  const char *s;

  proc_status(p);
  if((s = proc_getprop(p, "Uid"))) {
    while(*s && *s != ' ' && *s != '\t')
      ++s;
    if(*s)
      return conv(s);
  }
  return -1;
}

gid_t proc_get_rgid(pid_t pid) {
  struct process *p = proc_find(pid);
  const char *s;

  proc_status(p);
  if((s = proc_getprop(p, "Gid")) && *s)
    return conv(s);
  return -1;
}

gid_t proc_get_egid(pid_t pid) {
  struct process *p = proc_find(pid);
  const char *s;

  proc_status(p);
  if((s = proc_getprop(p, "Gid"))) {
    while(*s && *s != ' ' && *s != '\t')
      ++s;
    if(*s)
      return conv(s);
  }
  return -1;
}

pid_t proc_get_ppid(pid_t pid) {
  struct process *p = proc_find(pid);
  const char *s;

  proc_stat(p);
  if((s = proc_getprop(p, "ppid")))
    return conv(s);
  return -1;
}

pid_t proc_get_pgid(pid_t pid) {
  struct process *p = proc_find(pid);
  const char *s;

  proc_status(p);
  if((s = proc_getprop(p, "ptpgd")))
    return conv(s);
  return -1;
}

int proc_get_tty(pid_t pid) {
  struct process *p = proc_find(pid);
  const char *s;

  proc_stat(p);
  if((s = proc_getprop(p, "tty_nr")) && *s)
    return atoi(s);
  return 0;
}

const char *proc_get_comm(pid_t pid) {
  struct process *p = proc_find(pid);

  proc_stat(p);
  return proc_getprop(p, "comm");
}

const char *proc_get_cmdline(pid_t pid) {
  struct process *p = proc_find(pid);
  const char *s;
  char *t;

  proc_cmdline(p);
  if((s = proc_getprop(p, "cmdline")) && *s)
    return s;
  /* "Failing this, the command name, as it would appear without the
   * option -f, is written in square brackets. */
  if(asprintf(&t, "[%s]", proc_getprop(p, "comm")) < 0)
    fatal(errno, "asprintf");
  proc_delprop(p, "cmdline");
  proc_setprop(p, "cmdline", t);
  free(t);
  return proc_getprop(p, "cmdline");
}

long proc_get_scheduled_time(pid_t pid) {
  struct process *p = proc_find(pid);
  
  proc_stat(p);
  return clock_to_seconds(conv(proc_getprop(p, "utime"))
                          + conv(proc_getprop(p, "stime")));
}

long proc_get_elapsed_time(pid_t pid) {
  struct process *p = proc_find(pid);
  const char *s;
  char t[64];
  
  proc_stat(p);
  /* We have to return consistent values, otherwise the column size
   * computation becomes inconsistent */
  s = proc_getprop(p, "__elapsed__");
  if(s)
    return conv(s);
  snprintf(t, sizeof t, "%ld",
           (long)(time(NULL) 
                  - clock_to_time(conv(proc_getprop(p, "starttime")))));
  proc_setprop(p, "__elapsed__", t);
  return conv(t);
}

time_t proc_get_start_time(pid_t pid) {
  struct process *p = proc_find(pid);
  proc_stat(p);
  return clock_to_time(conv(proc_getprop(p, "starttime")));
}

uintmax_t proc_get_flags(pid_t pid) {
  struct process *p = proc_find(pid);
  proc_stat(p);
  return conv(proc_getprop(p, "flags"));
}

long proc_get_nice(pid_t pid) {
  struct process *p = proc_find(pid);
  proc_stat(p);
  return conv(proc_getprop(p, "nice"));
}

long proc_get_priority(pid_t pid) {
  struct process *p = proc_find(pid);
  proc_stat(p);
  return conv(proc_getprop(p, "priority"));
}

int proc_get_state(pid_t pid) {
  struct process *p = proc_find(pid);
  const char *s;
  proc_stat(p);
  s = proc_getprop(p, "state");
  return s && *s ? *s : '?';
}

int proc_get_pcpu(pid_t pid) {
  /* TODO PCPU is supposed to describe "recent" usage; we actually
   * return the process's %cpu usage over its entire history, which is
   * really stretching the definition.  The fix is to sample it twice
   * but we are not currently set up for that. */
  struct process *p = proc_find(pid);
  proc_stat(p);
  long scheduled = proc_get_scheduled_time(pid);
  long elapsed = proc_get_elapsed_time(pid);
  return elapsed ? scheduled * 100 / elapsed : 0;
}

uintmax_t proc_get_vsize(pid_t pid) {
  struct process *p = proc_find(pid);
  proc_stat(p);
  return conv(proc_getprop(p, "vsize"));
}

uintmax_t proc_get_rss(pid_t pid) {
  struct process *p = proc_find(pid);
  proc_stat(p);
  return conv(proc_getprop(p, "rss")) * sysconf(_SC_PAGESIZE);
}

uintmax_t proc_get_insn_pointer(pid_t pid) {
  struct process *p = proc_find(pid);
  proc_stat(p);
  return conv(proc_getprop(p, "kstkeip"));
}

uintmax_t proc_get_wchan(pid_t pid) {
  struct process *p = proc_find(pid);
  proc_stat(p);
  return conv(proc_getprop(p, "wchan"));
}

// ----------------------------------------------------------------------------

pid_t *proc_get_selected(size_t *npids) {
  size_t n, count = 0;
  pid_t *pids;
  for(n = 0; n < nprocs; ++n)
    if(procs[n].selected && !procs[n].vanished)
      ++count;
  pids = xrecalloc(NULL, count, sizeof *pids);
  count = 0;
  for(n = 0; n < nprocs; ++n)
    if(procs[n].selected && !procs[n].vanished)
      pids[count++] = procs[n].pid;
  *npids = count;
  return pids; 
}
