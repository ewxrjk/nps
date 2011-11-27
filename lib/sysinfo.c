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

struct sysinfo;

struct sysprop {
  const char *name;
  const char *heading;
  const char *description;
  void (*format)(const struct sysinfo *sp,
                 struct procinfo *pi, char *buffer, size_t bufsize);
};

struct sysinfo {
  const struct sysprop *prop;
  char *arg;
  char *heading;
};

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
    if(!(fp = fopen("/proc/stat", "r")))
      fatal(errno, "opening /proc/stat");
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

static void sysprop_format_time(double t, char *buffer, size_t bufsize) {
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
    snprintf(buffer, bufsize, "%jdd %d:%02d", d, h, m);
  else
    snprintf(buffer, bufsize, "%d:%02d:%02d", h, m, s);
}

// ----------------------------------------------------------------------------

static void sysprop_localtime(const struct sysinfo *si,
                              struct procinfo attribute((unused)) *pi,
                              char *buffer, size_t bufsize) {
  time_t now;
  struct tm now_tm;
  time(&now);
  localtime_r(&now, &now_tm);
  strftime(buffer, bufsize,
           si->arg ? si->arg : "%Y-%m-%d %H:%M:%S", &now_tm);
}

static void sysprop_processes(const struct sysinfo attribute((unused)) *si,
                              struct procinfo *pi,
                              char *buffer, size_t bufsize) {
  snprintf(buffer, bufsize, "%d", proc_count(pi));
}

static void sysprop_uptime(const struct sysinfo attribute((unused)) *si,
                              struct procinfo attribute((unused)) *pi,
                           char *buffer, size_t bufsize) {
  get_uptime();
  sysprop_format_time(up, buffer, bufsize);
}

static void sysprop_idletime(const struct sysinfo attribute((unused)) *si,
                              struct procinfo attribute((unused)) *pi,
                             char *buffer, size_t bufsize) {
  get_uptime();
  sysprop_format_time(idle, buffer, bufsize);
}

static void sysprop_load(const struct sysinfo attribute((unused)) *si,
                         struct procinfo attribute((unused)) *pi,
                         char *buffer, size_t bufsize) {
  FILE *fp;
  double l1, l2, l3;
  int prec;
  if(!(fp = fopen("/proc/loadavg", "r")))
    fatal(errno, "opening /proc/loadavg");
  if(fscanf(fp, "%lg %lg %lg", &l1, &l2, &l3) < 0)
    fatal(errno, "reading /proc/loadavg");
  fclose(fp);
  prec = si->arg ? atoi(si->arg) : 1;
  snprintf(buffer, bufsize, "%.*f %.*f %.*f", prec, l1, prec, l2, prec, l3);
}

static void sysprop_mem(const struct sysinfo *si,
                        struct procinfo attribute((unused)) *pi,
                        char *buffer, size_t bufsize) {
  const int ch = si->arg ? *si->arg : 0;
  char btot[16], bused[16], bfree[16], bbuf[16], bcache[16];

  get_meminfo();
  snprintf(buffer, bufsize, "%s tot %s used %s free %s buf %s cache",
           bytes(meminfo.MemTotal * 1024, 9, ch,
                 btot, sizeof btot),
           bytes((meminfo.MemTotal - meminfo.MemFree) * 1024, 9, ch,
                 bused, sizeof bused),
           bytes(meminfo.MemFree * 1024, 9, ch,
                 bfree, sizeof bfree),
           bytes(meminfo.Buffers * 1024, 9, ch,
                 bbuf, sizeof bbuf),
           bytes(meminfo.Cached * 1024, 9, ch,
                 bcache, sizeof bcache));
}

static void sysprop_swap(const struct sysinfo *si,
                         struct procinfo attribute((unused)) *pi,
                         char *buffer, size_t bufsize) {
  const int ch = si->arg ? *si->arg : 0;
  char btot[32], bused[32], bfree[32], bcache[32];

  get_meminfo();
  snprintf(buffer, bufsize, "%s tot %s used %s free %s cache",
           bytes(meminfo.SwapTotal * 1024, 9, ch,
                 btot, sizeof btot),
           bytes((meminfo.SwapTotal - meminfo.SwapFree) * 1024, 9, ch,
                 bused, sizeof bused),
           bytes(meminfo.SwapFree * 1024, 9, ch,
                 bfree, sizeof bfree),
           bytes(meminfo.Cached * 1024, 9, ch,
                 bcache, sizeof bcache));
}

static void sysprop_cpu_one(const struct cpuhistory *cpu,
                            char buffer[], size_t bufsize) {
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
#define CPUINFO_PCT(F) (int)(100 * (cpu->curr.F - cpu->last.F) / total)
  /* Display them */
  if(total)
    snprintf(buffer, bufsize, "%2d%% user %2d%% nice %2d%% guest %2d%% sys %2d%% io",
             CPUINFO_PCT(user_total),
             CPUINFO_PCT(nice),
             CPUINFO_PCT(guest_total),
             CPUINFO_PCT(system),
             CPUINFO_PCT(iowait));
  else
    snprintf(buffer, bufsize, " -%% user   -%% nice -%% guest  -%% sys  -%% io");
    
}

static void sysprop_cpu(const struct sysinfo attribute((unused)) *si,
                        struct procinfo attribute((unused)) *pi,
                        char *buffer, size_t bufsize) {
  get_stat();
  if(ncpuinfos) {
    sysprop_cpu_one(&cpuinfos[0], buffer, bufsize);
  }
}

static void sysprop_cpus(const struct sysinfo attribute((unused)) *si,
                         struct procinfo attribute((unused)) *pi,
                         char *buffer, size_t bufsize) {
  size_t n;
  get_stat();
  for(n = 1; n < ncpuinfos; ++n) {
    if(n > 1) {
      snprintf(buffer, bufsize, "\n");
      bufsize -= strlen(buffer);
      buffer += strlen(buffer);
    }
    snprintf(buffer, bufsize, "CPU %zu:", n - 1);
    bufsize -= strlen(buffer);
    buffer += strlen(buffer);
    sysprop_cpu_one(&cpuinfos[n], buffer, bufsize);
    bufsize -= strlen(buffer);
    buffer += strlen(buffer);
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
    "idletime", "Idle", "Cumulative time spent idle",
    sysprop_idletime
  },
  {
    "load", "Load", "System load (integer argument: precision)",
    sysprop_load
  },
  {
    "mem", "RAM ", "Memory information (argument: K/M/G/T/p)",
    sysprop_mem
  },
  {
    "processes", "Tasks", "Number of processes",
    sysprop_processes
  },
  {
    "swap", "Swap", "Swap information (argument: K/M/G/T/p)",
    sysprop_swap
  },
  {
    "time", "Time", "Current (local) time (argument: strftime format string)",
    sysprop_localtime
  },
  {
    "uptime", "Up", "Time since system booted",
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
  char buffer[128], heading_buffer[128], *heading, arg_buffer[128], *arg;
  size_t i;
  const struct sysprop *prop;

  if(!(flags & (FORMAT_CHECK|FORMAT_ADD))) {
    for(i = 0; i < nsysinfos; ++i)
      free(sysinfos[i].arg);
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
    while(*format && *format != ' ' && *format != ','
          && *format != '/' && *format != '=') {
      if(i < sizeof buffer - 1)
        buffer[i++] = *format;
      ++format;
    }
    buffer[i] = 0;
    if(*format == '=') {
      format = format_parse_arg(format + 1, heading_buffer, sizeof heading_buffer,
                                flags|FORMAT_QUOTED);
      heading = heading_buffer;
    } else
      heading = NULL;
    if(*format == '/') {
      format = format_parse_arg(format + 1, arg_buffer, sizeof arg_buffer,
                                flags|FORMAT_QUOTED);
      arg = arg_buffer;
    } else
      arg = NULL;
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
      sysinfos[nsysinfos].heading = heading ? xstrdup(heading) : NULL;
      sysinfos[nsysinfos].arg = arg ? xstrdup(arg) : NULL;
      ++nsysinfos;
    }
  }
  return 1;
}

size_t sysinfo_reset(void) {
  size_t ncpu;

  got_uptime = 0;
  got_meminfo = 0;
  got_stat = 0;
  for(ncpu = 0; ncpu < maxcpuinfos; ++ncpu) {
    cpuinfos[ncpu].last = cpuinfos[ncpu].curr;
    memset(&cpuinfos[ncpu].curr, 0, sizeof cpuinfos[ncpu].curr);
  }
  return nsysinfos;
}

int sysinfo_format(struct procinfo *pi, size_t n, char buffer[], size_t bufsize) {
  size_t i;
  buffer[0] = 0;
  if(n < nsysinfos) {
    const char *heading = sysinfos[n].heading;
    if(!heading)
      heading = sysinfos[n].prop->heading;
    if(heading && *heading) {
      /* Figure out how many spaces there are the end of the heading */
      for(i = strlen(heading); i > 0 && heading[i - 1] == ' '; --i)
        ;
      snprintf(buffer, bufsize, "%.*s:%*s",
               (int)i, heading,
               (int)(strlen(heading) - i + 1), "");
      bufsize -= strlen(buffer);
      buffer += strlen(buffer);
    }
    sysinfos[n].prop->format(&sysinfos[n], pi, buffer, bufsize);
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
  for(n = 0; n < nsysinfos; ++n) {
    size += strlen(sysinfos[n].prop->name) + 1;
    if(sysinfos[n].arg)
      size += strlen(sysinfos[n].arg) * 2 + 3;
  }
  ptr = buffer = xmalloc(size);
  for(n = 0; n < nsysinfos; ++n) {
    if(n)
      *ptr++ = ' ';
    strcpy(ptr, sysinfos[n].prop->name);
    ptr += strlen(ptr);
    if(sysinfos[n].heading) {
      *ptr++ = '=';
      ptr = format_get_arg(ptr, sysinfos[n].heading, !!sysinfos[n].arg);
    }
    if(sysinfos[n].arg) {
      *ptr++ = '/';
      ptr = format_get_arg(ptr, sysinfos[n].arg, 0);
    }
  }
  *ptr = 0;
  return buffer;
}
