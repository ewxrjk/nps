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
#include <stddef.h>
#include <sys/stat.h>

/* Know /proc/$PID/stat fields (after the first three); S for signed
 * and U for unsigned. */
#define STAT_PROPS(U,S) S(ppid)                 \
  S(pgrp)                                       \
  S(session)                                    \
  S(tty_nr)                                     \
  S(tpgid)                                      \
  U(flags)                                      \
  U(minflt)                                     \
  U(cminflt)                                    \
  U(majflt)                                     \
  U(cmajflt)                                    \
  U(utime)                                      \
  U(stime)                                      \
  S(cutime)                                     \
  S(cstime)                                     \
  S(priority)                                   \
  S(nice)                                       \
  S(num_threads)                                \
  S(itrealvalue)                                \
  U(starttime)                                  \
  U(vsize)                                      \
  U(rss)                                        \
  U(rsslim)                                     \
  U(startcode)                                  \
  U(endcode)                                    \
  U(startstack)                                 \
  U(kstkesp)                                    \
  U(kstkeip)                                    \
  U(signal)                                     \
  U(blocked)                                    \
  U(sigignore)                                  \
  U(sigcatch)                                   \
  U(wchan)                                      \
  U(nswap)                                      \
  U(cnswap)                                     \
  S(exit_signal)                                \
  S(processor)                                  \
  U(rt_priority)                                \
  U(policy)                                     \
  U(delayacct_blkio_ticks)                      \
  U(guest_time)                                 \
  S(cguest_time)

#define SMEMBER(X) intmax_t prop_##X;
#define UMEMBER(X) uintmax_t prop_##X;
#define OFFSET(X) offsetof(struct process, prop_##X),

struct process {
  pid_t pid;                    /* process ID */
  size_t link;                  /* hash table linkage */
  unsigned selected:1;          /* nonzero if selected */
  unsigned stat:1;              /* nonzero if proc_stat() called */
  unsigned status:1;            /* nonzero if proc_status() called */
  unsigned sorted:1;            /* nonzero if properties sorted */
  unsigned vanished:1;          /* nonzero if process vanished */
  unsigned elapsed_set:1;       /* nonzero if elapsed has been set */
  unsigned eid_set:1;           /* nonzero if e[ug]id have been set */
  char *prop_comm;
  char *prop_cmdline;
  int prop_state;
  intmax_t elapsed;
  uid_t prop_euid, prop_ruid;
  gid_t prop_egid, prop_rgid;
  STAT_PROPS(SMEMBER,UMEMBER)
};

static const size_t propinfo_stat[] = {
  STAT_PROPS(OFFSET,OFFSET)
};

#define HASH_SIZE 256           /* hash table size */

struct procinfo {
  size_t nprocs, nslots;
  struct process *procs;
  size_t lookup[HASH_SIZE];
};

// ----------------------------------------------------------------------------

static uintmax_t conv(const char *s) {
  return s ? strtoumax(s, NULL, 10) : 0;
}

// ----------------------------------------------------------------------------

void proc_free(struct procinfo *pi) {
  size_t n;
  for(n = 0; n < pi->nprocs; ++n) {
    free(pi->procs[n].prop_comm);
    free(pi->procs[n].prop_cmdline);
  }
  free(pi->procs);
  free(pi);
}

struct procinfo *proc_enumerate(void) {
  DIR *dp;
  struct dirent *de;
  size_t n;
  struct procinfo *pi;

  pi = xmalloc(sizeof *pi);
  memset(pi, 0, sizeof *pi);
  /* Look through /proc for process information */
  if(!(dp = opendir("/proc")))
    fatal(errno, "opening /proc");
  errno = 0;
  while((de = readdir(dp))) {
    errno = 0;
    /* Only consider files that look like processes */
    if(strspn(de->d_name, "0123456789") == strlen(de->d_name)) {
      /* Make sure the array is big enough */
      if(pi->nprocs >= pi->nslots) {
        if((ssize_t)(pi->nslots = pi->nslots ? 2 * pi->nslots : 64) <= 0)
          fatal(0, "too many processes");
        pi->procs = xrecalloc(pi->procs, pi->nslots, sizeof *pi->procs);
      }
      memset(&pi->procs[pi->nprocs], 0, sizeof *pi->procs);
      pi->procs[pi->nprocs++].pid = conv(de->d_name);
    }
  }
  if(errno)
    fatal(errno, "reading /proc");
  closedir(dp);
  for(n = 0; n < HASH_SIZE; ++n)
    pi->lookup[n] = SIZE_MAX;
  for(n = 0; n < pi->nprocs; ++n) {
    size_t h = pi->procs[n].pid % HASH_SIZE;
    if(pi->lookup[h] != SIZE_MAX)
      pi->procs[n].link = pi->lookup[h];
    pi->lookup[h] = n;
  }
  for(n = 0; n < pi->nprocs; ++n) {
    pi->procs[n].selected = select_test(pi, pi->procs[n].pid);
  }
  return pi;
}

// ----------------------------------------------------------------------------

static struct process *proc_find(const struct procinfo *pi, pid_t pid) {
  size_t n = pi->lookup[pid % HASH_SIZE];
  while(n != SIZE_MAX && pi->procs[n].pid != pid)
    n = pi->procs[n].link;
  return n != SIZE_MAX ? &pi->procs[n] : NULL;
}

// ----------------------------------------------------------------------------

static void proc_stat(struct process *p) {
  FILE *fp;
  char buffer[1024];
  int c;
  size_t field, i;
  uintmax_t *ptr;
  struct stat sb;

  if(p->stat || p->vanished)
    return;
  p->stat = 1;
  snprintf(buffer, sizeof buffer, "/proc/%ld/stat", (long)p->pid);
  if(!(fp = fopen(buffer, "r"))) {
    p->vanished = 1;
    return;
  }
  /* The owner/group of the file are the euid/egid of the process,
   * except that for some ("undumpable") processes it is forced to 0.
   * (See kernel change 87bfbf679ffb1e95dd9ada694f66aafc4bfa5959 for
   * discussion.) */
  if(!p->eid_set
     && fstat(fileno(fp), &sb) >= 0
     && (sb.st_uid || sb.st_gid)) {
    p->prop_euid = sb.st_uid;
    p->prop_egid = sb.st_gid;
    p->eid_set = 1;
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
      switch(field) {
      case 0:                   /* pid */
        break;
      case 1:                   /* comm */
        p->prop_comm = xstrdup(buffer);
        break;
      case 2:                   /* state */
        p->prop_state = buffer[0];
        break;
      default:
        ptr = (uintmax_t *)((char *)p + propinfo_stat[field - 3]);
        *ptr = strtoumax(buffer, NULL, 10);
        break;
      }
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
  long e, r;

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
        if(!strcmp(buffer, "Uid") && sscanf(s, "%ld %ld", &e, &r) == 2) {
          p->prop_euid = e;
          p->prop_ruid = r;
        } else if(!strcmp(buffer, "Gid") && sscanf(s, "%ld %ld", &e, &r) == 2){
          p->prop_egid = e;
          p->prop_rgid = r;
        }
      }
      i = 0;
    }
  }
  p->eid_set = 1;
  fclose(fp);
}

static void proc_cmdline(struct process *p) {
  char buffer[1024];
  size_t i;
  FILE *fp;
  int c;

  if(p->prop_cmdline || p->vanished)
    return;
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
  p->prop_cmdline = xstrdup(buffer);
}

// ----------------------------------------------------------------------------

pid_t proc_get_session(struct procinfo *pi, pid_t pid) {
  struct process *p = proc_find(pi, pid);

  proc_stat(p);
  return p->prop_session;
}

uid_t proc_get_ruid(struct procinfo *pi, pid_t pid) {
  struct process *p = proc_find(pi, pid);

  proc_status(p);
  return p->prop_ruid;
}

uid_t proc_get_euid(struct procinfo *pi, pid_t pid) {
  struct process *p = proc_find(pi, pid);

  if(!p->eid_set)
    proc_status(p);
  return p->prop_euid;
}

gid_t proc_get_rgid(struct procinfo *pi, pid_t pid) {
  struct process *p = proc_find(pi, pid);

  proc_status(p);
  return p->prop_rgid;
}

gid_t proc_get_egid(struct procinfo *pi, pid_t pid) {
  struct process *p = proc_find(pi, pid);

  if(!p->eid_set)
    proc_status(p);
  proc_status(p);
  return p->prop_egid;
}

pid_t proc_get_ppid(struct procinfo *pi, pid_t pid) {
  struct process *p = proc_find(pi, pid);

  proc_stat(p);
  return p->prop_ppid;
}

pid_t proc_get_pgid(struct procinfo *pi, pid_t pid) {
  struct process *p = proc_find(pi, pid);

  proc_stat(p);
  return p->prop_tpgid;
}

int proc_get_tty(struct procinfo *pi, pid_t pid) {
  struct process *p = proc_find(pi, pid);

  proc_stat(p);
  return p->prop_tty_nr;
}

const char *proc_get_comm(struct procinfo *pi, pid_t pid) {
  struct process *p = proc_find(pi, pid);

  proc_stat(p);
  return p->prop_comm;
}

const char *proc_get_cmdline(struct procinfo *pi, pid_t pid) {
  struct process *p = proc_find(pi, pid);

  proc_cmdline(p);
  if(!*p->prop_cmdline) {
    /* "Failing this, the command name, as it would appear without the
     * option -f, is written in square brackets. */
    free(p->prop_cmdline);
    proc_stat(p);
    if(asprintf(&p->prop_cmdline, "[%s]", p->prop_comm) < 0)
      fatal(errno, "asprintf");
  }
  return p->prop_cmdline;
}

intmax_t proc_get_scheduled_time(struct procinfo *pi, pid_t pid) {
  struct process *p = proc_find(pi, pid);
  
  proc_stat(p);
  return clock_to_seconds(p->prop_utime + p->prop_stime);
}

intmax_t proc_get_elapsed_time(struct procinfo *pi, pid_t pid) {
  struct process *p = proc_find(pi, pid);
  
  /* We have to return consistent values, otherwise the column size
   * computation becomes inconsistent */
  if(!p->elapsed_set) {
    proc_stat(p);
    p->elapsed = time(NULL) - clock_to_time(p->prop_starttime);
    p->elapsed_set = 1;
  }
  return p->elapsed;
}

time_t proc_get_start_time(struct procinfo *pi, pid_t pid) {
  struct process *p = proc_find(pi, pid);
  proc_stat(p);
  return clock_to_time(p->prop_starttime);
}

uintmax_t proc_get_flags(struct procinfo *pi, pid_t pid) {
  struct process *p = proc_find(pi, pid);
  proc_stat(p);
  return p->prop_flags;
}

intmax_t proc_get_nice(struct procinfo *pi, pid_t pid) {
  struct process *p = proc_find(pi, pid);
  proc_stat(p);
  return p->prop_nice;
}

intmax_t proc_get_priority(struct procinfo *pi, pid_t pid) {
  struct process *p = proc_find(pi, pid);
  proc_stat(p);
  return p->prop_priority;
}

int proc_get_state(struct procinfo *pi, pid_t pid) {
  struct process *p = proc_find(pi, pid);
  proc_stat(p);
  return p->prop_state;
}

int proc_get_pcpu(struct procinfo *pi, pid_t pid) {
  /* TODO PCPU is supposed to describe "recent" usage; we actually
   * return the process's %cpu usage over its entire history, which is
   * really stretching the definition.  The fix is to sample it twice
   * but we are not currently set up for that. */
  long scheduled = proc_get_scheduled_time(pi, pid);
  long elapsed = proc_get_elapsed_time(pi, pid);
  return elapsed ? scheduled * 100 / elapsed : 0;
}

uintmax_t proc_get_vsize(struct procinfo *pi, pid_t pid) {
  struct process *p = proc_find(pi, pid);
  proc_stat(p);
  return p->prop_vsize;
}

uintmax_t proc_get_rss(struct procinfo *pi, pid_t pid) {
  struct process *p = proc_find(pi, pid);
  proc_stat(p);
  return p->prop_rss * sysconf(_SC_PAGESIZE);
}

uintmax_t proc_get_insn_pointer(struct procinfo *pi, pid_t pid) {
  struct process *p = proc_find(pi, pid);
  proc_stat(p);
  return p->prop_kstkeip;
}

uintmax_t proc_get_wchan(struct procinfo *pi, pid_t pid) {
  struct process *p = proc_find(pi, pid);
  proc_stat(p);
  return p->prop_wchan;
}

// ----------------------------------------------------------------------------

pid_t *proc_get_selected(struct procinfo *pi, size_t *npids) {
  size_t n, count = 0;
  pid_t *pids;
  for(n = 0; n < pi->nprocs; ++n)
    if(pi->procs[n].selected && !pi->procs[n].vanished)
      ++count;
  pids = xrecalloc(NULL, count, sizeof *pids);
  count = 0;
  for(n = 0; n < pi->nprocs; ++n)
    if(pi->procs[n].selected && !pi->procs[n].vanished)
      pids[count++] = pi->procs[n].pid;
  *npids = count;
  return pids; 
}
