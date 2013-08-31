/*
 * This file is part of nps.
 * Copyright (C) 2011,13 Richard Kettlewell
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
#include "sysinfo.h"
#include "format.h"
#include "process.h"
#include "utils.h"
#include "general.h"
#include "buffer.h"
#include "parse.h"
#include "io.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <math.h>
#include <sys/time.h>

struct sysinfo;

struct sysprop {
  const char *name;
  const char *heading;
  const char *description;
  void (*format)(const struct sysinfo *sp,
                 struct procinfo *pi, struct buffer *b);
};

struct sysinfo {
  const struct sysprop *prop;
  char *arg;
  char *heading;
};

// ----------------------------------------------------------------------------

static int got_meminfo;

/* /proc/meminfo contents and ordering are not very consistent between
 * versions */
#define MEMINFO(X)                              \
    X(Active, Active)                           \
    X(Active(anon), Active_anon)                \
    X(Active(file), Active_file)                \
    X(AnonPages, AnonPages)                     \
    X(Bounce, Bounce)                           \
    X(Buffers, Buffers)                         \
    X(Cached, Cached)                           \
    X(CommitLimit, CommitLimit)                 \
    X(Committed_AS, Committed_AS)               \
    X(DirectMap2M, DirectMap2M)                 \
    X(DirectMap4k, DirectMap4k)                 \
    X(Dirty, Dirty)                             \
    X(HardwareCorrupted, HardwareCorrupted)     \
    X(HighFree, HighFree)			\
    X(HighTotal, HighTotal)			\
    X(HugePages_Free, HugePages_Free)           \
    X(Hugepagesize, Hugepagesize)               \
    X(HugePages_Rsvd, HugePages_Rsvd)           \
    X(HugePages_Surp, HugePages_Surp)           \
    X(HugePages_Total, HugePages_Total)         \
    X(Inactive(anon), Inactive_anon)            \
    X(Inactive(file), Inactive_file)            \
    X(Inactive, Inactive)                       \
    X(LowFree, LowFree)				\
    X(LowTotal, LowTotal)			\
    X(KernelStack, KernelStack)                 \
    X(Mapped, Mapped)                           \
    X(MemFree, MemFree)                         \
    X(MemTotal, MemTotal)                       \
    X(Mlocked, Mlocked)                         \
    X(NFS_Unstable, NFS_Unstable)               \
    X(PageTables, PageTables)                   \
    X(Shmem, Shmem)                             \
    X(Slab, Slab)                               \
    X(SReclaimable, SReclaimable)               \
    X(SUnreclaim, SUnreclaim)                   \
    X(SwapCached, SwapCached)                   \
    X(SwapFree, SwapFree)                       \
    X(SwapTotal, SwapTotal)                     \
    X(Unevictable, Unevictable)                 \
    X(VmallocChunk, VmallocChunk)               \
    X(VmallocTotal, VmallocTotal)               \
    X(VmallocUsed, VmallocUsed)                 \
    X(WritebackTmp, WritebackTmp)               \
    X(Writeback, Writeback)

#define MEMINFO_FIELD(N,F) uintmax_t F;
#define MEMINFO_MAPPING(N,F) { #N, offsetof(struct meminfo, F) },

struct meminfo { MEMINFO(MEMINFO_FIELD) };
static struct meminfo meminfo;

static const struct meminfo_name {
  const char *name;
  size_t offset;
} meminfo_names[] = { MEMINFO(MEMINFO_MAPPING) };
#define NMEMINFOS (sizeof meminfo_names / sizeof *meminfo_names)

static void get_meminfo(void) {
  if(!got_meminfo) {
    FILE *fp;
    char input[128], *colon;
    ssize_t l, r, m;
    int c;
    uintmax_t *ptr;
    memset(&meminfo, 0, sizeof meminfo);
    fp = xfopenf(NULL, "r", "%s/meminfo", proc);
    while(fgets(input, sizeof input, fp)) {
      if((colon = strchr(input, ':'))) {
        *colon++ = 0;
	l = 0;
	r = NMEMINFOS - 1;
	while(l <= r) {
	  m = l + (r - l) / 2;
	  c = strcmp(input, meminfo_names[m].name);
	  if(c < 0)
	    r = m - 1;
	  else if(c > 0)
	    l = m + 1;
	  else {
	    ptr = (uintmax_t *)((char *)&meminfo + meminfo_names[m].offset);
	    *ptr = strtoull(colon, &colon, 10);
	    break;
	  }
	}
      }
    }
    fclose(fp);
    got_meminfo = 1;
  }
}

// ----------------------------------------------------------------------------

#define CPUINFO(X,XLAST) \
  X(user) \
  X(nice) \
  X(system) \
  X(idle) \
  X(iowait) \
  X(irq) \
  X(softirq) \
  X(steal) \
  X(guest) \
  XLAST(guest_nice)
#define CPUINFO_FIELD(F) uintmax_t F;
#define CPUINFO_FIELD_LAST(F) uintmax_t F
#define CPUINFO_FORMAT(F) "%ju "
#define CPUINFO_FORMAT_LAST(F) "%ju"
#define CPUINFO_ARG(F) &cpu->curr.F,
#define CPUINFO_ARG_LAST(F) &cpu->curr.F

static int got_stat;

struct cpuinfo {
  CPUINFO(CPUINFO_FIELD,CPUINFO_FIELD_LAST);
  uintmax_t user_total;
  uintmax_t guest_total;
};

struct cpuhistory {
  struct cpuinfo curr, last;
};

/* ncpu=0 for all CPUs, ncpu=1 for cpu0, etc. */
struct cpuhistory *cpuinfos;
static size_t ncpuinfos, maxcpuinfos;

static void get_cpuinfo(struct cpuhistory *cpu, const char *ptr) {
  sscanf(ptr,
         CPUINFO(CPUINFO_FORMAT, CPUINFO_FORMAT_LAST),
         CPUINFO(CPUINFO_ARG, CPUINFO_ARG_LAST));
  cpu->curr.user_total = cpu->curr.user + cpu->curr.nice;
  cpu->curr.guest_total = cpu->curr.guest + cpu->curr.guest_nice;
}

static void get_stat(void) {
  if(!got_stat) {
    FILE *fp;
    char input[256], *ptr;
    size_t ncpu;

    ncpuinfos = 0;
    fp = xfopenf(NULL, "r", "%s/stat", proc);
    while(fgets(input, sizeof input, fp)) {
      if((ptr = strchr(input, ' '))) {
        *ptr++ = 0;
        if(input[0] == 'c' && input[1] == 'p' && input[2] == 'u') {
          /* Identify the CPU */
          if(input[3] == 0)
            ncpu = 0;
          else
            ncpu = strtoul(input + 3, NULL, 10) + 1;
          /* Make sure we have space */
          if(ncpu >= maxcpuinfos) {
            if(!(1 + ncpu))
              fatal(0, "too many CPUs");
            cpuinfos = xrecalloc(cpuinfos, 1 + ncpu, sizeof *cpuinfos);
            memset(&cpuinfos[maxcpuinfos], 0, (1 + ncpu - maxcpuinfos) * sizeof *cpuinfos);
            maxcpuinfos = 1 + ncpu;
          }
          if(ncpu >= ncpuinfos)
            ncpuinfos = 1 + ncpu;
          get_cpuinfo(&cpuinfos[ncpu], ptr);
        }
      }
    }
    fclose(fp);
    got_stat = 1;
  }
}

// ----------------------------------------------------------------------------

static void sysprop_format_time(intmax_t t, const char *format, struct buffer *b) {
  strfelapsed(b, format ? format : "%?+dd%02?+:H%02M:%02S", t);
}

// ----------------------------------------------------------------------------

static void sysprop_localtime(const struct sysinfo *si,
                              struct procinfo attribute((unused)) *pi,
                              struct buffer *b) {
  time_t now = timespec_now(NULL);
  struct tm now_tm;
  localtime_r(&now, &now_tm);
  buffer_strftime(b, si->arg ? si->arg : "%Y-%m-%d %H:%M:%S", &now_tm);
}

static void sysprop_processes(const struct sysinfo attribute((unused)) *si,
                              struct procinfo *pi,
                              struct buffer *b) {
  buffer_printf(b, "%d", proc_processes(pi));
}

static void sysprop_threads(const struct sysinfo attribute((unused)) *si,
                            struct procinfo *pi,
                            struct buffer *b) {
  buffer_printf(b, "%d", proc_threads(pi));
}

static void sysprop_uptime(const struct sysinfo *si,
                           struct procinfo attribute((unused)) *pi,
                           struct buffer *b) {
  sysprop_format_time(uptime_up(), si->arg, b);
}

static void sysprop_idletime(const struct sysinfo *si,
                             struct procinfo attribute((unused)) *pi,
                             struct buffer *b) {
  get_stat();
  sysprop_format_time(clock_to_seconds(cpuinfos[0].curr.idle), si->arg, b);
}

static void sysprop_load(const struct sysinfo attribute((unused)) *si,
                         struct procinfo attribute((unused)) *pi,
                         struct buffer *b) {
  FILE *fp;
  double l1, l2, l3;
  int prec;
  char *path;
  fp = xfopenf(&path, "r", "%s/loadavg", proc);
  if(fscanf(fp, "%lg %lg %lg", &l1, &l2, &l3) < 0)
    fatal(errno, "reading %s", path);
  fclose(fp);
  prec = si->arg ? atoi(si->arg) : 1;
  buffer_printf(b, "%.*f %.*f %.*f", prec, l1, prec, l2, prec, l3);
}

static void sysprop_mem(const struct sysinfo *si,
                        struct procinfo attribute((unused)) *pi,
                        struct buffer *b) {
  char btot[16], bused[16], bfree[16], bbuf[16], bcache[16];
  unsigned cutoff = 1;
  int ch = parse_byte_arg(si->arg, &cutoff, 0);

  get_meminfo();
  buffer_printf(b, "%s tot %s used %s free %s buf %s cache",
                bytes(meminfo.MemTotal * KILOBYTE, 9, ch,
                      btot, sizeof btot, cutoff),
                bytes((meminfo.MemTotal - meminfo.MemFree) * KILOBYTE, 9, ch,
                      bused, sizeof bused, cutoff),
                bytes(meminfo.MemFree * KILOBYTE, 9, ch,
                      bfree, sizeof bfree, cutoff),
                bytes(meminfo.Buffers * KILOBYTE, 9, ch,
                      bbuf, sizeof bbuf, cutoff),
                bytes(meminfo.Cached * KILOBYTE, 9, ch,
                      bcache, sizeof bcache, cutoff));
}

static void sysprop_swap(const struct sysinfo *si,
                         struct procinfo attribute((unused)) *pi,
                         struct buffer *b) {
  char btot[32], bused[32], bfree[32], bcache[32];
  unsigned cutoff = 1;
  int ch = parse_byte_arg(si->arg, &cutoff, 0);

  get_meminfo();
  buffer_printf(b, "%s tot %s used %s free %s cache",
                bytes(meminfo.SwapTotal * KILOBYTE, 9, ch,
                      btot, sizeof btot, cutoff),
                bytes((meminfo.SwapTotal - meminfo.SwapFree) * KILOBYTE, 9, ch,
                      bused, sizeof bused, cutoff),
                bytes(meminfo.SwapFree * KILOBYTE, 9, ch,
                      bfree, sizeof bfree, cutoff),
                bytes(meminfo.SwapCached * KILOBYTE, 9, ch,
                      bcache, sizeof bcache, cutoff));
}

static void sysprop_cpu_one(int prec,
                            const struct cpuhistory *cpu,
                            struct buffer *b) {
  int width = prec ? prec + 3 : 2;
  /* Figure out the total tick difference
   *
   * Observations based on the kernel:
   * - guest and guest_nice include user and nice respectively
   *   (account_guest_time)
   * - user does not include nice (account_user_time)
   * - idle does not include iowait (account_idle_time)
   * - irq, softirq and system are exclusive (account_system_time)
   */
  uintmax_t total = (cpu->curr.user_total + cpu->curr.system + cpu->curr.iowait
           + cpu->curr.idle + cpu->curr.irq + cpu->curr.softirq
           + cpu->curr.steal)
    - (cpu->last.user_total + cpu->last.system + cpu->last.iowait
       + cpu->last.idle + cpu->last.irq + cpu->last.softirq
       + cpu->last.steal);
  /* How to figure out the percentage differences */
#define CPUINFO_PCT(F) (100.0 * (cpu->curr.F - cpu->last.F) / total)
  /* Display them */
  if(total)
    buffer_printf(b, "%*.*f%% user %*.*f%% nice %*.*f%% guest %*.*f%% sys %*.*f%% io",
                  width, prec, CPUINFO_PCT(user_total),
                  width, prec, CPUINFO_PCT(nice),
                  width, prec, CPUINFO_PCT(guest_total),
                  width, prec, CPUINFO_PCT(system),
                  width, prec, CPUINFO_PCT(iowait));
  else
    buffer_printf(b, " -%% user   -%% nice -%% guest  -%% sys  -%% io");
  
}

static void sysprop_cpu(const struct sysinfo *si,
                        struct procinfo attribute((unused)) *pi,
                        struct buffer *b) {
  const int prec = si->arg && *si->arg ? atoi(si->arg) : 0;
  get_stat();
  if(ncpuinfos) {
    sysprop_cpu_one(prec, &cpuinfos[0], b);
  }
}

static void sysprop_cpus(const struct sysinfo attribute((unused)) *si,
                         struct procinfo attribute((unused)) *pi,
                         struct buffer *b) {
  const int prec = si->arg && *si->arg ? atoi(si->arg) : 0;
  size_t n;
  get_stat();
  for(n = 1; n < ncpuinfos; ++n) {
    if(n > 1)
      buffer_putc(b, '\n');
    buffer_printf(b, "CPU %zu:", n - 1);
    sysprop_cpu_one(prec, &cpuinfos[n], b);
  }
}


// ----------------------------------------------------------------------------

const struct sysprop sysproperties[] = {
  {
    "cpu", "CPU  ", "CPU usage",
    sysprop_cpu
  },
  {
    "cpus", NULL, "Per-CPU usage",
    sysprop_cpus
  },
  {
    "idletime", "Idle", "Cumulative time spent idle (argument: format string)",
    sysprop_idletime
  },
  {
    "load", "Load", "System load (integer argument: precision)",
    sysprop_load
  },
  {
    "mem", "RAM ", "Memory information (argument: K/M/G/T/P/p)",
    sysprop_mem
  },
  {
    "processes", "Procs", "Number of processes",
    sysprop_processes
  },
  {
    "swap", "Swap", "Swap information (argument: K/M/G/T/P/p)",
    sysprop_swap
  },
  {
    "threads", "Threads", "Number of threads",
    sysprop_threads
  },
  {
    "time", "Time", "Current (local) time (argument: strftime format string)",
    sysprop_localtime
  },
  {
    "uptime", "Up", "Time since system booted (argument: format string)",
    sysprop_uptime
  },
};
#define NSYSPROPERTIES (sizeof sysproperties / sizeof *sysproperties)

// ----------------------------------------------------------------------------

static size_t nsysinfos;
static struct sysinfo *sysinfos;

static const struct sysprop *sysinfo_find(const char *name) {
  size_t l = 0, r = NSYSPROPERTIES - 1, m;
  int c;
  while(l <= r) {
    m = l + (r - l) / 2;
    c = strcmp(name, sysproperties[m].name);
    if(c < 0)
      r = m - 1;
    else if(c > 0)
      l = m + 1;
    else
      return &sysproperties[m];
  }
  return NULL;
}

int sysinfo_set(const char *format, unsigned flags) {
  char *name, *heading, *arg;
  size_t i;
  const struct sysprop *prop;
  enum parse_status ps;

  if(!(flags & (FORMAT_CHECK|FORMAT_ADD))) {
    for(i = 0; i < nsysinfos; ++i) {
      free(sysinfos[i].heading);
      free(sysinfos[i].arg);
    }
    free(sysinfos);
    sysinfos = NULL;
    nsysinfos = 0;
  }
  while(!(ps = parse_element(&format, NULL, &name, NULL, &heading, &arg,
                             flags|FORMAT_QUOTED|FORMAT_HEADING|FORMAT_ARG))) {
    prop = sysinfo_find(name);
    if(flags & FORMAT_CHECK) {
      free(name);
      free(heading);
      free(arg);
      if(!prop)
        return 0;
    } else {
      if(!prop)
        fatal(0, "unknown system property '%s'", name);
      if((ssize_t)(nsysinfos + 1) < 0)
        fatal(0, "too many columns");
      sysinfos = xrecalloc(sysinfos, nsysinfos + 1, sizeof *sysinfos);
      sysinfos[nsysinfos].prop = prop;
      sysinfos[nsysinfos].heading = heading;
      sysinfos[nsysinfos].arg = arg;
      ++nsysinfos;
      free(name);
    }
  }
  return ps == parse_eof;
}

size_t sysinfo_reset(void) {
  size_t ncpu;

  got_meminfo = 0;
  got_stat = 0;
  for(ncpu = 0; ncpu < maxcpuinfos; ++ncpu) {
    cpuinfos[ncpu].last = cpuinfos[ncpu].curr;
    memset(&cpuinfos[ncpu].curr, 0, sizeof cpuinfos[ncpu].curr);
  }
  return nsysinfos;
}

int sysinfo_format(struct procinfo *pi, size_t n, struct buffer *b) {
  size_t i;
  int rc;
  b->pos = 0;
  if(n < nsysinfos) {
    const char *heading = sysinfos[n].heading;
    if(!heading)
      heading = sysinfos[n].prop->heading;
    if(heading && *heading) {
      /* Figure out how many spaces there are the end of the heading */
      for(i = strlen(heading); i > 0 && heading[i - 1] == ' '; --i)
        ;
      buffer_printf(b, "%.*s:%*s",
               (int)i, heading,
               (int)(strlen(heading) - i + 1), "");
    }
    sysinfos[n].prop->format(&sysinfos[n], pi, b);
    rc = 0;
  } else
    rc = -1;
  buffer_terminate(b);
  return rc;
}

char **sysinfo_help(void) {
  size_t n;
  char *ptr, **result, **next;

  next = result = xrecalloc(NULL, 2 + NSYSPROPERTIES, sizeof (char *));
  *next++ = xstrdup("  Property    Description");
  for(n = 0; n < NSYSPROPERTIES; ++n) {
    xasprintf(&ptr, "  %-10s  %s",
              sysproperties[n].name,
              sysproperties[n].description);
    *next++ = ptr;
  }
  *next = NULL;
  return result;
}

char *sysinfo_get(void) {
  size_t n;
  struct buffer b[1];
  buffer_init(b);
  for(n = 0; n < nsysinfos; ++n) {
    if(n)
      buffer_putc(b, ' ');
    buffer_append(b, sysinfos[n].prop->name);
    if(sysinfos[n].heading) {
      buffer_putc(b, '=');
      format_get_arg(b, sysinfos[n].heading, !!sysinfos[n].arg);
    }
    if(sysinfos[n].arg) {
      buffer_putc(b, '/');
      format_get_arg(b, sysinfos[n].arg, 0);
    }
  }
  buffer_terminate(b);
  return b->base;
}
