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
#include "process.h"
#include "utils.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>

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

#define MEMINFO(X)                              \
    X(MemTotal, MemTotal)                       \
    X(MemFree, MemFree)                         \
    X(Buffers, Buffers)                         \
    X(Cached, Cached)                           \
    X(SwapCached, SwapCached)                   \
    X(Active, Active)                           \
    X(Inactive, Inactive)                       \
    X(Active(anon), Active_anon)                \
    X(Inactive(anon), Inactive_anon)            \
    X(Active(file), Active_file)                \
    X(Inactive(file), Inactive_file)            \
    X(Unevictable, Unevictable)                 \
    X(Mlocked, Mlocked)                         \
    X(SwapTotal, SwapTotal)                     \
    X(SwapFree, SwapFree)                       \
    X(Dirty, Dirty)                             \
    X(Writeback, Writeback)                     \
    X(AnonPages, AnonPages)                     \
    X(Mapped, Mapped)                           \
    X(Shmem, Shmem)                             \
    X(Slab, Slab)                               \
    X(SReclaimable, SReclaimable)               \
    X(SUnreclaim, SUnreclaim)                   \
    X(KernelStack, KernelStack)                 \
    X(PageTables, PageTables)                   \
    X(NFS_Unstable, NFS_Unstable)               \
    X(Bounce, Bounce)                           \
    X(WritebackTmp, WritebackTmp)               \
    X(CommitLimit, CommitLimit)                 \
    X(Committed_AS, Committed_AS)               \
    X(VmallocTotal, VmallocTotal)               \
    X(VmallocUsed, VmallocUsed)                 \
    X(VmallocChunk, VmallocChunk)               \
    X(HardwareCorrupted, HardwareCorrupted)     \
    X(HugePages_Total, HugePages_Total)         \
    X(HugePages_Free, HugePages_Free)           \
    X(HugePages_Rsvd, HugePages_Rsvd)           \
    X(HugePages_Surp, HugePages_Surp)           \
    X(Hugepagesize, Hugepagesize)               \
    X(DirectMap4k, DirectMap4k)                 \
    X(DirectMap2M, DirectMap2M)

#define FIELD(N,F) uintmax_t F;
#define OFFSET(N,F) offsetof(struct meminfo, F),
#define NAME(N,F) #N,
struct meminfo { MEMINFO(FIELD) };
static struct meminfo meminfo;

static const char *meminfo_names[] = { MEMINFO(NAME) };
static const size_t meminfo_offsets[] = { MEMINFO(OFFSET) };
#define NMEMINFOS (sizeof meminfo_names / sizeof *meminfo_names)

static void get_meminfo(void) {
  if(!got_meminfo) {
    FILE *fp;
    char input[128], *colon;
    size_t i = 0;
    memset(&meminfo, 0, sizeof meminfo);
    if(!(fp = fopen("/proc/meminfo", "r")))
      fatal(errno, "opening /proc/meminfo");
    while(fgets(input, sizeof input, fp)) {
      if((colon = strchr(input, ':'))) {
        *colon++ = 0;
        if(i < NMEMINFOS && !strcmp(meminfo_names[i], input)) {
          uintmax_t *ptr = (uintmax_t *)((char *)&meminfo + meminfo_offsets[i]);
          *ptr = strtoull(colon, &colon, 10);
        }
        i++;
      }
    }
    fclose(fp);
    
    got_meminfo = 1;
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
  snprintf(buffer, bufsize, "Swap: %9ju tot %9ju used %9ju free",
           meminfo.SwapTotal,
           meminfo.SwapTotal - meminfo.SwapFree,
           meminfo.SwapFree);
}

static void sysprop_swapM(struct procinfo attribute((unused)) *pi,
                         char *buffer, size_t bufsize) {
  get_meminfo();
  snprintf(buffer, bufsize, "Swap: %8juM tot %8juM used %8juM free",
           meminfo.SwapTotal / 1024,
           (meminfo.SwapTotal - meminfo.SwapFree) / 1024,
           meminfo.SwapFree / 1024);
}

// ----------------------------------------------------------------------------

const struct sysprop {
  const char *name;
  const char *description;
  void (*format)(struct procinfo *pi, char *buffer, size_t bufsize);
} sysproperties[] = {
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
  fatal(0, "unknown system property '%s'", name);
}

void sysinfo_format(const char *format) {
  char buffer[128];
  size_t i;
  free(sysinfos);
  sysinfos = NULL;
  nsysinfos = 0;
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
    if((ssize_t)(nsysinfos + 1) < 0)
      fatal(0, "too many columns");
    sysinfos = xrecalloc(sysinfos, nsysinfos + 1, sizeof *sysinfos);
    sysinfos[nsysinfos].prop = sysinfo_find(buffer);
    ++nsysinfos;
  }
}

size_t sysinfo_reset(void) {
  got_uptime = 0;
  got_meminfo = 0;
  return nsysinfos;
}

int sysinfo_get(struct procinfo *pi, size_t n, char buffer[], size_t bufsize) {
  buffer[0] = 0;
  if(n < nsysinfos) {
    sysinfos[n].prop->format(pi, buffer, bufsize);
    return 0;
  } else
    return -1;
}

void sysinfo_help(void) {
  size_t n;
  printf("  Property    Description\n");
  for(n = 0; n < NSYSPROPERTIES; ++n)
    printf("  %-10s  %s\n",
           sysproperties[n].name,
           sysproperties[n].description);
}
