/*
 * This file is part of nps.
 * Copyright (C) 2012 Richard Kettlewell
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
#include "io.h"
#include "utils.h"
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static void snapshot_processes(const char *destdir);
static void snapshot_process(const char *destdir,
                             const char *pid, const char *tid);
static void snapshot_tasks(const char *destdir, const char *pid);
static void snapshot_file(const char *srcdir, const char *destdir, 
                          const char *name, int indent);

static int verbose;

enum {
  OPT_HELP = 256,
  OPT_VERSION,
};

const struct option options[] = {
  { "help", no_argument, 0, OPT_HELP },
  { "version", no_argument, 0, OPT_VERSION },
  { "delay", required_argument, 0, 's' },
  { "destination", required_argument, 0, 'd' },
  { "count", required_argument, 0, 'n' },
  { "proc", required_argument, 0, 'p' },
  { "verbose", no_argument, 0, 'v' },
  { 0, 0, 0, 0 },
};

int main(int argc, char **argv) {
  int delay = 1, count = 1, n;
  const char *destination = NULL;
  char *dest;

  while((n = getopt_long(argc, argv, "+s:d:n:p:v", options, NULL)) >= 0) {
    switch(n) {
    case OPT_HELP:
      xprintf(
"Usage:\n"
"  snapshot -d DESTINATION [OPTIONS]\n"
"Options:\n"
"  -s, --delay SECONDS     Delay between snapshots (default: 1)\n"
"  -d, --destination DIR   Destination directory (required)\n"
"  -n, --count COUNT       Number of snapshost to make (default: 1)\n"
"  -p, --proc DIR          Source directory (default: /proc)\n"
"  -v, --verbose           Verbose operation\n"
"  --help                  Display option summary\n"
"  --version               Display version string\n"
"\n"
"Snapshots the files used by nps and nps-top, in order to generate\n"
"test cases.\n");
      xexit(0);
    case OPT_VERSION:
      xprintf("%s\n", PACKAGE_VERSION);
      xexit(0);
    case 's':
      delay = atoi(optarg);
      break;
    case 'd':
      destination = optarg;
      break;
    case 'n':
      count = atoi(optarg);
      break;
    case 'p':
      proc = optarg;
      break;
    case 'v':
      verbose = 1;
      break;
    default:
      xexit(1);
    }
  }
  if(!destination)
    fatal(errno, "--destination option is required (try --help)");
  if(optind < argc)
    fatal(0, "excess options (try --help)");
  if(mkdir(destination, 0777) < 0)
    fatal(errno, "mkdir %s", destination);
  for(n = 0; n < count; ++n) {
    if(n)
      sleep(delay);
    if(verbose)
      xprintf("Iteration %d\n", n);
    xasprintf(&dest, "%s/%d", destination, n);
    if(verbose)
      xprintf("  mkdir %s\n", dest);
    xmkdir(dest, 0777);
    snapshot_file(proc, dest, "uptime", 0);
    snapshot_file(proc, dest, "meminfo", 0);
    snapshot_file(proc, dest, "stat", 0);
    snapshot_file(proc, dest, "loadavg", 0);
    snapshot_processes(dest);
  }
  return 0;
}

static void snapshot_processes(const char *destdir) {
  DIR *dp;
  struct dirent *de;
  if(verbose)
    xprintf("  scanning %s\n", proc);
  dp = opendirf(NULL, "%s", proc);
  if(!dp)
    fatal(errno, "opendir %s", proc);
  while((de = xreaddir(proc, dp)))
    if(isdigit((unsigned char)de->d_name[0])) {
      snapshot_process(destdir, de->d_name, NULL);
      snapshot_tasks(destdir, de->d_name);
    }
  closedir(dp);
}

static void snapshot_process(const char *destdir,
                             const char *pid, const char *tid) {
  char *procsrcdir, *procdestdir;
  if(tid) {
    if(verbose)
      xprintf("    thread %s\n", tid);
    xasprintf(&procsrcdir, "%s/%s/task/%s", proc, pid, tid);
    xasprintf(&procdestdir, "%s/%s/task/%s", destdir, pid, tid);
  } else {
    if(verbose)
      xprintf("  process %s\n", pid);
    xasprintf(&procsrcdir, "%s/%s", proc, pid);
    xasprintf(&procdestdir, "%s/%s", destdir, pid);
  }
  xmkdir(procdestdir, 0777);
  snapshot_file(procsrcdir, procdestdir, "stat", 1 + !!tid);
  snapshot_file(procsrcdir, procdestdir, "status", 1 + !!tid);
  snapshot_file(procsrcdir, procdestdir, "cmdline", 1 + !!tid);
  snapshot_file(procsrcdir, procdestdir, "oom_score", 1 + !!tid);
  if(!getuid()) {
    snapshot_file(procsrcdir, procdestdir, "io", 1 + !!tid);
    snapshot_file(procsrcdir, procdestdir, "smaps", 1 + !!tid);
  }
  free(procdestdir);
  free(procsrcdir);
}

static void snapshot_tasks(const char *destdir, const char *pid) {
  char *taskssrcdir, *tasksdestdir;
  struct dirent *de;
  DIR *dp;

  if(verbose)
    xprintf("    scanning %s/%s/task\n", proc, pid);
  dp = opendirf(&taskssrcdir, "%s/%s/task", proc, pid);
  if(!dp)
    fatal(errno, "opening %s", taskssrcdir);
  xasprintf(&tasksdestdir, "%s/%s/task", destdir, pid);
  xmkdir(tasksdestdir, 0777);
  while((de = xreaddir(taskssrcdir, dp)))
    if(isdigit((unsigned char)de->d_name[0]))
      snapshot_process(destdir, pid, de->d_name);
  closedir(dp);
  free(tasksdestdir);
  free(taskssrcdir);
}

static void snapshot_file(const char *srcdir, const char *destdir, 
                          const char *name, int indent) {
  char *inpath, *outpath;
  int c;
  FILE *in = xfopenf(&inpath, "rb", "%s/%s", srcdir, name);
  FILE *out = xfopenf(&outpath, "wb", "%s/%s", destdir, name);
  if(verbose)
    xprintf("%*s%s -> %s\n", 2 * (indent + 1), "", inpath, outpath);
  while((c = getc(in)) >= 0
        && putc(c, out) >= 0)
    ;
  if(ferror(in) || fclose(in) < 0)
    fatal(errno, "reading %s", inpath);
  if(ferror(out) || fclose(out) < 0)
    fatal(errno, "writing %s", outpath);
  free(inpath);
  free(outpath);
}
