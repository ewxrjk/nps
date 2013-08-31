/*
 * This file is part of nps.
 * Copyright (C) 2012,13 Richard Kettlewell
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
#include "io.h"
#include "utils.h"
#include <stdlib.h>
#include <errno.h>

static double boot_time;
static double idle_time;

static void uptime_init(void) {
  if(!boot_time) {
    FILE *fp;
    char *path;
    double up;
    fp = xfopenf(&path, "r", "%s/uptime", proc);
    if(fscanf(fp, "%lg %lg", &up, &idle_time) != 2)
      fatal(errno, "reading %s", path);
    fclose(fp);
    free(path);
    boot_time = clock_now() - up;
  }
  
}

double uptime_up(void) {
  uptime_init();
  return clock_now() - uptime_booted();
}

double uptime_booted(void) {
  uptime_init();
  return boot_time;
}
