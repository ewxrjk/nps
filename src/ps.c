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
#include "compare.h"
#include <getopt.h>
#include <stdio.h>
#include <errno.h>
#include <termios.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <limits.h>
#include <ctype.h>

enum {
  OPT_HELP = 256,
  OPT_HELP_FORMAT,
  OPT_HELP_MATCH,
  OPT_VERSION,
  OPT_PPID,
  OPT_SORT,
  OPT_GROUP,
};

const struct option options[] = {
  { "all", no_argument, 0, 'e' },
  { "command", required_argument, 0, 'C' },
  { "full", no_argument, 0, 'f' },
  { "long", no_argument, 0, 'l' },
  { "group", required_argument, 0, OPT_GROUP },
  { "real-group", required_argument, 0, 'G' },
  { "Group", required_argument, 0, 'G' },
  { "user", required_argument, 0, 'u' },
  { "real-user", required_argument, 0, 'U' },
  { "User", required_argument, 0, 'U' },
  { "forest", no_argument, 0, 'H' },
  { "format", required_argument, 0, 'O' },
  { "pid", required_argument, 0, 'p' },
  { "tty", required_argument, 0, 't' },
  { "ppid", required_argument, 0, OPT_PPID },
  { "sort", required_argument, 0, OPT_SORT },
  { "help", no_argument, 0, OPT_HELP },
  { "help-format", no_argument, 0, OPT_HELP_FORMAT },
  { "help-match", no_argument, 0, OPT_HELP_MATCH },
  { "version", no_argument, 0, OPT_VERSION },
  { 0, 0, 0, 0 },
};

int main(int argc, char **argv) {
  int n, width = 0, sorting = 0;
  union arg *args;
  size_t nargs, npids;
  pid_t *pids;
  size_t i;
  int set_format = 0;
  char buffer[1024];
  struct winsize ws;
  const char *s;
  char **help, *t;

  /* Read configuration */
  read_rc();
  /* Parse command line */
  while((n = getopt_long(argc, argv, "+aAdeflg:G:n:o:O:p:t:u:U:R:C:wH", 
                         options, NULL)) >= 0) {
    switch(n) {
    case 'a':
      select_add(select_has_terminal, NULL, 0);
      break;
    case 'A': case 'e':
      select_add(select_all, NULL, 0);
      break;
    case 'C':
      asprintf(&t, "comm=:%s", optarg);
      select_match(t);
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
        format_set("flags,state,uid,pid,ppid,pcpu=C,pri,nice,addr,vszK=SZ,wchan,tty=TTY,time,comm=CMD", FORMAT_QUOTED);
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
    case OPT_GROUP:
      args = split_arg(optarg, arg_group, &nargs);
      select_add(select_egid, args, nargs);
      break;
    case 'H':
      format_hierarchy = 1;
      format_ordering("-_hier", FORMAT_INTERNAL);
      sorting = 1;
      break;
    case 'n':
      /* ignored */
      break;
    case 'o':
      format_set(optarg, FORMAT_ARGUMENT|FORMAT_ADD);
      set_format = 1;
      break;
    case 'O':
      format_set(optarg, FORMAT_QUOTED|FORMAT_ADD);
      set_format = 1;
      break;
    case 'p':
      args = split_arg(optarg, arg_process, &nargs);
      select_add(select_pid, args, nargs);
      break;
    case OPT_PPID:
      args = split_arg(optarg, arg_process, &nargs);
      select_add(select_ppid, args, nargs);
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
    case 'w':
      width = INT_MAX;
      break;
    case OPT_SORT:
      format_ordering(optarg, 0);
      sorting = 1;
      break;
    case OPT_HELP:
      printf("Usage:\n"
             "  ps [OPTIONS] [MATCH|PIDS...]\n"
             "Options:\n"
             "  -a                      Select process with a terminal\n"
             "  -A, -e, --all           Select all processes\n"
             "  -C, --command NAME      Select by process name\n"
             "  -d                      Select non-session-leaders\n"
             "  -f, --full, -l, --long  Full/long output format\n"
             "  -g SIDS                 Select processes by session ID\n"
             "  -G GIDS, --group GIDS   Select processes by real/effective group ID\n"
             "  -H, --forest            Hierarchical display\n"
             "  -o, -O, --format PROPS  Set output format; see --help-format\n"
             "  -p, --pids PIDS         Select processes by process ID\n"
             "  --ppid PIDS             Select processes by parent process ID\n"
             "  --sort [+/-]PROPS...    Set ordering; see --help-format\n"
             "  -t, --tty TERMS         Select processes by terminal\n"
             "  -u, -U UIDS             Select processes by real/effective user ID\n"
             "  -w                      Don't truncate output\n"
             "  --help                  Display option summary\n"
             "  --version               Display version string\n"
             "See also --help-format, --help-match.\n");
      return 0;
    case OPT_HELP_FORMAT:
      printf("The following properties can be used with the -O, -o and --sort options:\n"
             "\n");
      help = format_help();
      while(*help)
        puts(*help++);
      printf("\n"
             "Multiple properties can be specified in one -o option, separated by\n"
             "commas or spaces. Multiple -o options accumulate rather than overriding\n"
             "one another.\n"
             "\n"
             "Use property=heading to override the heading and property/argument to set\n"
             "an argument.  With -o, headings extend to the end of the string.\n"
             "For headings with -O, and for arguments in general, they must be quoted\n"
             "if they contain a space or a comma.  Headings must be quoted if an\n"
             "argument follows.\n"
             "\n"
             "Multiple properties can also be specified with --sort.  Later properties are\n"
             "used to order processes that match in earlier properties.  To reverse the\n"
             "sense of an ordering, prefix it with '-'.\n");
      return 0;
    case OPT_HELP_MATCH:
      printf("The following match expressions can be used:\n"
             "\n"
             "  PROP=VALUE     Exact match against displayed string\n"
             "  PROP~REGEXP    POSIX extended regular expression match\n"
             "  PROP<VALUE     Less-than match against value\n"
             "  PROP<=VALUE    Less-than-or-equal match against value\n"
             "  PROP>VALUE     Greater-than match against value\n"
             "  PROP>=VALUE    Greater-than-or-equal match against value\n"
             "  PROP==VALUE    Equal value\n"
             "  PROP<>VALUE    Different value\n"
             "\n"
             "Put a ':' after the operator to avoid confusion with first character of VALUE.\n");
      return 0;             
    case OPT_VERSION:
      printf("%s\n", PACKAGE_VERSION);
      return 0;
    default:
      exit(1);
    }
  }
  while(optind < argc) {
    if(isdigit((unsigned char)argv[optind][0])) {
      args = split_arg(argv[optind++], arg_process, &nargs);
      select_add(select_pid, args, nargs);
    } else
      select_match(argv[optind++]);
  }
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
  global_procinfo = proc_enumerate(NULL);
  pids = proc_get_selected(global_procinfo, &npids);
  /* Put them into order */
  if(sorting)
    qsort(pids, npids, sizeof *pids, compare_pid);
  /* Set up output formatting */
  format_columns(global_procinfo, pids, npids);
  format_heading(global_procinfo, buffer, sizeof buffer);
  /* Figure out the display width */
  if(!width) {
    if((s = getenv("COLUMNS")) && (n = atoi(s)))
      width = n;
    else if(isatty(1)
            && ioctl(1, TIOCGWINSZ, &ws) >= 0 
            && ws.ws_col > 0)
      width = ws.ws_col;
    else
      width = INT_MAX;            /* don't truncate */
  }
  /* Generate the output */
  if(*buffer && printf("%.*s\n", width, buffer) < 0) 
    fatal(errno, "writing to stdout");
  for(i = 0; i < npids; ++i) {
    format_process(global_procinfo, pids[i], buffer, sizeof buffer);
    if(printf("%.*s\n", width, buffer) < 0) 
      fatal(errno, "writing to stdout");
  }
  if(fclose(stdout) < 0)
    fatal(errno, "writing to stdout");
  proc_free(global_procinfo);
  exit(0);
}
