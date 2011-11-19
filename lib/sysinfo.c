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
#include "sysinfo.h"
#include "format.h"
#include "process.h"
#include "utils.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#include <sys/time.h>

static int got_uptime;
static double up, idle;

static void get_uptime(void) {
  if(!got_uptime) {
    FILE *fp;
    up = idle = 0;
    if(!(fp = fopen("/proc/uptime", "r")))
      fatal(errno, "opening /proc/uptime");
    if(fscanf(fp, "%lg %lg", &up, &idle) < 0)
      fatal(errno, "reading /proc/uptime");
    fclose(fp);
    got_uptime = 1;
  }
}

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
    if(!(fp = fopen("/proc/meminfo", "r")))
      fatal(errno, "opening /proc/meminfo");
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
#define CPUINFO_ARG(F) &cpuinfo.F,
#define CPUINFO_ARG_LAST(F) &cpuinfo.F

static int got_stat;
static struct cpuinfo {
  CPUINFO(CPUINFO_FIELD,CPUINFO_FIELD_LAST);
  uintmax_t user_total;
  uintmax_t guest_total;
} cpuinfo, cpuinfo_last;

static void get_stat(void) {
  if(!got_stat) {
    FILE *fp;
    char input[256], *ptr;
    memset(&cpuinfo, 0, sizeof cpuinfo);
    if(!(fp = fopen("/proc/stat", "r")))
      fatal(errno, "opening /proc/stat");
    while(fgets(input, sizeof input, fp)) {
      if((ptr = strchr(input, ' '))) {
        *ptr++ = 0;
        if(!strcmp(input, "cpu")) {
          sscanf(ptr,
                 CPUINFO(CPUINFO_FORMAT, CPUINFO_FORMAT_LAST),
                 CPUINFO(CPUINFO_ARG, CPUINFO_ARG_LAST));
          cpuinfo.user_total = cpuinfo.user + cpuinfo.nice;
          cpuinfo.guest_total = cpuinfo.guest + cpuinfo.guest_nice;
        }
      }
    }
    fclose(fp);
    got_stat = 1;
  }
}

// ----------------------------------------------------------------------------

static void sysprop_format_time(const char *what, double t,
                                char *buffer, size_t bufsize) {
  double id;
  intmax_t i, d;
  int h, m, s;
  modf(t, &id);
  i = id;
  d = i / 86400;
  s = i % 86400;
  h = s / 3600;
  s %= 3600;
  m = s / 60;
  s %= 60;
  if(d)
    snprintf(buffer, bufsize, "%s: %jdd %d:%02d", what, d, h, m);
  else
    snprintf(buffer, bufsize, "%s: %d:%02d:%02d", what, h, m, s);
}

// ----------------------------------------------------------------------------

static void sysprop_localtime(struct procinfo attribute((unused)) *pi,
                              char *buffer, size_t bufsize) {
  time_t now;
  struct tm now_tm;
  time(&now);
  localtime_r(&now, &now_tm);
  strftime(buffer, bufsize, "Time: %Y-%m-%d %H:%M:%S", &now_tm);
}

static void sysprop_processes(struct procinfo *pi,
                              char *buffer, size_t bufsize) {
  snprintf(buffer, bufsize, "Tasks: %d", proc_count(pi));
}

static void sysprop_uptime(struct procinfo attribute((unused)) *pi,
                           char *buffer, size_t bufsize) {
  get_uptime();
  sysprop_format_time("Up", up, buffer, bufsize);
}

static void sysprop_idletime(struct procinfo attribute((unused)) *pi,
                             char *buffer, size_t bufsize) {
  get_uptime();
  sysprop_format_time("Idle", idle, buffer, bufsize);
}

static void sysprop_load(struct procinfo attribute((unused)) *pi,
                         char *buffer, size_t bufsize) {
  FILE *fp;
  double l1, l2, l3;
  if(!(fp = fopen("/proc/loadavg", "r")))
    fatal(errno, "opening /proc/loadavg");
  if(fscanf(fp, "%lg %lg %lg", &l1, &l2, &l3) < 0)
    fatal(errno, "reading /proc/loadavg");
  fclose(fp);
  snprintf(buffer, bufsize, "Load: %.1f %.1f %.1f", l1, l2, l3);
}

static void sysprop_mem(struct procinfo attribute((unused)) *pi,
                        char *buffer, size_t bufsize) {
  get_meminfo();
  snprintf(buffer, bufsize, "RAM:  %9ju tot %9ju used %9ju free %9ju buf %9ju cache",
           meminfo.MemTotal,
           meminfo.MemTotal - meminfo.MemFree,
           meminfo.MemFree,
           meminfo.Buffers,
           meminfo.Cached);
}

static void sysprop_memM(struct procinfo attribute((unused)) *pi,
                        char *buffer, size_t bufsize) {
  get_meminfo();
  snprintf(buffer, bufsize, "RAM:  %8juM tot %8juM used %8juM free %8juM buf %8juM cache",
           meminfo.MemTotal / 1024,
           (meminfo.MemTotal - meminfo.MemFree) / 1024,
           meminfo.MemFree / 1024,
           meminfo.Buffers / 1024,
           meminfo.Cached / 1024);
}

static void sysprop_swap(struct procinfo attribute((unused)) *pi,
                         char *buffer, size_t bufsize) {
  get_meminfo();
  snprintf(buffer, bufsize, "Swap: %9ju tot %9ju used %9ju free %9ju cache",
           meminfo.SwapTotal,
           meminfo.SwapTotal - meminfo.SwapFree,
           meminfo.SwapFree,
           meminfo.SwapCached);
}

static void sysprop_swapM(struct procinfo attribute((unused)) *pi,
                          char *buffer, size_t bufsize) {
  get_meminfo();
  snprintf(buffer, bufsize, "Swap: %8juM tot %8juM used %8juM free %8juM cache",
           meminfo.SwapTotal / 1024,
           (meminfo.SwapTotal - meminfo.SwapFree) / 1024,
           meminfo.SwapFree / 1024,
           meminfo.SwapCached / 1024);
}

static void sysprop_cpu(struct procinfo attribute((unused)) *pi,
                        char *buffer, size_t bufsize) {
  uintmax_t total = 0;

  get_stat();
  /* Figure out the total tick difference
   *
   * Observations based on the kernel:
   * - guest and guest_nice include user and nice respectively
   *   (account_guest_time)
   * - user does not include nice (account_user_time)
   * - idle does not include iowait (account_idle_time)
   * - irq, softirq and system are exclusive (account_system_time)
   */
  total = (cpuinfo.user_total + cpuinfo.system + cpuinfo.iowait
           + cpuinfo.idle + cpuinfo.irq + cpuinfo.softirq
           + cpuinfo.steal)
    - (cpuinfo_last.user_total + cpuinfo_last.system + cpuinfo_last.iowait
       + cpuinfo_last.idle + cpuinfo_last.irq + cpuinfo_last.softirq
       + cpuinfo_last.steal);
  /* How to figure out the percentage differences */
#define CPUINFO_PCT(F) (int)(100 * (cpuinfo.F - cpuinfo_last.F) / total)
  /* Display them */
  snprintf(buffer, bufsize, "CPU: %2d%% user %2d%% nice %2d%% guest %2d%% sys %2d%% io",
           CPUINFO_PCT(user_total),
           CPUINFO_PCT(nice),
           CPUINFO_PCT(guest_total),
           CPUINFO_PCT(system),
           CPUINFO_PCT(iowait));
}

// ----------------------------------------------------------------------------

const struct sysprop {
  const char *name;
  const char *description;
  void (*format)(struct procinfo *pi, char *buffer, size_t bufsize);
} sysproperties[] = {
  {
    "cpu", "CPU usage",
    sysprop_cpu
  },
  {
    "idletime", "Cumulative time spent idle",
    sysprop_idletime
  },
  {
    "load", "System load",
    sysprop_load
  },
  {
    "mem", "Memory information (kilobytes)",
    sysprop_mem
  },
  {
    "memM", "Memory information (megabytes)",
    sysprop_memM
  },
  {
    "processes", "Number of processes",
    sysprop_processes
  },
  {
    "swap", "Swap information (kilobytes)",
    sysprop_swap
  },
  {
    "swapM", "Swap information (megabytes)",
    sysprop_swapM
  },
  {
    "time", "Current (local) time",
    sysprop_localtime
  },
  {
    "uptime", "Time since system booted",
    sysprop_uptime
  },
};
#define NSYSPROPERTIES (sizeof sysproperties / sizeof *sysproperties)

struct sysinfo {
  const struct sysprop *prop;
};

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
  char buffer[128];
  size_t i;
  const struct sysprop *prop;

  if(!(flags & (FORMAT_CHECK|FORMAT_ADD))) {
    free(sysinfos);
    sysinfos = NULL;
    nsysinfos = 0;
  }
  while(*format) {
    if(*format == ' ' || *format == ',') {
      ++format;
      continue;
    }
    i = 0;
    while(*format && *format != ' ' && *format != ',') {
      if(i < sizeof buffer - 1)
        buffer[i++] = *format;
      ++format;
    }
    buffer[i] = 0;
    prop = sysinfo_find(buffer);
    if(flags & FORMAT_CHECK) {
      if(!prop)
        return 0;
    } else {
      if(!prop)
        fatal(0, "unknown system property '%s'", buffer);
      if((ssize_t)(nsysinfos + 1) < 0)
        fatal(0, "too many columns");
      sysinfos = xrecalloc(sysinfos, nsysinfos + 1, sizeof *sysinfos);
      sysinfos[nsysinfos].prop = prop;
      ++nsysinfos;
    }
  }
  return 1;
}

size_t sysinfo_reset(void) {
  got_uptime = 0;
  got_meminfo = 0;
  got_stat = 0;
  cpuinfo_last = cpuinfo;
  return nsysinfos;
}

int sysinfo_format(struct procinfo *pi, size_t n, char buffer[], size_t bufsize) {
  buffer[0] = 0;
  if(n < nsysinfos) {
    sysinfos[n].prop->format(pi, buffer, bufsize);
    return 0;
  } else
    return -1;
}

char **sysinfo_help(void) {
  size_t n = 0, size = 128;
  char *ptr, **result, **next;

  for(n = 0; n < NSYSPROPERTIES; ++n)
    size += max(strlen(sysproperties[n].name), 10)
      + strlen(sysproperties[n].description)
      + 10;
  next = result = xmalloc(sizeof (char *) * (2 + NSYSPROPERTIES) + size);
  ptr = (char *)(result + 2 + NSYSPROPERTIES);
  *next++ = strcpy(ptr, "  Property    Description");
  ptr += strlen(ptr) + 1;
  for(n = 0; n < NSYSPROPERTIES; ++n) {
    *next++ = ptr;
    ptr += 1 + sprintf(ptr, "  %-10s  %s",
                       sysproperties[n].name,
                       sysproperties[n].description);
  }
  *next = NULL;
  return result;
}

char *sysinfo_get(void) {
  size_t size = 10, n;
  char *buffer, *ptr;
  for(n = 0; n < nsysinfos; ++n)
    size += strlen(sysinfos[n].prop->name) + 1;
  ptr = buffer = xmalloc(size);
  for(n = 0; n < nsysinfos; ++n) {
    if(n)
      *ptr++ = ' ';
    strcpy(ptr, sysinfos[n].prop->name);
    ptr += strlen(ptr);
  }
  *ptr = 0;
  return buffer;
}
