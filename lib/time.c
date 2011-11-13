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
#include "utils.h"
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

time_t clock_to_time(unsigned long long ticks) {
  static time_t boot_time;

  if(!boot_time) {
    FILE *fp;
    long ssb;
    if(!(fp = fopen("/proc/uptime", "r")))
      fatal(errno, "opening /proc/uptime");
    if(fscanf(fp, "%ld", &ssb) != 1)
      fatal(errno, "reading /proc/uptime");
    fclose(fp);
    boot_time = time(NULL) - ssb;
  }
  return boot_time + clock_to_seconds(ticks);
}

double clock_to_seconds(unsigned long long ticks) {
  return (double)ticks / sysconf(_SC_CLK_TCK);
}
