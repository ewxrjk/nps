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
#include <getopt.h>
#include <stdio.h>
#include <curses.h>
#include <locale.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>

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

static void loop(void);

int main(int argc, char **argv) {
  int n;
  int set_format = 0;

  /* Set locale */
  if(!setlocale(LC_ALL, ""))
    fatal(errno, "setlocale");
  /* Parse command line */
  while((n = getopt_long(argc, argv, "+o:", 
                         options, NULL)) >= 0) {
    switch(n) {
    case 'o':
      format_add(optarg);
      set_format = 1;
      break;
    case OPT_HELP:
      printf("Usage:\n"
             "  top [OPTIONS]\n"
             "Options:\n"
             "  -o FMT,FMT,...    Set output format\n"
             "  --help            Display option summary\n"
             "  --help-format     Display formatting help\n"
             "  --version         Display version string\n");
      return 0;
    case OPT_HELP_FORMAT:
      printf("The following properties can be used with the -o option:\n"
             "\n");
      format_help();
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
  /* Set the default selection */
  select_default(select_all, NULL, 0);
  /* Set the default format */
  if(!set_format) {
    format_add("user,pid,rss,vsz,pcpu,tty=TTY");
    format_add("time,args=CMD");
  }
  /* Initialize curses */
  if(!initscr())
    fatal(0, "initscr failed");
  onfatal = endwin;
  if(cbreak() == ERR)           /* Read keys as they are pressed */
    fatal(0, "cbreak failed");
  if(noecho() == ERR)           /* Suppress echoing of type keys */
    fatal(0, "noecho failed");
  if(nonl() == ERR)             /* Suppress newline translation */
    fatal(0, "nonl failed");
  if(intrflush(stdscr, FALSE) == ERR) /* Flush output on ^C */
    fatal(0, "initrflush failed");
  if(keypad(stdscr, TRUE) == ERR) /* Enable keypad support */
    fatal(0, "keypad failed");
  /* Loop until quit */
  loop();
  /* Deinitialize curses */
  onfatal = NULL;
  if(endwin() == ERR)
    fatal(0, "endwin failed");
  return 0;
}

static struct procinfo *pi;

static int compare_pid(const void *av, const void *bv) {
  pid_t a = *(const pid_t *)av;
  pid_t b = *(const pid_t *)bv;
  return format_compare(pi, "+pcpu", a, b);
}

static void loop(void) {
  struct procinfo *last = NULL;
  char buffer[1024];
  int x, y, maxx, maxy;
  size_t n, npids;
  pid_t *pids;

  for(;;) {
    pi = proc_enumerate(last);
    pids = proc_get_selected(pi, &npids);
    qsort(pids, npids, sizeof *pids, compare_pid);
    format_columns(pi, pids, npids);
    
    /* Start at the top with a blank screen */
    if(erase() == ERR)
      fatal(0, "erase failed");
    x = y = 0;
    getmaxyx(stdscr, maxy, maxx);

    /* System information */
    // TODO

    /* Heading */
    if(y < maxy) {
      attron(A_REVERSE);
      format_heading(pi, buffer, sizeof buffer);
      if(mvaddnstr(y, 0, buffer, maxx) == ERR)
        fatal(0, "mvaddstr %d failed", y);
      ++y;
      for(x = strlen(buffer); x < maxx; ++x)
        addch(' ');
      attroff(A_REVERSE);
    }

    /* Processes */
    for(n = 0; n < npids && y < maxy; ++n) {
      format_process(pi, pids[n], buffer, sizeof buffer);
      if(mvaddnstr(y, 0, buffer, maxx) == ERR)
        fatal(0, "mvaddstr %d failed", y);
      ++y;
    }

    /* Display what we've got */    
    if(refresh() == ERR)
      fatal(0, "refresh failed");

    proc_free(last);
    last = pi;
    sleep(1);                   /* TODO but wait for a keypresss... */
  }
}
