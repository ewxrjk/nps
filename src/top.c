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
#include "sysinfo.h"
#include "utils.h"
#include <getopt.h>
#include <stdio.h>
#include <curses.h>
#include <locale.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <math.h>

enum {
  OPT_HELP = 256,
  OPT_HELP_FORMAT,
  OPT_HELP_SYSINFO,
  OPT_VERSION,
};

const struct option options[] = {
  { "help", no_argument, 0, OPT_HELP },
  { "help-format", no_argument, 0, OPT_HELP_FORMAT },
  { "help-sysinfo", no_argument, 0, OPT_HELP_SYSINFO },
  { "version", no_argument, 0, OPT_VERSION },
  { 0, 0, 0, 0 },
};

static void loop(void);
static void await(void);
static void process_command(int ch);

static double update_interval = 1.0;
static double update_last;
static int quit;
static void (*process_key)(int) = process_command;

int main(int argc, char **argv) {
  int n;
  int set_format = 0;
  int show_idle = 1;
  char *e;
  int megabytes = 0;
  int set_sysinfo = 0;

  /* Set locale */
  if(!setlocale(LC_ALL, ""))
    fatal(errno, "setlocale");
  /* Set the default ordering */
  format_ordering("+pcpu,+io,+rss,+vsz");
  /* Parse command line */
  while((n = getopt_long(argc, argv, "+o:s:id:M", 
                         options, NULL)) >= 0) {
    switch(n) {
    case 'o':
      format_add(optarg);
      set_format = 1;
      break;
    case 's':
      format_ordering(optarg);
      break;
    case 'i':
      show_idle = 0;
      break;
    case 'I':
      sysinfo_format(optarg);
      break;
    case 'd':
      errno = 0;
      update_interval = strtod(optarg, &e);
      if(errno)
        fatal(errno, "invalid update interval '%s'", optarg);
      if(e == optarg || *e
         || isnan(update_interval) || isinf(update_interval)
         || update_interval <= 0)
        fatal(0, "invalid update interval '%s'", optarg);
      break;
    case 'M':
      megabytes = 1;
      break;
    case OPT_HELP:
      printf("Usage:\n"
             "  top [OPTIONS]\n"
             "Options:\n"
             "  -o FMT,FMT,...    Set output format\n"
             "  -s [+/-]FMT,...   Set ordering\n"
             "  -i                Hide idle processes\n"
             "  -I PROP,PROP,...  Set system information format\n"
             "  -d SECONDS        Set update interval\n"
             "  -M                Display memory sizes in megabytes\n"
             "  --help            Display option summary\n"
             "  --help-format     Display formatting & ordering help (-o/-s)\n"
             "  --help-sysinfo    Display system information help (-I)\n"
             "  --version         Display version string\n");
      return 0;
    case OPT_HELP_FORMAT:
      printf("The following properties can be used with the -o and -s option:\n"
             "\n");
      format_help();
      printf("\n"
             "Multiple properties can be specified in one -o option, separated by\n"
             "commas or spaces. Multiple -o options accumulate rather than overriding\n"
             "one another.\n"
             "\n"
             "Use property=heading to override the heading (but only for the last\n"
             "property in each argument).\n"
             "\n"
             "Multiple properties can also be specified with -s.  Later properties are\n"
             "used to order processes that match in earlier properties.  To reverse the\n"
             "sense of an ordering, prefix it with '-'.\n");
      return 0;
    case OPT_HELP_SYSINFO:
      printf("The following properties can be used with the -I option:\n"
             "\n");
      sysinfo_help();
      printf("\n"
             "Multiple properties can be specified in one -I option, separated by\n"
             "commas or spaces.\n");
      return 0;
    case OPT_VERSION:
      printf("%s\n", PACKAGE_VERSION);
      return 0;
    default:
      exit(1);

    }
  }
  /* Set the system info to display */
  if(!set_sysinfo) {
    if(megabytes)
      sysinfo_format("time,uptime,processes,load,cpu,memM,swapM");
    else
      sysinfo_format("time,uptime,processes,load,cpu,mem,swap");
  }
  /* Set the default selection */
  if(show_idle)
    select_default(select_all, NULL, 0);
  else
    select_default(select_nonidle, NULL, 0);
  /* Set the default format */
  if(!set_format) {
    format_add("user,pid,nice");
    if(megabytes)
      format_add("rssM");
    else
      format_add("rss");
    format_add("pcpu=%C");
    if(!getuid())
      format_add("read,write");
    format_add("tty=TTY");
    format_add("args=CMD");
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
  curs_set(0);                  /* Hide cursor */
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
  return format_compare(pi, a, b);
}

static void loop(void) {
  struct procinfo *last = NULL;
  char buffer[1024];
  int x, y, maxx, maxy;
  size_t n, npids, ninfos, len;
  pid_t *pids;

  while(!quit) {
    update_last = clock_now();
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
    ninfos = sysinfo_reset();
    for(n = 0; n < ninfos; ++n) {
      if(!sysinfo_get(pi, n, buffer, sizeof buffer)) {
        len = strlen(buffer);
        if(x && x + len > (size_t)maxx) {
          ++y;
          x = 0;
        }
        if(y >= maxy)
          break;
        if(mvaddnstr(y, x, buffer, maxx - x) == ERR)
          fatal(0, "mvaddstr %d,%d failed", y, x);
        x += strlen(buffer) + 2;
      }
    }

    if(x) {
      ++y;
      x = 0;
    }

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
      // curses seems to have trouble with the last position on the screen
      if(mvaddnstr(y, 0, buffer, y == maxy - 1 ? maxx - 1 : maxx) == ERR)
        fatal(0, "mvaddstr %d failed", y);
      ++y;
    }

    /* Display what we've got */    
    if(refresh() == ERR)
      fatal(0, "refresh failed");

    proc_free(last);
    last = pi;
    await();
  }
}

static void await(void) {
  double update_next, now, delta;
  struct timeval tv;
  fd_set fdin;
  int n, ch;

  while(!quit
        && (now = clock_now()) < (update_next = update_last + update_interval)) {
    delta = update_next - now;
    if(delta > 5.0)
      delta = 5.0;
    tv.tv_sec = floor(delta);
    tv.tv_usec = 1000000 * (delta - tv.tv_sec);
    FD_ZERO(&fdin);
    FD_SET(0, &fdin);
    n = select(1, &fdin, NULL, NULL, &tv);
    if(n < 0) {
      if(errno == EINTR || errno == EAGAIN)
        continue;
      fatal(errno, "select");
    }
    if(FD_ISSET(0, &fdin)) {
      ch = getch();
      if(ch != ERR)
        process_key(ch);
    }
  }
}

static void process_command(int ch) {
  switch(ch) {
  case 'q':
  case 'Q':
    quit = 1;
    break;
  case 12:
    if(redrawwin(stdscr) == ERR)
      fatal(0, "redrawwin failed");
    if(refresh() == ERR)
      fatal(0, "refresh failed");
    break;
  }
}
