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
#include "priv.h"
#include "general.h"
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
#include <sys/time.h>

#if HAVE_GETC_UNLOCKED
# define GETC getc_unlocked
#else
# define GETC getc
#endif

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

#define IO_PROPS(U,S) U(rchar) \
  U(wchar)                     \
  U(syscr)                     \
  U(syscw)                     \
  U(read_bytes)                \
  U(write_bytes)               \
  U(cancelled_write_bytes)

#define SMEMBER(X) intmax_t prop_##X;
#define BASE_SMEMBER(X) uintmax_t base_##X;
#define UMEMBER(X) uintmax_t prop_##X;
#define BASE_UMEMBER(X) uintmax_t base_##X;
#define OFFSET(X) offsetof(struct process, prop_##X),
#define UPDATE_BASE(X) p->base_##X = lastp->prop_##X;

struct process {
  taskident taskid;      /* process/thread ID */
  size_t link;                  /* hash table linkage */
  unsigned selected:1;          /* nonzero if selected */
  unsigned stat:1;              /* nonzero if proc_stat() called */
  unsigned status:1;            /* nonzero if proc_status() called */
  unsigned io:1;                /* nonzero if proc_io() called */
  unsigned smaps:1;             /* nonzero if proc_smaps() called */
  unsigned sorted:1;            /* nonzero if properties sorted */
  unsigned vanished:1;          /* nonzero if process vanished */
  unsigned elapsed_set:1;       /* nonzero if elapsed has been set */
  unsigned eid_set:1;           /* nonzero if e[ug]id have been set */
  unsigned oom_score_set:1;     /* nonzero if oom_score is set */
  char *prop_comm;
  char *prop_cmdline;
  int prop_state;
  intmax_t elapsed;
  uid_t prop_euid, prop_ruid;
  gid_t prop_egid, prop_rgid;
  intmax_t base_utime, base_stime;
  uintmax_t base_majflt, base_minflt;
  struct timeval base_stat_time, stat_time;
  struct timeval base_io_time, io_time;
  intmax_t oom_score;
  uintmax_t prop_pss, prop_swap;
  STAT_PROPS(UMEMBER,SMEMBER)
  IO_PROPS(UMEMBER,SMEMBER)
  IO_PROPS(BASE_SMEMBER,BASE_UMEMBER)
};

static const size_t propinfo_stat[] = {
  STAT_PROPS(OFFSET,OFFSET)
};
#define NSTATS (sizeof propinfo_stat / sizeof *propinfo_stat)

static const size_t propinfo_io[] = {
  IO_PROPS(OFFSET,OFFSET)
};
#define NIOS (sizeof propinfo_io / sizeof *propinfo_io)

#define HASH_SIZE 256           /* hash table size */

struct procinfo {
  size_t nprocs, nslots;
  struct process *procs;
  unsigned flags;
  size_t lookup[HASH_SIZE];
};

static struct process *proc_find(const struct procinfo *pi, taskident taskid);

// ----------------------------------------------------------------------------

static uintmax_t conv(const char *s) {
  return s ? strtoumax(s, NULL, 10) : 0;
}

// ----------------------------------------------------------------------------

void proc_free(struct procinfo *pi) {
  if(pi) {
    size_t n;
    for(n = 0; n < pi->nprocs; ++n) {
      free(pi->procs[n].prop_comm);
      free(pi->procs[n].prop_cmdline);
    }
    free(pi->procs);
    free(pi);
  }
}

static struct process *proc_add(struct procinfo *pi, struct procinfo *last,
                                pid_t pid, pid_t tid) {
  struct process *p, *lastp;
  /* Make sure the array is big enough */
  if(pi->nprocs >= pi->nslots) {
    if((ssize_t)(pi->nslots = pi->nslots ? 2 * pi->nslots : 64) <= 0)
      fatal(0, "too many processes");
    pi->procs = xrecalloc(pi->procs, pi->nslots, sizeof *pi->procs);
  }
  p = &pi->procs[pi->nprocs++];
  memset(p, 0, sizeof *p);
  p->taskid.pid = pid;
  p->taskid.tid = tid;
  /* Retrieve bases for delta values */
  if(last && (lastp = proc_find(last, p->taskid)) && lastp->stat) {
    p->base_utime = lastp->prop_utime;
    p->base_stime = lastp->prop_stime;
    p->base_stat_time = lastp->stat_time;
    p->base_majflt = lastp->prop_majflt;
    p->base_minflt = lastp->prop_minflt;
    IO_PROPS(UPDATE_BASE, UPDATE_BASE);
    p->base_io_time = lastp->io_time;
  }
  p->link = SIZE_MAX;
  return p;
}

static void proc_enumerate_threads(struct procinfo *pi, struct procinfo *last,
                                   pid_t pid) {
  DIR *dp;
  struct dirent *de;
  char buffer[128];
  pid_t tid;

  snprintf(buffer, sizeof buffer, "/proc/%ld/task", (long)pid);
  if(!(dp = opendir(buffer))) {
    /* TODO we should mark the process as vanished */
    return;
  }
  errno = 0;
  while((de = readdir(dp))) {
    errno = 0;
    /* Only consider files that look like threads */
    if(strspn(de->d_name, "0123456789") == strlen(de->d_name)) {
      tid = conv(de->d_name);
      proc_add(pi, last, pid, tid);
    }
  }
  if(errno)
    fatal(errno, "reading %s", buffer);
  closedir(dp);
}

struct procinfo *proc_enumerate(struct procinfo *last,
                                unsigned flags) {
  DIR *dp;
  struct dirent *de;
  size_t n;
  struct procinfo *pi;
  pid_t pid;

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
      pid = conv(de->d_name);
      proc_add(pi, last, pid, -1);
      if(flags & PROC_THREADS)
        proc_enumerate_threads(pi, last, pid);
    }
  }
  if(errno)
    fatal(errno, "reading /proc");
  closedir(dp);
  for(n = 0; n < HASH_SIZE; ++n)
    pi->lookup[n] = SIZE_MAX;
  for(n = 0; n < pi->nprocs; ++n) {
    size_t h = (pi->procs[n].taskid.pid + pi->procs[n].taskid.tid) % HASH_SIZE;
    if(pi->lookup[h] != SIZE_MAX)
      pi->procs[n].link = pi->lookup[h];
    pi->lookup[h] = n;
  }
  proc_reselect(pi);
  return pi;
}

int proc_count(struct procinfo *pi) {
  return pi->nprocs;
}

void proc_reselect(struct procinfo *pi) {
  size_t n;
  for(n = 0; n < pi->nprocs; ++n)
    pi->procs[n].selected = select_test(pi, pi->procs[n].taskid);
}

// ----------------------------------------------------------------------------

static struct process *proc_find(const struct procinfo *pi, taskident taskid) {
  size_t n = pi->lookup[(taskid.pid + taskid.tid) % HASH_SIZE];
  while(n != SIZE_MAX
        && (pi->procs[n].taskid.pid != taskid.pid
            || pi->procs[n].taskid.tid != taskid.tid))
    n = pi->procs[n].link;
  return n != SIZE_MAX ? &pi->procs[n] : NULL;
}

// ----------------------------------------------------------------------------

static void getpath(const struct process *p,
                    const char *what,
                    char buffer[],
                    size_t bufsize) {
  if(p->taskid.tid == -1)
    snprintf(buffer, bufsize, "/proc/%ld/%s", (long)p->taskid.pid, what);
  else
    snprintf(buffer, bufsize, "/proc/%ld/task/%ld/%s",
             (long)p->taskid.pid, (long)p->taskid.tid, what);
}

static void proc_stat(struct process *p) {
  FILE *fp;
  char buffer[1024], *start, *bp;
  size_t field;
  uintmax_t *ptr;
  struct stat sb;

  if(p->stat || p->vanished)
    return;
  p->stat = 1;
  getpath(p, "stat", buffer, sizeof buffer);
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
  field = 0;
  if(fgets(buffer, sizeof buffer, fp)) {
    bp = buffer;
    while(*bp) {
      if(*bp == ' ' || *bp == '\n') {
        ++bp;
        continue;
      }
      switch(field) {
      default:
        if((field - 3) < NSTATS) {
          ptr = (uintmax_t *)((char *)p + propinfo_stat[field - 3]);
          *ptr = strtoumax(bp, &bp, 10);
        }
        break;
      case 0:                   /* pid */
        while(*bp && *bp != ' ')
          ++bp;
        break;
      case 1:                   /* comm */
        if(*bp == '(')
          ++bp;
        start = bp;
        while(*bp && *bp != ')')
          ++bp;
        p->prop_comm = xstrndup(start, bp - start);
        if(*bp)
          ++bp;
        break;
      case 2:                   /* state */
        p->prop_state = *bp++;
        break;
      }
      field++;
    }
  }
  if(!p->prop_comm)
    p->prop_comm = xstrdup("-");
  fclose(fp);
  if(gettimeofday(&p->stat_time, NULL) < 0)
    fatal(errno, "gettimeofday");
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
  getpath(p, "status", buffer, sizeof buffer);
  if(!(fp = fopen(buffer, "r"))) {
    p->vanished = 1;
    return;
  }
  i = 0;
  while((c = GETC(fp)) != EOF) {
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
  int c, trailing_space = 0;

  if(p->prop_cmdline || p->vanished)
    return;
  getpath(p, "cmdline", buffer, sizeof buffer);
  if(!(fp = fopen(buffer, "r"))) {
    p->vanished = 1;
    return;
  }
  i = 0;
  while((c = GETC(fp)) != EOF) {
    if(!c) {
      c = ' ';
      trailing_space = 1;
    } else
      trailing_space = 0;
    if(i < sizeof buffer - 1)
      buffer[i++] = c;
  }
  fclose(fp);
  if(trailing_space)
    --i;
  buffer[i] = 0;
  p->prop_cmdline = xstrdup(buffer);
}

struct read_io_data {
  FILE *fp;
  struct process *p;
};

static int read_io(void *u) {
  struct read_io_data *d = u;
  char buffer[1024], *colon;
  size_t field;
  uintmax_t *ptr;
  field = 0;
  while(fgets(buffer, sizeof buffer, d->fp)) {
    if(!strchr(buffer, '\n'))
      return -1;
    colon = strchr(buffer, ':');
    if(colon) {
      ++colon;
      if(field < NIOS) {
        ptr = (uintmax_t *)((char *)d->p + propinfo_io[field]);
        *ptr = strtoumax(colon + 1, NULL, 10);
      }
      ++field;
    }
  }
  return 0;
}

static void proc_io(struct process *p) {
  struct read_io_data d[1];
  char buffer[128];

  if(p->io || p->vanished)
    return;
  p->io = 1;
  getpath(p, "io", buffer, sizeof buffer);
  if(!(d->fp = fopen(buffer, "r"))) {
    if(errno != EACCES)
      p->vanished = 1;
    return;
  }
  d->p = p;
  priv_run(read_io, d);
  fclose(d->fp);
  if(gettimeofday(&p->io_time, NULL) < 0)
    fatal(errno, "gettimeofday");
}

static void proc_oom_score(struct process *p) {
  char buffer[128];
  FILE *fp;
  if(p->oom_score_set || p->vanished)
    return;
  p->oom_score_set =1;
  getpath(p, "oom_score", buffer, sizeof buffer);
  if(!(fp = fopen(buffer, "r"))) {
    p->vanished = 1;
    return;
  }
  fscanf(fp, "%jd", &p->oom_score);
  fclose(fp);
}

struct read_smaps_data {
  FILE *fp;
  uintmax_t pss;
  uintmax_t swap;
};

static int read_smaps(void *u) {
  struct read_smaps_data *d = u;
  char buffer[1024], *ptr;
  while(fgets(buffer, sizeof buffer, d->fp)) {
    if(!strchr(buffer, '\n'))
      return -1;
    if(buffer[0] >= 'A' && buffer[0] <= 'Z'
       && (ptr = strchr(buffer, ':'))) {
      *ptr++ = 0;
      if(!strcmp(buffer, "Pss"))
        d->pss += strtoumax(ptr, NULL, 0);
      else if(!strcmp(buffer, "Swap"))
        d->swap += strtoumax(ptr, NULL, 0);
    }
  }
  return 0;
}

static void proc_smaps(struct process *p) {
  struct read_smaps_data d[1];
  char buffer[128];
  if(p->smaps || p->vanished)
    return;
  p->smaps = 1;
  getpath(p, "smaps", buffer, sizeof buffer);
  if(!(d->fp = fopen(buffer, "r"))) {
    p->vanished = 1;
    return;
  }
  d->pss = d->swap = 0;
  priv_run(read_smaps, d);
  fclose(d->fp);
  p->prop_pss = d->pss;
  p->prop_swap = d->swap;
}

// ----------------------------------------------------------------------------

pid_t proc_get_pid(struct procinfo attribute((unused)) *pi, taskident taskid) {
  return taskid.pid;
}

pid_t proc_get_tid(struct procinfo attribute((unused)) *pi, taskident taskid) {
  return taskid.tid;
}

pid_t proc_get_session(struct procinfo *pi, taskident taskid) {
 struct process *p = proc_find(pi, taskid);

  proc_stat(p);
  return p->prop_session;
}

uid_t proc_get_ruid(struct procinfo *pi, taskident taskid) {
  struct process *p = proc_find(pi, taskid);

  proc_status(p);
  return p->prop_ruid;
}

uid_t proc_get_euid(struct procinfo *pi, taskident taskid) {
  struct process *p = proc_find(pi, taskid);

  if(!p->eid_set)
    proc_status(p);
  return p->prop_euid;
}

gid_t proc_get_rgid(struct procinfo *pi, taskident taskid) {
  struct process *p = proc_find(pi, taskid);

  proc_status(p);
  return p->prop_rgid;
}

gid_t proc_get_egid(struct procinfo *pi, taskident taskid) {
  struct process *p = proc_find(pi, taskid);

  if(!p->eid_set)
    proc_status(p);
  proc_status(p);
  return p->prop_egid;
}

pid_t proc_get_ppid(struct procinfo *pi, taskident taskid) {
  struct process *p = proc_find(pi, taskid);

  proc_stat(p);
  return p->prop_ppid;
}

pid_t proc_get_pgid(struct procinfo *pi, taskident taskid) {
  struct process *p = proc_find(pi, taskid);

  proc_stat(p);
  return p->prop_tpgid;
}

int proc_get_tty(struct procinfo *pi, taskident taskid) {
  struct process *p = proc_find(pi, taskid);

  proc_stat(p);
  return p->prop_tty_nr;
}

const char *proc_get_comm(struct procinfo *pi, taskident taskid) {
  struct process *p = proc_find(pi, taskid);

  proc_stat(p);
  return p->prop_comm;
}

const char *proc_get_cmdline(struct procinfo *pi, taskident taskid) {
  struct process *p = proc_find(pi, taskid);

  proc_cmdline(p);
  if(!p->prop_cmdline || !*p->prop_cmdline) {
    /* "Failing this, the command name, as it would appear without the
     * option -f, is written in square brackets. */
    free(p->prop_cmdline);
    proc_stat(p);
    if(asprintf(&p->prop_cmdline, "[%s]", p->prop_comm) < 0)
      fatal(errno, "asprintf");
  }
  return p->prop_cmdline;
}

intmax_t proc_get_scheduled_time(struct procinfo *pi, taskident taskid) {
  struct process *p = proc_find(pi, taskid);
  
  proc_stat(p);
  return clock_to_seconds(p->prop_utime + p->prop_stime);
}

intmax_t proc_get_elapsed_time(struct procinfo *pi, taskident taskid) {
  struct process *p = proc_find(pi, taskid);
  
  /* We have to return consistent values, otherwise the column size
   * computation becomes inconsistent */
  if(!p->elapsed_set) {
    proc_stat(p);
    p->elapsed = time(NULL) - clock_to_time(p->prop_starttime);
    p->elapsed_set = 1;
  }
  return p->elapsed;
}

intmax_t proc_get_start_time(struct procinfo *pi, taskident taskid) {
  struct process *p = proc_find(pi, taskid);
  proc_stat(p);
  return clock_to_time(p->prop_starttime);
}

uintmax_t proc_get_flags(struct procinfo *pi, taskident taskid) {
  struct process *p = proc_find(pi, taskid);
  proc_stat(p);
  return p->prop_flags;
}

intmax_t proc_get_nice(struct procinfo *pi, taskident taskid) {
  struct process *p = proc_find(pi, taskid);
  proc_stat(p);
  return p->prop_nice;
}

intmax_t proc_get_priority(struct procinfo *pi, taskident taskid) {
  struct process *p = proc_find(pi, taskid);
  proc_stat(p);
  return p->prop_priority;
}

int proc_get_state(struct procinfo *pi, taskident taskid) {
  struct process *p = proc_find(pi, taskid);
  proc_stat(p);
  return p->prop_state;
}

/* Many properties are supposed to describe the "recent" rate of
 * change of some variable, but the kernel only reports the value of
 * the variable since the start of the relevant process.  When we have
 * two samples at known times this is not a problem but for ps and for
 * the first sample of top we have no such luxury.  Therefore we
 * report the rate over the process's entire lifetime. */
static double proc_rate(struct process *p,
                        struct timeval base_time,
                        struct timeval end_time,
                        double quantity) {
  double seconds;
  /* If the process has vanished then whatever we've got now is
   * probably bogus. */
  if(p->vanished)
    return 0;
  if(base_time.tv_sec)
    seconds = (end_time.tv_sec - base_time.tv_sec)
      + (end_time.tv_usec - base_time.tv_usec) / 1000000.0;
  else
    seconds = end_time.tv_sec + end_time.tv_usec / 1000000.0
      - clock_to_time(p->prop_starttime);
  return quantity / seconds;
}

double proc_get_pcpu(struct procinfo *pi, taskident taskid) {
  struct process *p = proc_find(pi, taskid);
  proc_stat(p);
  return proc_rate(p, p->base_stat_time, p->stat_time,
                   clock_to_seconds((p->prop_utime + p->prop_stime)
                                    - (p->base_utime + p->base_stime)));
}

uintmax_t proc_get_vsize(struct procinfo *pi, taskident taskid) {
  struct process *p = proc_find(pi, taskid);
  proc_stat(p);
  return p->prop_vsize;
}

uintmax_t proc_get_rss(struct procinfo *pi, taskident taskid) {
  struct process *p = proc_find(pi, taskid);
  proc_stat(p);
  return p->prop_rss * sysconf(_SC_PAGESIZE);
}

uintmax_t proc_get_insn_pointer(struct procinfo *pi, taskident taskid) {
  struct process *p = proc_find(pi, taskid);
  proc_stat(p);
  return p->prop_kstkeip;
}

uintmax_t proc_get_wchan(struct procinfo *pi, taskident taskid) {
  struct process *p = proc_find(pi, taskid);
  proc_stat(p);
  return p->prop_wchan;
}

double proc_get_rchar(struct procinfo *pi, taskident taskid) {
  struct process *p = proc_find(pi, taskid);
  proc_io(p);
  return proc_rate(p, p->base_io_time, p->io_time,
                   p->prop_rchar - p->base_rchar);
}

double proc_get_wchar(struct procinfo *pi, taskident taskid) {
  struct process *p = proc_find(pi, taskid);
  proc_io(p);
  return proc_rate(p, p->base_io_time, p->io_time,
                   p->prop_wchar - p->base_wchar);
}

double proc_get_read_bytes(struct procinfo *pi, taskident taskid) {
  struct process *p = proc_find(pi, taskid);
  proc_io(p);
  return proc_rate(p, p->base_io_time, p->io_time,
                   p->prop_read_bytes - p->base_read_bytes);
}

double proc_get_write_bytes(struct procinfo *pi, taskident taskid) {
  struct process *p = proc_find(pi, taskid);
  proc_io(p);
  return proc_rate(p, p->base_io_time, p->io_time,
                   p->prop_write_bytes - p->base_write_bytes);
}

double proc_get_rw_bytes(struct procinfo *pi, taskident taskid) {
  struct process *p = proc_find(pi, taskid);
  proc_io(p);
  return proc_rate(p, p->base_io_time, p->io_time,
                   p->prop_read_bytes + p->prop_write_bytes
                   - p->base_read_bytes - p->base_write_bytes);
}

intmax_t proc_get_oom_score(struct procinfo *pi, taskident taskid) {
  struct process *p = proc_find(pi, taskid);
  proc_oom_score(p);
  return p->oom_score;
}

double proc_get_majflt(struct procinfo *pi, taskident taskid) {
  struct process *p = proc_find(pi, taskid);
  proc_stat(p);
  return proc_rate(p, p->base_stat_time, p->stat_time,
                   p->prop_majflt - p->base_majflt) * sysconf(_SC_PAGE_SIZE);
}

double proc_get_minflt(struct procinfo *pi, taskident taskid) {
  struct process *p = proc_find(pi, taskid);
  proc_stat(p);
  return proc_rate(p, p->base_stat_time, p->stat_time,
                   p->prop_minflt - p->base_minflt) * sysconf(_SC_PAGE_SIZE);
}

uintmax_t proc_get_pss(struct procinfo *pi, taskident taskid) {
  struct process *p = proc_find(pi, taskid);
  proc_smaps(p);
  return p->prop_pss * KILOBYTE;
}

uintmax_t proc_get_swap(struct procinfo *pi, taskident taskid) {
  struct process *p = proc_find(pi, taskid);
  proc_smaps(p);
  return p->prop_swap * KILOBYTE;
}

uintmax_t proc_get_mem(struct procinfo *pi, taskident taskid) {
  return proc_get_rss(pi, taskid) + proc_get_swap(pi, taskid);
}

uintmax_t proc_get_pmem(struct procinfo *pi, taskident taskid) {
  return proc_get_pss(pi, taskid) + proc_get_swap(pi, taskid);
}

int proc_get_num_threads(struct procinfo *pi, taskident taskid) {
  if(taskid.tid < 0) {
    struct process *p = proc_find(pi, taskid);
    proc_stat(p);
    return p->prop_num_threads;
  } else
    return -1;
}

// ----------------------------------------------------------------------------

int proc_get_depth(struct procinfo *pi, taskident taskid) {
  struct process *p = proc_find(pi, taskid);
  if(!p)
    return -1;
  proc_stat(p);
  if(taskid.pid == p->prop_ppid)
    return 0;
  else {
    taskident parent = { p->prop_ppid, -1 };
    return proc_get_depth(pi, parent) + 1;
  }
}

int proc_is_ancestor(struct procinfo *pi, taskident a, taskident b) {
  struct process *p;
  if(b.pid == a.pid)
    return 1;
  p = proc_find(pi, b);
  proc_stat(p);
  taskident parent = { p->prop_ppid, -1 };
  return proc_is_ancestor(pi, a, parent);
}

// ----------------------------------------------------------------------------

static int selected(const struct process *p, unsigned flags) {
  if(p->selected && !p->vanished) {
    if((flags & PROC_PROCESSES) && p->taskid.tid == -1)
      return 1;
    if((flags & PROC_THREADS) && p->taskid.tid != -1)
      return 1;
  }
  return 0;
}

taskident *proc_get_selected(struct procinfo *pi, size_t *ntasks,
                             unsigned flags) {
  size_t n, count = 0;
  taskident *tasks;
  for(n = 0; n < pi->nprocs; ++n)
    if(selected(&pi->procs[n], flags))
      ++count;
  tasks = xrecalloc(NULL, count, sizeof *tasks);
  count = 0;
  for(n = 0; n < pi->nprocs; ++n)
    if(selected(&pi->procs[n], flags))
      tasks[count++] = pi->procs[n].taskid;
  *ntasks = count;
  return tasks; 
}

