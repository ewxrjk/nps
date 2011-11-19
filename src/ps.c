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
#include "selectors.h"
#include "format.h"
#include "process.h"
#include "utils.h"
#include "rc.h"
#include <getopt.h>
#include <stdio.h>
#include <errno.h>
#include <termios.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <limits.h>

enum {
  OPT_HELP = 256,
  OPT_HELP_FORMAT,
  OPT_VERSION,
};

const struct option options[] = {
  { "help", no_argument, 0, OPT_HELP },
  { "help-format", no_argument, 0, OPT_HELP_FORMAT },
  { "version", no_argument, 0, OPT_VERSION },
  { 0, 0, 0, 0 },
};

int main(int argc, char **argv) {
  int n, width = 80;
  union arg *args;
  size_t nargs, npids;
  pid_t *pids;
  size_t i;
  int set_format = 0;
  char buffer[1024];
  struct winsize ws;
  const char *s;
  struct procinfo *pi;
  char **help;

  /* Read configuration */
  read_rc();
  /* Parse command line */
  while((n = getopt_long(argc, argv, "+aAdeflg:G:n:o:p:t:u:U:", 
                         options, NULL)) >= 0) {
    switch(n) {
    case 'a':
      select_add(select_has_terminal, NULL, 0);
      break;
    case 'A': case 'e':
      select_add(select_all, NULL, 0);
      break;
    case 'd':
      select_add(select_not_session_leader, NULL, 0);
      break;
    case 'f':
      if(rc_ps_f_format)
        format_set(rc_ps_f_format, FORMAT_QUOTED);
      else
        format_set("user=UID,pid,ppid,pcpu=C,stime,tty=TTY,time,argsbrief=CMD", FORMAT_QUOTED);
      set_format = 1;
      break;
    case 'l':
      if(rc_ps_l_format)
        format_set(rc_ps_l_format, FORMAT_QUOTED);
      else
        format_set("flags,state,uid,pid,ppid,pcpu=C,pri,nice,addr,vsz=SZ,wchan,tty=TTY,time,comm=CMD", FORMAT_QUOTED);
      set_format = 1;
      break;
    case 'g':
      args = split_arg(optarg, arg_process, &nargs);
      select_add(select_leader, args, nargs);
      break;
    case 'G':
      args = split_arg(optarg, arg_group, &nargs);
      select_add(select_rgid, args, nargs);
      break;
    case 'n':
      /* ignored */
      break;
    case 'o':
      format_set(optarg, FORMAT_ARGUMENT|FORMAT_ADD);
      set_format = 1;
      break;
    case 'p':
      args = split_arg(optarg, arg_process, &nargs);
      select_add(select_pid, args, nargs);
      break;
    case 't':
      args = split_arg(optarg, arg_tty, &nargs);
      select_add(select_terminal, args, nargs);
      break;
    case 'u':
      args = split_arg(optarg, arg_user, &nargs);
      select_add(select_euid, args, nargs);
      break;
    case 'U':
      args = split_arg(optarg, arg_user, &nargs);
      select_add(select_ruid, args, nargs);
      break;
    case OPT_HELP:
      printf("Usage:\n"
             "  ps [OPTIONS]\n"
             "Options:\n"
             "  -a                Select process with a terminal\n"
             "  -A, -e            Select all processes\n"
             "  -d                Select non-session-leaders\n"
             "  -f                Full output\n"
             "  -l                Long output\n"
             "  -g SID,SID....    Select processes by session ID\n"
             "  -G GID,GID,...    Select processes by real group ID\n"
             "  -o FMT,FMT,...    Set output format\n"
             "  -p PID,PID,...    Select processes by process ID\n"
             "  -t TERM,TERM,...  Select processes by terminal\n"
             "  -u UID,UID,...    Select processes by real user ID\n"
             "  -U UID,UID,...    Select processes by effective user ID\n"
             "  --help            Display option summary\n"
             "  --help-format     Display formatting help\n"
             "  --version         Display version string\n");
      return 0;
    case OPT_HELP_FORMAT:
      printf("The following properties can be used with the -o option:\n"
             "\n");
      help = format_help();
      while(*help)
        puts(*help++);
      printf("\n"
             "Multiple properties can be specified in one -o option, separated by\n"
             "commas or spaces. Multiple -o options accumulate rather than overriding\n"
             "one another.\n"
             "\n"
             "Use property=heading to override the heading (but only for the last\n"
             "property in each argument).\n");
      return 0;
    case OPT_VERSION:
      printf("%s\n", PACKAGE_VERSION);
      return 0;
    default:
      exit(1);
    }
  }
  if(optind < argc)
    fatal(0, "excess arguments");
  /* Set the default format */
  if(!set_format) {
    if(rc_ps_format)
      format_set(rc_ps_format, FORMAT_QUOTED);
    else
      format_set("pid,tty=TTY,time,comm=CMD", FORMAT_QUOTED);
  }
  /* Set the default selection */
  select_default(select_uid_tty, NULL, 0);
  /* Get the list of processes */
  pi = proc_enumerate(NULL);
  pids = proc_get_selected(pi, &npids);
  /* Set up output formatting */
  format_columns(pi, pids, npids);
  format_heading(pi, buffer, sizeof buffer);
  /* Figure out the display width */
  if((s = getenv("COLUMNS")) && (n = atoi(s)))
    width = n;
  else if(isatty(1)
          && ioctl(1, TIOCGWINSZ, &ws) >= 0 
          && ws.ws_col > 0)
    width = ws.ws_col;
  else
    width = INT_MAX;            /* don't truncate */
  /* Generate the output */
  if(*buffer && printf("%.*s\n", width, buffer) < 0) 
    fatal(errno, "writing to stdout");
  for(i = 0; i < npids; ++i) {
    format_process(pi, pids[i], buffer, sizeof buffer);
    if(printf("%.*s\n", width, buffer) < 0) 
      fatal(errno, "writing to stdout");
  }
  if(fclose(stdout) < 0)
    fatal(errno, "writing to stdout");
  proc_free(pi);
  exit(0);
}
