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

static void parse_uptime(double *up, double *idle) {
  FILE *fp;
  double junk;
  if(!(fp = fopen("/proc/uptime", "r")))
    fatal(errno, "opening /proc/uptime");
  if(fscanf(fp, "%lg %lg", up ? up : &junk, idle ? idle : &junk) < 0)
    fatal(errno, "reading /proc/uptime");
  fclose(fp);
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

static void sysprop_processes(struct procinfo *pi, char *buffer, size_t bufsize) {
  snprintf(buffer, bufsize, "Tasks: %d", proc_count(pi));
}

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

static void sysprop_uptime(struct procinfo attribute((unused)) *pi,
                           char *buffer, size_t bufsize) {
  double u;
  parse_uptime(&u, NULL);
  sysprop_format_time("Up", u, buffer, bufsize);
}

static void sysprop_idletime(struct procinfo attribute((unused)) *pi,
                             char *buffer, size_t bufsize) {
  double i;
  parse_uptime(&i, NULL);
  sysprop_format_time("Idle", i, buffer, bufsize);
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
    "processes", "Number of processes",
    sysprop_processes
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

// ----------------------------------------------------------------------------

static size_t nsysprops;
static const struct sysprop **sysprops;

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
  free(sysprops);
  sysprops = NULL;
  nsysprops = 0;
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
    if((ssize_t)(nsysprops + 1) < 0)
      fatal(0, "too many columns");
    sysprops = xrecalloc(sysprops, nsysprops + 1, sizeof *sysprops);
    sysprops[nsysprops] = sysinfo_find(buffer);
    ++nsysprops;
  }
}

size_t sysinfo_reset(void) {
  return nsysprops;
}

int sysinfo_get(struct procinfo *pi, size_t n, char buffer[], size_t bufsize) {
  buffer[0] = 0;
  if(n < nsysprops) {
    sysprops[n]->format(pi, buffer, bufsize);
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
