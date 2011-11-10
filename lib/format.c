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
#include "format.h"
#include "buffer.h"
#include "process.h"
#include "utils.h"
#include <string.h>
#include <inttypes.h>
#include <stdio.h>
#include <pwd.h>
#include <grp.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>

// ----------------------------------------------------------------------------

typedef void formatfn(struct buffer *b, pid_t pid);

struct column {
  size_t id;
  size_t width;
  char *heading;
};

static size_t ncolumns;
static struct column *columns;

// ----------------------------------------------------------------------------

static void format_integer(intmax_t im, struct buffer *b) {
  char t[64];
  snprintf(t, sizeof t, "%jd", im);
  buffer_append(b, t);
}

static void format_octal(uintmax_t im, struct buffer *b) {
  char t[64];
  snprintf(t, sizeof t, "%jo", im);
  buffer_append(b, t);
}

static void format_hex(uintmax_t im, struct buffer *b) {
  char t[64];
  if(im > 0xFFFFFFFFFFFFLL)
    snprintf(t, sizeof t, "%016jx", im);
  else if(im > 0xFFFFFFFF)
    snprintf(t, sizeof t, "%012jx", im);
  else
    snprintf(t, sizeof t, "%08jx", im);
  buffer_append(b, t);
}

static void format_user(uid_t uid, struct buffer *b) {
  struct passwd *pw = getpwuid(uid);
  if(pw)
    buffer_append(b, pw->pw_name);
  else
    format_integer(uid, b);
}

static void format_group(gid_t gid, struct buffer *b) {
  struct group *gr = getgrgid(gid);
  if(gr)
    buffer_append(b, gr->gr_name);
  else
    return format_integer(gid, b);
}

static void format_interval(long seconds, struct buffer *b,
                            int always_hours) {
  char t[64];
  if(seconds >= 86400) {
    snprintf(t, sizeof t, "%ld-%02ld:%02ld:%02ld",
             seconds / 86400,
             (seconds % 86400) / 3600,
             (seconds % 3600) / 60,
             (seconds % 60));
  } else if(seconds >= 3600 || always_hours) {
    snprintf(t, sizeof t, "%02ld:%02ld:%02ld",
             seconds / 3600,
             (seconds % 3600) / 60,
             (seconds % 60));
  } else {
    snprintf(t, sizeof t, "%02ld:%02ld",
             seconds / 60,
             (seconds % 60));
  }
  buffer_append(b, t);
}

static void format_time(time_t when, struct buffer *b) {
  char t[64];
  time_t now;
  struct tm when_tm, now_tm;

  time(&now);
  localtime_r(&when, &when_tm);
  localtime_r(&now, &now_tm);
  if(now_tm.tm_year == when_tm.tm_year
     && now_tm.tm_mon == when_tm.tm_mon
     && now_tm.tm_mday == when_tm.tm_mday)
    strftime(t, sizeof t, "%H:%M:%S", &when_tm);
  else
    strftime(t, sizeof t, "%Y-%m-%d", &when_tm);
  buffer_append(b, t);
}

// ----------------------------------------------------------------------------

static void property_ruser(struct buffer *b, pid_t pid) {
  return format_user(proc_get_ruid(pid), b);
}

static void property_euser(struct buffer *b, pid_t pid) {
  return format_user(proc_get_euid(pid), b);
}

static void property_rgroup(struct buffer *b, pid_t pid) {
  return format_group(proc_get_rgid(pid), b);
}

static void property_egroup(struct buffer *b, pid_t pid) {
  return format_group(proc_get_egid(pid), b);
}

static void property_ruid(struct buffer *b, pid_t pid) {
  return format_integer(proc_get_ruid(pid), b);
}

static void property_euid(struct buffer *b, pid_t pid) {
  return format_integer(proc_get_euid(pid), b);
}

static void property_rgid(struct buffer *b, pid_t pid) {
  return format_integer(proc_get_rgid(pid), b);
}

static void property_egid(struct buffer *b, pid_t pid) {
  return format_integer(proc_get_egid(pid), b);
}

static void property_pid(struct buffer *b, pid_t pid) {
  return format_integer(pid, b);
}

static void property_ppid(struct buffer *b, pid_t pid) {
  return format_integer(proc_get_ppid(pid), b);
}

static void property_pgid(struct buffer *b, pid_t pid) {
  return format_integer(proc_get_pgid(pid), b);
}

static void property_tty(struct buffer *b, pid_t pid) {
  const char *path;
  int tty = proc_get_tty(pid);
  if(tty <= 0) {
    buffer_putc(b, '-');
    return;
  }
  path = device_path(0, tty);
  if(!path) {
    format_hex(tty, b);
    return;
  }
  /* "On XSI-conformant systems, they shall be given in one of two
   * forms: the device's filename (for example, tty04) or, if the
   * device's filename starts with tty, just the identifier following
   * the characters tty (for example, "04")" */
  if(!strncmp(path, "/dev/", 5))
    path += 5;
  if(!strncmp(path, "tty", 3))
    path += 3;
  buffer_append(b, path);
}

static void property_time(struct buffer *b, pid_t pid) {
  /* time wants [dd-]hh:mm:ss */
  return format_interval(proc_get_scheduled_time(pid), b, 1);
}

static void property_etime(struct buffer *b, pid_t pid) {
  /* etime wants [[dd-]hh:]mm:ss */
  return format_interval(proc_get_elapsed_time(pid), b, 0);
}

static void property_stime(struct buffer *b, pid_t pid) {
  return format_time(proc_get_start_time(pid), b);
}

static void property_comm_args(struct buffer *b, pid_t pid, 
                               const char *(*get)(pid_t)) {
  char *t;
  const char *comm = get(pid);
  /* "A process that has exited and has a parent, but has not yet been
   * waited for by the parent, shall be marked defunct." */
  if(proc_get_state(pid) != 'Z')
    buffer_append(b, comm);
  else {
    if(asprintf(&t, "%s <defunct>", comm) < 0)
      fatal(errno, "asprintf");
    buffer_append(b, t);
    free(t);
  }
}

static void property_comm(struct buffer *b, pid_t pid) {
  return property_comm_args(b, pid, proc_get_comm);
}

static void property_args(struct buffer *b, pid_t pid) {
  return property_comm_args(b, pid, proc_get_cmdline);
}

static void property_flags(struct buffer *b, pid_t pid) {
  return format_octal(proc_get_flags(pid), b);
}

static void property_nice(struct buffer *b, pid_t pid) {
  format_integer(proc_get_nice(pid), b);
}

static void property_pri(struct buffer *b, pid_t pid) {
  format_integer(proc_get_priority(pid), b);
}

static void property_state(struct buffer *b, pid_t pid) {
  buffer_putc(b, proc_get_state(pid));
}

static void property_pcpu(struct buffer *b, pid_t pid) {
  format_integer(proc_get_pcpu(pid), b);
}

static void property_vsize(struct buffer *b, pid_t pid) {
  format_integer(proc_get_vsize(pid) / 1024, b);
}

static void property_rss(struct buffer *b, pid_t pid) {
  format_integer(proc_get_rss(pid) / 1024, b);
}

static void property_addr(struct buffer *b, pid_t pid) {
  format_hex(proc_get_insn_pointer(pid), b);
}

static void property_wchan(struct buffer *b, pid_t pid) {
  unsigned long long wchan = proc_get_wchan(pid);
  if(wchan && wchan + 1)
    format_hex(wchan, b);
  else
    /* suppress pointless values */
    buffer_putc(b, '-');
}

// ----------------------------------------------------------------------------

static const struct {
  const char *name;
  const char *heading;
  formatfn *property;
  const char *description;
} properties[] = {
  { "addr", "ADDR", property_addr, "Instruction pointer address (hex)" },
  { "args", "COMMAND", property_args, "Command with arguments" },
  { "comm", "COMMAND", property_comm, "Command" },
  { "etime", "ELAPSED", property_etime, "Elapsed time" },
  { "flags", "F", property_flags, "Flags (octal)" },
  { "gid", "GID", property_egid, "Effective group ID (decimal)" },
  { "group", "GROUP", property_egroup, "Effective group ID (name)" },
  { "nice", "NI", property_nice, "Nice value" },
  { "pcpu", "%CPU", property_pcpu, "%age CPU used" },
  { "pgid", "PGID", property_pgid, "Process group ID" },
  { "pid", "PID", property_pid, "Process ID" },
  { "ppid", "PPID", property_ppid, "Parent process ID" },
  { "pri", "PRI", property_pri, "Priority" },
  { "rgid", "RGID", property_rgid, "Real group ID (decimal)" },
  { "rgroup", "RGROUP", property_rgroup, "Real group ID (name)" },
  { "rss", "RSS", property_rss, "Resident set size (1024 bytes)" },
  { "ruid", "RUID", property_ruid, "Real user ID (decimal)" },
  { "ruser", "RUSER", property_ruser, "Real user ID (name)" },
  { "state", "S", property_state, "Process state" },
  { "stime", "STIME", property_stime, "Start time" },
  { "time", "TIME", property_time, "Scheduled time" },
  { "tty", "TT", property_tty, "Terminal" },
  { "uid", "UID", property_euid, "Effective user ID (decimal)" },
  { "user", "USER", property_euser, "Effective user ID (name)" },
  { "vsz", "VSZ", property_vsize, "Virtual memory used (1024 bytes)" },
  { "wchan", "WCHAN", property_wchan, "Wait channel (hex)" },
};
#define NPROPERTIES (sizeof properties / sizeof *properties)

void format_add(const char *f) {
  char buffer[128], *heading;
  size_t i, n;
  int seen_equals;
  while(*f) {
    if(*f == ' ' || *f == ',') {
      ++f;
      continue;
    }
    i = 0;
    seen_equals = 0;
    /* property=... extends to the end of the argument, not until the
     * next comma; "The default header can be overridden by appending
     * an <equals-sign> and the new text of the header. The rest of
     * the characters in the argument shall be used as the header
     * text." */
    while(*f && (seen_equals || (*f != ' ' && *f != ','))) {
      if(*f == '=')
        seen_equals = 1;
      if(i < sizeof buffer - 1)
        buffer[i++] = *f;
      ++f;
    }
    buffer[i] = 0;
    if((heading = strchr(buffer, '=')))
      *heading++ = 0;
    for(n = 0; n < NPROPERTIES; ++n)
      if(!strcmp(properties[n].name, buffer))
        break;
    if(n >= NPROPERTIES)
      fatal(0, "unknown column format '%s'", buffer);
    if((ssize_t)(ncolumns + 1) < 0)
      fatal(0, "too many columns");
    columns = xrecalloc(columns, ncolumns + 1, sizeof *columns);
    columns[ncolumns].id = n;
    columns[ncolumns].heading = xstrdup(heading ? heading : properties[n].heading);
    ++ncolumns;
  }
}

void format_clear(void) {
  size_t n;
  for(n = 0; n < ncolumns; ++n)
    free(columns[n].heading);
  ncolumns = 0;
  free(columns);
  columns = NULL;
}

void format_columns(const pid_t *pids, size_t npids) {
  size_t n = 0, c;
  /* "The field widths shall be selected by the system to be at least
   * as wide as the header text (default or overridden value). If the
   * header text is null, such as -o user=, the field width shall be
   * at least as wide as the default header text." */
  for(c = 0; c < ncolumns; ++c)
    columns[c].width = strlen(*columns[c].heading
                              ? columns[c].heading
                              : properties[columns[c].id].heading);
  // We actually make columns wide enough for everything that may be
  // put in them.
  for(n = 0; n < npids; ++n) {
    for(c = 0; c < ncolumns; ++c) {
      // Render the value to a false buffer to find out how big it is
      struct buffer b[1];
      b->base = 0;
      b->pos = 0;
      b->size = 0;
      properties[columns[c].id].property(b, pids[n]);
      if(b->pos > columns[c].width)
        columns[c].width = b->pos;
    }
  }
}

void format_heading(char *buffer, size_t bufsize) {
  size_t c;
  for(c = 0; c < ncolumns && !*columns[c].heading; ++c)
    ;
  if(c < ncolumns)
    format_process(-1, buffer, bufsize);
  else
    *buffer = 0;
}

void format_process(pid_t pid, char *buffer, size_t bufsize) {
  size_t w, left, id, c, start;
  struct buffer b[1];
  b->base = buffer;
  b->pos = 0;
  b->size = bufsize;
  for(c = 0; c < ncolumns; ++c) {
    id = columns[c].id;
    start = b->pos;
    if(pid == -1)
      buffer_append(b, columns[c].heading);
    else
      properties[id].property(b, pid);
    /* Figure out how much we wrote */
    w = b->pos - start;
    /* For non-final columns, pad to the column width and one more for
     * the column separator */
    if(c + 1 < ncolumns) {
      left = 1 + columns[c].width - w;
      while(left-- > 0)
        buffer_putc(b, ' ');
    }
  }
  buffer_terminate(b);
}

void format_help(void) {
  size_t n;
  printf("  Property  Heading  Description\n");
  for(n = 0; n < NPROPERTIES; ++n)
    printf("  %-8s  %-7s  %s\n",
           properties[n].name,
           properties[n].heading,
           properties[n].description);
}
