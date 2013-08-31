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
#include "tasks.h"
#include "utils.h"
#include "selectors.h"
#include "priv.h"
#include "general.h"
#include "io.h"
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <inttypes.h>
#include <stddef.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <signal.h>
#include <time.h>

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

#define VM_PROPS(M) M(VmPeak, 1)                \
  M(VmSize, 2)                                  \
  M(VmLck, 4)                                   \
  M(VmPin, 8)                                   \
  M(VmHWM, 16)                                   \
  M(VmRSS, 32)                                  \
  M(VmData, 64)                                 \
  M(VmStk, 128)                                  \
  M(VmExe, 256)                                 \
  M(VmLib, 512)                                 \
  M(VmPTE, 1024)                                 \
  M(VmSwap, 2048)
#define VMMEMBER(N,B) uintmax_t prop_##N;
#define VMTABLE(N,B) { #N, offsetof(struct task, prop_##N), B },
#define VMENUM(N,B) bit_##N = B,
enum {
  VM_PROPS(VMENUM)
};

#define SMEMBER(X) intmax_t prop_##X;
#define BASE_SMEMBER(X) uintmax_t base_##X;
#define UMEMBER(X) uintmax_t prop_##X;
#define BASE_UMEMBER(X) uintmax_t base_##X;
#define OFFSET(X) offsetof(struct task, prop_##X),
#define UPDATE_BASE(X) t->base_##X = lastt->prop_##X;

struct task {
  taskident taskid;      /* process/thread ID */
  size_t link;                  /* hash table linkage */
  unsigned selected:1;          /* nonzero if selected */
  unsigned stat:1;              /* nonzero if task_stat() called */
  unsigned status:1;            /* nonzero if task_status() called */
  unsigned io:1;                /* nonzero if task_io() called */
  unsigned smaps:1;             /* nonzero if task_smaps() called */
  unsigned sorted:1;            /* nonzero if properties sorted */
  unsigned vanished:1;          /* nonzero if process vanished */
  unsigned elapsed_set:1;       /* nonzero if elapsed has been set */
  unsigned oom_score_set:1;     /* nonzero if oom_score is set */
  unsigned pss:1;               /* nonzero if prop_pss valid */
  unsigned vmbits;              /* Vm... bit set */
  char *prop_comm;
  char *prop_cmdline;
  int prop_state;
  intmax_t elapsed;
  uid_t prop_ruid, prop_euid, prop_suid, prop_fsuid;
  gid_t prop_rgid, prop_egid, prop_sgid, prop_fsgid;
  intmax_t base_utime, base_stime;
  uintmax_t base_majflt, base_minflt;
  struct timespec base_stat_time, stat_time;
  struct timespec base_io_time, io_time;
  intmax_t oom_score;
  uintmax_t prop_pss, prop_swap;
  size_t ngroups;
  gid_t *groups;
  sigset_t sigpending, sigblocked, sigignored, sigcaught;
  STAT_PROPS(UMEMBER,SMEMBER)
  IO_PROPS(UMEMBER,SMEMBER)
  IO_PROPS(BASE_SMEMBER,BASE_UMEMBER)
  VM_PROPS(VMMEMBER)
};

static const size_t propinfo_stat[] = {
  STAT_PROPS(OFFSET,OFFSET)
};
#define NSTATS (sizeof propinfo_stat / sizeof *propinfo_stat)

static const size_t propinfo_io[] = {
  IO_PROPS(OFFSET,OFFSET)
};
#define NIOS (sizeof propinfo_io / sizeof *propinfo_io)

static const struct {
  const char *name;
  size_t offset;
  unsigned bit;
} propinfo_vm[] = {
  VM_PROPS(VMTABLE)
};
#define NVMS (sizeof propinfo_vm / sizeof *propinfo_vm)

#define HASH_SIZE 256           /* hash table size */

struct taskinfo {
  /* How many processes/threads are in the system */
  size_t nprocesses, nthreads;
  /* Number of entries, and space, in tasks array */
  size_t ntasks, nslots;
  /* Table of processes and threads */
  struct task *tasks;
  /* Real time that this sample was started */
  struct timespec time;
  /* Heads of hash chains */
  size_t lookup[HASH_SIZE];
};

static struct task *task_find(const struct taskinfo *ti, taskident taskid);

const char *proc = "/proc";
pid_t selfpid = -1;

// ----------------------------------------------------------------------------

static uintmax_t conv(const char *s) {
  return s ? strtoumax(s, NULL, 10) : 0;
}

// ----------------------------------------------------------------------------

void task_free(struct taskinfo *ti) {
  if(ti) {
    size_t n;
    for(n = 0; n < ti->ntasks; ++n) {
      free(ti->tasks[n].prop_comm);
      free(ti->tasks[n].prop_cmdline);
      free(ti->tasks[n].groups);
    }
    free(ti->tasks);
    free(ti);
  }
}

static struct task *task_add(struct taskinfo *ti, struct taskinfo *last,
                                pid_t pid, pid_t tid) {
  struct task *t, *lastt;
  /* Make sure the array is big enough */
  if(ti->ntasks >= ti->nslots) {
    if((ssize_t)(ti->nslots = ti->nslots ? 2 * ti->nslots : 64) <= 0)
      fatal(0, "too many tasks");
    ti->tasks = xrecalloc(ti->tasks, ti->nslots, sizeof *ti->tasks);
  }
  t = &ti->tasks[ti->ntasks++];
  memset(t, 0, sizeof *t);
  t->taskid.pid = pid;
  t->taskid.tid = tid;
  /* Retrieve bases for delta values */
  if(last && (lastt = task_find(last, t->taskid))) {
    t->base_utime = lastt->prop_utime;
    t->base_stime = lastt->prop_stime;
    t->base_stat_time = lastt->stat_time;
    t->base_majflt = lastt->prop_majflt;
    t->base_minflt = lastt->prop_minflt;
    IO_PROPS(UPDATE_BASE, UPDATE_BASE);
    t->base_io_time = lastt->io_time;
  }
  t->link = SIZE_MAX;
  return t;
}

static void task_enumerate_threads(struct taskinfo *ti, struct taskinfo *last,
                                   pid_t pid) {
  DIR *dp;
  struct dirent *de;
  char *path;
  pid_t tid;

  if(!(dp = opendirf(&path, "%s/%ld/task", proc, (long)pid))) {
    free(path);
    /* TODO we should mark the process as vanished */
    return;
  }
  while((de = xreaddir(path, dp))) {
    /* Only consider files that look like threads */
    if(strspn(de->d_name, "0123456789") == strlen(de->d_name)) {
      tid = conv(de->d_name);
      task_add(ti, last, pid, tid);
      ++ti->nthreads;
    }
  }
  closedir(dp);
  free(path);
}

struct taskinfo *task_enumerate(struct taskinfo *last,
                                unsigned flags) {
  DIR *dp;
  struct dirent *de;
  size_t n;
  struct taskinfo *ti;
  pid_t pid;

  ti = xmalloc(sizeof *ti);
  memset(ti, 0, sizeof *ti);
  if(clock_gettime(CLOCK_REALTIME, &ti->time) < 0)
    fatal(errno, "clock_gettime");
  /* Look through /proc for process information */
  if(!(dp = opendir(proc)))
    fatal(errno, "opening %s", proc);
  while((de = xreaddir(proc, dp))) {
    /* Only consider files that look like processes */
    if(strspn(de->d_name, "0123456789") == strlen(de->d_name)) {
      pid = conv(de->d_name);
      task_add(ti, last, pid, -1);
      ++ti->nprocesses;
      if(flags & TASK_THREADS)
        task_enumerate_threads(ti, last, pid);
    }
  }
  closedir(dp);
  for(n = 0; n < HASH_SIZE; ++n)
    ti->lookup[n] = SIZE_MAX;
  for(n = 0; n < ti->ntasks; ++n) {
    size_t h = (size_t)(ti->tasks[n].taskid.pid + ti->tasks[n].taskid.tid) % HASH_SIZE;
    if(ti->lookup[h] != SIZE_MAX)
      ti->tasks[n].link = ti->lookup[h];
    ti->lookup[h] = n;
  }
  task_reselect(ti);
  return ti;
}

int task_processes(struct taskinfo *ti) {
  return ti->nprocesses;
}

int task_threads(struct taskinfo *ti) {
  return ti->nthreads;
}

void task_time(struct taskinfo *ti, struct timespec *ts) {
  *ts = ti->time;
}

void task_reselect(struct taskinfo *ti) {
  size_t n;
  for(n = 0; n < ti->ntasks; ++n)
    ti->tasks[n].selected = select_test(ti, ti->tasks[n].taskid);
}

// ----------------------------------------------------------------------------

static struct task *task_find(const struct taskinfo *ti, taskident taskid) {
  size_t n = ti->lookup[(size_t)(taskid.pid + taskid.tid) % HASH_SIZE];
  while(n != SIZE_MAX
        && (ti->tasks[n].taskid.pid != taskid.pid
            || ti->tasks[n].taskid.tid != taskid.tid))
    n = ti->tasks[n].link;
  return n != SIZE_MAX ? &ti->tasks[n] : NULL;
}

// ----------------------------------------------------------------------------

static void getpath(const struct task *t,
                    const char *what,
                    char buffer[],
                    size_t bufsize) {
  if(t->taskid.tid == -1)
    snprintf(buffer, bufsize, "%s/%ld/%s", proc, (long)t->taskid.pid, what);
  else
    snprintf(buffer, bufsize, "%s/%ld/task/%ld/%s",
             proc, (long)t->taskid.pid, (long)t->taskid.tid, what);
}

static void task_stat(struct task *t) {
  FILE *fp;
  char buffer[1024], *start, *bp;
  size_t field;
  uintmax_t *ptr, value;

  if(t->stat || t->vanished)
    return;
  t->stat = 1;
  getpath(t, "stat", buffer, sizeof buffer);
  if(!(fp = fopen(buffer, "r"))) {
    t->vanished = 1;
    return;
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
	value = strtoumax(bp, &bp, 10);
        if((field - 3) < NSTATS) {
          ptr = (uintmax_t *)((char *)t + propinfo_stat[field - 3]);
          *ptr = value;
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
        t->prop_comm = xstrndup(start, bp - start);
        if(*bp)
          ++bp;
        break;
      case 2:                   /* state */
        t->prop_state = *bp++;
        break;
      }
      field++;
    }
  }
  if(!t->prop_comm)
    t->prop_comm = xstrdup("-");
  fclose(fp);
  timespec_now(&t->stat_time);
}

static size_t parse_groups(const char *ptr,
                           gid_t *groups,
                           size_t max) {
  size_t ngroups = 0;
  gid_t gid;
  char *end;

  while(*ptr) {
    if(*ptr == ' ' || *ptr == '\n') {
      ++ptr;
      continue;
    }
    gid = strtol(ptr, &end, 10);
    ptr = end;
    if(groups && ngroups < max)
      groups[ngroups] = gid;
    ++ngroups;
  }
  return ngroups;
}

static void parse_sigset(sigset_t *ss, const char *ptr) {
  int ch, sig, d, bit;
  if(sigemptyset(ss) < 0)
    fatal(errno, "sigemptyset");
  ptr += strspn(ptr, " \t");
  /* The inverse of render_sigset_t in the kernel */
  sig = 4 * strspn(ptr, "0123456789abcdefABCDEF");
  while(sig > 0) {
    ch = *ptr++;
    if(ch <= '9')
      d = ch - '0';
    else if(ch <= 'F')
      d = ch - ('A' - 10);
    else
      d = ch - ('a' - 10);
    for(bit = 8; bit >= 1; bit /= 2) {
      if(d & bit)
        if(sigaddset(ss, sig) < 0)
          fatal(errno, "sigaddset");
      --sig;
    }
  }
}

static void task_status(struct task *t) {
  char buffer[1024], *ptr;
  size_t i, n;
  FILE *fp;
  int c;
  long e, r, s, f;

  if(t->status || t->vanished)
    return;
  t->status = 1;
  getpath(t, "status", buffer, sizeof buffer);
  if(!(fp = fopen(buffer, "r"))) {
    t->vanished = 1;
    return;
  }
  i = 0;
  while((c = GETC(fp)) != EOF) {
    if(c != '\n') {
      if(i < sizeof buffer - 1)
        buffer[i++] = c;
    } else {
      buffer[i] = 0;
      if((ptr = strchr(buffer, ':'))) {
        *ptr++ = 0;
        while(*ptr && (*ptr == ' ' || *ptr == '\t'))
          ++ptr;
        if(buffer[0] == 'V' && buffer[1] == 'm') {
          for(n = 0; n < NVMS; ++n)
            if(!strcmp(buffer, propinfo_vm[n].name)) {
              uintmax_t *u = (uintmax_t *)((char *)t + propinfo_vm[n].offset);
              *u = strtoumax(ptr, NULL, 10);
              t->vmbits |= propinfo_vm[n].bit;
              break;
            }
        } else if(!strcmp(buffer, "Uid")
           && sscanf(ptr, "%ld %ld %ld %ld", &r, &e, &s, &f) == 4) {
          t->prop_ruid = r;
          t->prop_euid = e;
          t->prop_suid = s;
          t->prop_fsuid = f;
        } else if(!strcmp(buffer, "Gid")
                  && sscanf(ptr, "%ld %ld %ld %ld", &r, &e, &s, &f) == 4) {
          t->prop_rgid = r;
          t->prop_egid = e;
          t->prop_sgid = s;
          t->prop_fsgid = f;
        } else if(!strcmp(buffer, "Groups")) {
          if(t->groups)
            free(t->groups);
          t->ngroups = parse_groups(ptr, NULL, 0);
          t->groups = xrecalloc(NULL, t->ngroups, sizeof *t->groups);
          parse_groups(ptr, t->groups, t->ngroups);
        } else if(!strcmp(buffer, "SigPnd"))
          parse_sigset(&t->sigpending, ptr);
        else if(!strcmp(buffer, "SigBlk"))
          parse_sigset(&t->sigblocked, ptr);
        else if(!strcmp(buffer, "SigIgn"))
          parse_sigset(&t->sigignored, ptr);
        else if(!strcmp(buffer, "SigCgt"))
          parse_sigset(&t->sigcaught, ptr);
      }
      i = 0;
    }
  }
  fclose(fp);
}

static void task_cmdline(struct task *t) {
  char buffer[1024];
  size_t i;
  FILE *fp;
  int c, trailing_space = 0;

  if(t->prop_cmdline || t->vanished)
    return;
  getpath(t, "cmdline", buffer, sizeof buffer);
  if(!(fp = fopen(buffer, "r"))) {
    t->vanished = 1;
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
  t->prop_cmdline = xstrdup(buffer);
}

struct priv_callback_data {
  char path[128];
  struct task *t;
};

static int read_io(void *u) {
  struct priv_callback_data *d = u;
  char buffer[1024], *colon;
  size_t field;
  uintmax_t *ptr;
  FILE *fp;

  if(!(fp = fopen(d->path, "r"))) {
    if(errno != EACCES)
      d->t->vanished = 1;
    return -1;
  }
  field = 0;
  while(fgets(buffer, sizeof buffer, fp)) {
    if(!strchr(buffer, '\n')) {
      fclose(fp);
      return -1;
    }
    colon = strchr(buffer, ':');
    if(colon) {
      ++colon;
      if(field < NIOS) {
        ptr = (uintmax_t *)((char *)d->t + propinfo_io[field]);
        *ptr = strtoumax(colon + 1, NULL, 10);
      }
      ++field;
    }
  }
  fclose(fp);
  return 0;
}

static void task_io(struct task *t) {
  struct priv_callback_data d[1];

  if(t->io || t->vanished)
    return;
  t->io = 1;
  getpath(t, "io", d->path, sizeof d->path);
  d->t = t;
  priv_run(read_io, d);
  timespec_now(&t->io_time);
}

static void task_oom_score(struct task *t) {
  char buffer[128];
  FILE *fp;
  if(t->oom_score_set || t->vanished)
    return;
  t->oom_score_set =1;
  getpath(t, "oom_score", buffer, sizeof buffer);
  if(!(fp = fopen(buffer, "r"))) {
    t->vanished = 1;
    return;
  }
  if(fscanf(fp, "%jd", &t->oom_score) < 0)
    t->vanished = 1;
  fclose(fp);
}

static int read_smaps(void *u) {
  struct priv_callback_data *d = u;
  char buffer[1024], *ptr;
  FILE *fp;

  if(!(fp = fopen(d->path, "r"))) {
    if(errno != EACCES)
      d->t->vanished = 1;
    return -1;
  }
  while(fgets(buffer, sizeof buffer, fp)) {
    if(!strchr(buffer, '\n')) {
      fclose(fp);
      return -1;
    }
    if(buffer[0] >= 'A' && buffer[0] <= 'Z'
       && (ptr = strchr(buffer, ':'))) {
      *ptr++ = 0;
      if(!strcmp(buffer, "Pss"))
        d->t->prop_pss += strtoumax(ptr, NULL, 0);
      else if(!strcmp(buffer, "Swap"))
        d->t->prop_swap += strtoumax(ptr, NULL, 0);
    }
  }
  d->t->pss = 1;
  fclose(fp);
  return 0;
}

static void task_smaps(struct task *t) {
  struct priv_callback_data d[1];
  if(t->smaps || t->vanished)
    return;
  t->smaps = 1;
  getpath(t, "smaps", d->path, sizeof d->path);
  d->t = t;
  t->prop_pss = t->prop_swap = 0;
  priv_run(read_smaps, d);
}

// ----------------------------------------------------------------------------

pid_t task_get_pid(struct taskinfo attribute((unused)) *ti, taskident taskid) {
  return taskid.pid;
}

pid_t task_get_tid(struct taskinfo attribute((unused)) *ti, taskident taskid) {
  return taskid.tid;
}

pid_t task_get_session(struct taskinfo *ti, taskident taskid) {
 struct task *t = task_find(ti, taskid);

  task_stat(t);
  return t->prop_session;
}

uid_t task_get_ruid(struct taskinfo *ti, taskident taskid) {
  struct task *t = task_find(ti, taskid);

  task_status(t);
  return t->prop_ruid;
}

uid_t task_get_euid(struct taskinfo *ti, taskident taskid) {
  struct task *t = task_find(ti, taskid);

  task_status(t);
  return t->prop_euid;
}

uid_t task_get_suid(struct taskinfo *ti, taskident taskid) {
  struct task *t = task_find(ti, taskid);

  task_status(t);
  return t->prop_suid;
}

uid_t task_get_fsuid(struct taskinfo *ti, taskident taskid) {
  struct task *t = task_find(ti, taskid);

  task_status(t);
  return t->prop_fsuid;
}

gid_t task_get_rgid(struct taskinfo *ti, taskident taskid) {
  struct task *t = task_find(ti, taskid);

  task_status(t);
  return t->prop_rgid;
}

gid_t task_get_egid(struct taskinfo *ti, taskident taskid) {
  struct task *t = task_find(ti, taskid);

  task_status(t);
  return t->prop_egid;
}

gid_t task_get_sgid(struct taskinfo *ti, taskident taskid) {
  struct task *t = task_find(ti, taskid);

  task_status(t);
  return t->prop_sgid;
}

gid_t task_get_fsgid(struct taskinfo *ti, taskident taskid) {
  struct task *t = task_find(ti, taskid);

  task_status(t);
  return t->prop_fsgid;
}

pid_t task_get_ppid(struct taskinfo *ti, taskident taskid) {
  struct task *t = task_find(ti, taskid);

  task_stat(t);
  return t->prop_ppid;
}

pid_t task_get_pgrp(struct taskinfo *ti, taskident taskid) {
  struct task *t = task_find(ti, taskid);

  task_stat(t);
  return t->prop_pgrp;
}

pid_t task_get_tpgid(struct taskinfo *ti, taskident taskid) {
  struct task *t = task_find(ti, taskid);

  task_stat(t);
  return t->prop_tpgid;
}

int task_get_tty(struct taskinfo *ti, taskident taskid) {
  struct task *t = task_find(ti, taskid);

  task_stat(t);
  return t->prop_tty_nr;
}

const char *task_get_comm(struct taskinfo *ti, taskident taskid) {
  struct task *t = task_find(ti, taskid);

  task_stat(t);
  return t->prop_comm;
}

const char *task_get_cmdline(struct taskinfo *ti, taskident taskid) {
  struct task *t = task_find(ti, taskid);

  task_cmdline(t);
  if(!t->prop_cmdline || !*t->prop_cmdline) {
    /* "Failing this, the command name, as it would appear without the
     * option -f, is written in square brackets. */
    free(t->prop_cmdline);
    task_stat(t);
    xasprintf(&t->prop_cmdline, "[%s]", t->prop_comm);
  }
  return t->prop_cmdline;
}

intmax_t task_get_scheduled_time(struct taskinfo *ti, taskident taskid) {
  struct task *t = task_find(ti, taskid);
  
  task_stat(t);
  return clock_to_seconds(t->prop_utime + t->prop_stime);
}

intmax_t task_get_elapsed_time(struct taskinfo *ti, taskident taskid) {
  struct task *t = task_find(ti, taskid);
  
  /* We have to return consistent values, otherwise the column size
   * computation becomes inconsistent */
  if(!t->elapsed_set) {
    task_stat(t);
    t->elapsed = time(NULL) - clock_to_time(t->prop_starttime);
    t->elapsed_set = 1;
  }
  return t->elapsed;
}

intmax_t task_get_start_time(struct taskinfo *ti, taskident taskid) {
  struct task *t = task_find(ti, taskid);
  task_stat(t);
  return clock_to_time(t->prop_starttime);
}

uintmax_t task_get_flags(struct taskinfo *ti, taskident taskid) {
  struct task *t = task_find(ti, taskid);
  task_stat(t);
  return t->prop_flags;
}

intmax_t task_get_nice(struct taskinfo *ti, taskident taskid) {
  struct task *t = task_find(ti, taskid);
  task_stat(t);
  return t->prop_nice;
}

intmax_t task_get_priority(struct taskinfo *ti, taskident taskid) {
  struct task *t = task_find(ti, taskid);
  task_stat(t);
  return t->prop_priority;
}

int task_get_state(struct taskinfo *ti, taskident taskid) {
  struct task *t = task_find(ti, taskid);
  task_stat(t);
  return t->prop_state;
}

/* Many properties are supposed to describe the "recent" rate of
 * change of some variable, but the kernel only reports the value of
 * the variable since the start of the relevant process.  When we have
 * two samples at known times this is not a problem but for ps and for
 * the first sample of top we have no such luxury.  Therefore we
 * report the rate over the process's entire lifetime. */
static double task_rate(struct task *t,
                        struct timespec base_time,
                        struct timespec end_time,
                        double quantity) {
  double seconds;
  /* If the process has vanished then whatever we've got now is
   * probably bogus. */
  if(t->vanished)
    return 0;
  if(base_time.tv_sec)
    seconds = (end_time.tv_sec - base_time.tv_sec)
      + (end_time.tv_nsec - base_time.tv_nsec) / 1000000000.0;
  else
    seconds = end_time.tv_sec + end_time.tv_nsec / 1000000000.0
      - clock_to_time(t->prop_starttime);
  return quantity / seconds;
}

double task_get_pcpu(struct taskinfo *ti, taskident taskid) {
  struct task *t = task_find(ti, taskid);
  task_stat(t);
  return task_rate(t, t->base_stat_time, t->stat_time,
                   clock_to_seconds((t->prop_utime + t->prop_stime)
                                    - (t->base_utime + t->base_stime)));
}

uintmax_t task_get_vsize(struct taskinfo *ti, taskident taskid) {
  struct task *t = task_find(ti, taskid);
  if(t->vmbits & bit_VmSize)
    return t->prop_VmSize * KILOBYTE;
  task_stat(t);
  return t->prop_vsize;
}

uintmax_t task_get_peak_vsize(struct taskinfo *ti, taskident taskid) {
  struct task *t = task_find(ti, taskid);
  task_status(t);
  return t->prop_VmPeak * KILOBYTE;
}

uintmax_t task_get_rss(struct taskinfo *ti, taskident taskid) {
  struct task *t = task_find(ti, taskid);
  if(t->vmbits & bit_VmRSS)
    return t->prop_VmRSS * KILOBYTE;
  task_stat(t);
  return t->prop_rss * sysconf(_SC_PAGESIZE);
}

uintmax_t task_get_peak_rss(struct taskinfo *ti, taskident taskid) {
  struct task *t = task_find(ti, taskid);
  task_status(t);
  return t->prop_VmHWM * KILOBYTE;
}

uintmax_t task_get_insn_pointer(struct taskinfo *ti, taskident taskid) {
  struct task *t = task_find(ti, taskid);
  task_stat(t);
  return t->prop_kstkeip;
}

uintmax_t task_get_wchan(struct taskinfo *ti, taskident taskid) {
  struct task *t = task_find(ti, taskid);
  task_stat(t);
  return t->prop_wchan;
}

double task_get_rchar(struct taskinfo *ti, taskident taskid) {
  struct task *t = task_find(ti, taskid);
  task_io(t);
  return task_rate(t, t->base_io_time, t->io_time,
                   t->prop_rchar - t->base_rchar);
}

double task_get_wchar(struct taskinfo *ti, taskident taskid) {
  struct task *t = task_find(ti, taskid);
  task_io(t);
  return task_rate(t, t->base_io_time, t->io_time,
                   t->prop_wchar - t->base_wchar);
}

double task_get_read_bytes(struct taskinfo *ti, taskident taskid) {
  struct task *t = task_find(ti, taskid);
  task_io(t);
  return task_rate(t, t->base_io_time, t->io_time,
                   t->prop_read_bytes - t->base_read_bytes);
}

double task_get_write_bytes(struct taskinfo *ti, taskident taskid) {
  struct task *t = task_find(ti, taskid);
  task_io(t);
  return task_rate(t, t->base_io_time, t->io_time,
                   t->prop_write_bytes - t->base_write_bytes);
}

double task_get_rw_bytes(struct taskinfo *ti, taskident taskid) {
  struct task *t = task_find(ti, taskid);
  task_io(t);
  return task_rate(t, t->base_io_time, t->io_time,
                   t->prop_read_bytes + t->prop_write_bytes
                   - t->base_read_bytes - t->base_write_bytes);
}

intmax_t task_get_oom_score(struct taskinfo *ti, taskident taskid) {
  struct task *t = task_find(ti, taskid);
  task_oom_score(t);
  return t->oom_score;
}

double task_get_majflt(struct taskinfo *ti, taskident taskid) {
  struct task *t = task_find(ti, taskid);
  task_stat(t);
  return task_rate(t, t->base_stat_time, t->stat_time,
                   t->prop_majflt - t->base_majflt) * sysconf(_SC_PAGE_SIZE);
}

double task_get_minflt(struct taskinfo *ti, taskident taskid) {
  struct task *t = task_find(ti, taskid);
  task_stat(t);
  return task_rate(t, t->base_stat_time, t->stat_time,
                   t->prop_minflt - t->base_minflt) * sysconf(_SC_PAGE_SIZE);
}

uintmax_t task_get_pss(struct taskinfo *ti, taskident taskid) {
  struct task *t = task_find(ti, taskid);
  task_smaps(t);
  return t->prop_pss * KILOBYTE;
}

uintmax_t task_get_swap(struct taskinfo *ti, taskident taskid) {
  struct task *t = task_find(ti, taskid);
  task_status(t);
  /* Since 2.6.34 (b084d4353ff99d824d3bc5a5c2c22c70b1fba722), swap
   * usage has been exposed directly */
  if(t->vmbits & bit_VmSwap)
    return t->prop_VmSwap * KILOBYTE;
  /* Prior to that we have to work it out by adding up smaps entries */
  task_smaps(t);
  return t->prop_swap * KILOBYTE;
}

uintmax_t task_get_mem(struct taskinfo *ti, taskident taskid) {
  return task_get_rss(ti, taskid) + task_get_swap(ti, taskid);
}

uintmax_t task_get_pmem(struct taskinfo *ti, taskident taskid) {
  return task_get_pss(ti, taskid) + task_get_swap(ti, taskid);
  struct task *t = task_find(ti, taskid);
  uintmax_t pss = task_get_pss(ti, taskid);
  return t->pss ? pss + task_get_swap(ti, taskid) : 0;
}

int task_get_num_threads(struct taskinfo *ti, taskident taskid) {
  if(taskid.tid < 0) {
    struct task *t = task_find(ti, taskid);
    task_stat(t);
    return t->prop_num_threads;
  } else
    return -1;
}

const gid_t *task_get_supgids(struct taskinfo *ti, taskident taskid,
                              size_t *countp) {
  struct task *t = task_find(ti, taskid);
  task_status(t);
  if(countp)
    *countp = t->ngroups;
  return t->groups;
}

uintmax_t task_get_rtprio(struct taskinfo *ti, taskident taskid) {
  struct task *t = task_find(ti, taskid);
  task_stat(t);
  return t->prop_rt_priority;
}

int task_get_sched_policy(struct taskinfo *ti, taskident taskid) {
  struct task *t = task_find(ti, taskid);
  task_stat(t);
  return t->prop_policy;
}

void task_get_sig_pending(struct taskinfo *ti, taskident taskid,
                          sigset_t *signals) {
  struct task *t = task_find(ti, taskid);
  task_status(t);
  *signals = t->sigpending;
}

void task_get_sig_blocked(struct taskinfo *ti, taskident taskid,
                          sigset_t *signals) {
  struct task *t = task_find(ti, taskid);
  task_status(t);
  *signals = t->sigblocked;
}

void task_get_sig_ignored(struct taskinfo *ti, taskident taskid,
                          sigset_t *signals) {
  struct task *t = task_find(ti, taskid);
  task_status(t);
  *signals = t->sigignored;
}

void task_get_sig_caught(struct taskinfo *ti, taskident taskid,
                          sigset_t *signals) {
  struct task *t = task_find(ti, taskid);
  task_status(t);
  *signals = t->sigcaught;
}

uintmax_t task_get_stack(struct taskinfo *ti, taskident taskid) {
  struct task *t = task_find(ti, taskid);
  task_status(t);
  return t->prop_VmStk * KILOBYTE;
}

uintmax_t task_get_locked(struct taskinfo *ti, taskident taskid) {
  struct task *t = task_find(ti, taskid);
  task_status(t);
  return t->prop_VmLck * KILOBYTE;
}

uintmax_t task_get_pinned(struct taskinfo *ti, taskident taskid) {
  struct task *t = task_find(ti, taskid);
  task_status(t);
  return t->prop_VmPin * KILOBYTE;
}

uintmax_t task_get_pte(struct taskinfo *ti, taskident taskid) {
  struct task *t = task_find(ti, taskid);
  task_status(t);
  return t->prop_VmPTE * KILOBYTE;
}

// ----------------------------------------------------------------------------

int task_get_depth(struct taskinfo *ti, taskident taskid) {
  struct task *t = task_find(ti, taskid);
  if(!t)
    return -1;
  task_stat(t);
  if(taskid.pid == t->prop_ppid)
    return 0;
  else {
    taskident parent = { t->prop_ppid, -1 };
    return task_get_depth(ti, parent) + 1;
  }
}

int task_is_ancestor(struct taskinfo *ti, taskident a, taskident b) {
  struct task *t;
  if(b.pid == a.pid)
    return 1;
  t = task_find(ti, b);
  if(!t)
    return 0;
  task_stat(t);
  taskident parent = { t->prop_ppid, -1 };
  return task_is_ancestor(ti, a, parent);
}

// ----------------------------------------------------------------------------

static int selected(const struct task *t, unsigned flags) {
  if(t->selected && !t->vanished) {
    if((flags & TASK_PROCESSES) && t->taskid.tid == -1)
      return 1;
    if((flags & TASK_THREADS) && t->taskid.tid != -1)
      return 1;
  }
  return 0;
}

taskident *task_get_selected(struct taskinfo *ti, size_t *ntasks,
                             unsigned flags) {
  size_t n, count = 0;
  taskident *tasks;
  for(n = 0; n < ti->ntasks; ++n)
    if(selected(&ti->tasks[n], flags))
      ++count;
  tasks = xrecalloc(NULL, count, sizeof *tasks);
  count = 0;
  for(n = 0; n < ti->ntasks; ++n)
    if(selected(&ti->tasks[n], flags))
      tasks[count++] = ti->tasks[n].taskid;
  *ntasks = count;
  return tasks; 
}

static int selected_all(const struct task *t, unsigned flags) {
  if(!t->vanished) {
    if((flags & TASK_PROCESSES) && t->taskid.tid == -1)
      return 1;
    if((flags & TASK_THREADS) && t->taskid.tid != -1)
      return 1;
  }
  return 0;
}

taskident *task_get_all(struct taskinfo *ti, size_t *ntasks,
                        unsigned flags) {
  size_t n, count = 0;
  taskident *tasks;
  for(n = 0; n < ti->ntasks; ++n)
    if(selected_all(&ti->tasks[n], flags))
      ++count;
  tasks = xrecalloc(NULL, count, sizeof *tasks);
  count = 0;
  for(n = 0; n < ti->ntasks; ++n)
    if(selected_all(&ti->tasks[n], flags))
      tasks[count++] = ti->tasks[n].taskid;
  *ntasks = count;
  return tasks; 
}

int self_tty(struct taskinfo *ti) {
  taskident self = { getpid(), -1 };
  if(selfpid != -1)
    self.pid = selfpid;
  return task_get_tty(ti, self);
}
