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
#include "input.h"
#include "rc.h"
#include "compare.h"
#include "priv.h"
#include "buffer.h"
#include <getopt.h>
#include <stdio.h>
#include <curses.h>
#include <locale.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <signal.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>

#include "threads.h"

enum {
  OPT_HELP = 256,
  OPT_HELP_FORMAT,
  OPT_HELP_SYSINFO,
  OPT_VERSION,
};

const struct option options[] = {
  { "format", required_argument, 0, 'O' },
  { "sort", required_argument, 0, 's' },
  { "idle", no_argument, 0, 'i' },
  { "sysinfo", required_argument, 0, 'j' },
  { "delay", required_argument, 0, 'd' },
  { "threads", no_argument, 0, 'L' },
  { "help", no_argument, 0, OPT_HELP },
  { "help-format", no_argument, 0, OPT_HELP_FORMAT },
  { "help-sysinfo", no_argument, 0, OPT_HELP_SYSINFO },
  { "version", no_argument, 0, OPT_VERSION },
  { 0, 0, 0, 0 },
};

/** @brief What loop() and await() should do next */
enum next_action {
  /** @brief Re-enumerate processes */
  NEXT_RESAMPLE = 1,
  
  /** @brief Re-run column formatting */
  NEXT_REFORMAT = 2,
  
  /** @brief Re-select processes */
  NEXT_RESELECT = 4,
  
  /** @brief Re-sort processes */
  NEXT_RESORT = 8,

  /** @brief Redraw system information */
  NEXT_RESYSINFO = 16,

  /** @brief Redraw existing output
   *
   * This is used after changing @ref display_offset, and after @ref
   * NEXT_REFORMAT and @ref NEXT_RESORT have been processed.
   */
  NEXT_REDRAW = 32,

  /** @brief Continue to wait for input or timeout
   *
   * Only await() should see this; it should never return it to
   * loop(). */
  NEXT_WAIT = 64,

  /** @brief Terminate the program */
  NEXT_QUIT = 128
};

struct help_page {
  const char *const *lines;
  size_t nlines;
};

static void sighandler(int sig);
static void loop(void);
static enum next_action await(void);
static enum next_action process_command(int ch);
static void collect_input(const char *prompt,
                          int (*validator)(const char *),
                          enum next_action (*setter)(const char *),
                          char **(helper)(void));
static enum next_action process_input_key(int ch);
static int valid_delay(const char *s);
static enum next_action set_delay(const char *s);
static int valid_format(const char *s);
static enum next_action set_format(const char *s);
static int valid_order(const char *s);
static enum next_action set_order(const char *s);
static int valid_sysinfo(const char *s);
static enum next_action set_sysinfo(const char *s);
static void display_help(const struct help_page *page);

/** @brief Time between updates in seconds */
static double update_interval = 1.0;

/** @brief Time of last update */
static double update_last;

/** @brief Whether to show idle processes */
static int show_idle = 1;

/** @brief Keypress handler 
 *
 * Usually this is process_command() but it switches to
 * process_input_key() when something is being edited.
 */
static enum next_action (*process_key)(int) = process_command;

/** @brief Horizontal display offset */
static size_t display_offset;

/** @brief Pipe for signal communication */
static int sigpipe[2];

/** @brief Handled signals */
static sigset_t sighandled;

/** @brief Currently displayed help page
 *
 * There is "always" a help page being displayed - but page 0 is 0
 * lines long.
 */
static size_t help_page;

/** @brief Scroll offset within help page */
static size_t help_offset;

/** @brief Line editor context */
static struct input_context input;

/** @brief Line editor input buffer */
static char input_buffer[1024];

/** @brief Called when editing completes with a valid string */
static enum next_action (*input_set)(const char *);

/** @brief Help page for current input */
static struct help_page input_help;

/** @brief Number of lines reserved for help
 *
 * Note that when editing something, one of the help lines is lost.
 */
#define HELP_SIZE 8

static const const char *const command_help[] = {
  "Keyboard commands:",
  "^L  Redisplay                j  Edit system info",
  " d  Edit update interval     o  Edit column list",
  " h  Help (repeat for more)   s  Edit sort order",
  " i  Toggle idle processes    t  Toggle thread display",
  "                             q  Quit",
};

static const const char *const panning_help[] = {
  "Panning:",
  "  ^F, right arrow    Move viewport right by 1",
  "  page down          Move viewport right by 8",
  "  ^B, left arrow     Move viewport left by 1",
  "  page up            Move viewport left by 8",
  "  ^A                 Move viewport to left margin",
  "  h                  Dismiss help"
};

/** @brief Table of help pages */
static const struct help_page help_pages[] = {
  { NULL, 0 },
  { command_help, sizeof command_help / sizeof *command_help },
  { panning_help, sizeof panning_help / sizeof *panning_help }
};

#define NHELPPAGES (sizeof help_pages / sizeof *help_pages)

int main(int argc, char **argv) {
  int n;
  int have_set_format = 0;
  char *e;
  int have_set_sysinfo = 0;
  char **help;
  struct sigaction sa;

  /* Initialize privilege support (this must stay first) */
  priv_init(argc, argv);
  /* Set locale */
  if(!setlocale(LC_ALL, ""))
    fatal(errno, "setlocale");
  read_rc();
  /* Set the default ordering */
  if(rc_top_order)
    format_ordering(rc_top_order, 0);
  else
    format_ordering("+pcpu,+io,+rss,+pmem", 0);
  if(rc_top_delay) {
    double v;
    errno = 0;
    v = strtod(rc_top_delay, &e);
    if(!errno && e != rc_top_delay && !*e && !isnan(v) && !isinf(v) && v > 0)
      update_interval = v;
  }
  /* Parse command line */
  while((n = getopt_long(argc, argv, "+o:s:ij:d:O:L",
                         options, NULL)) >= 0) {
    switch(n) {
    case 'o':
      format_set(optarg, FORMAT_ARGUMENT|FORMAT_ADD);
      have_set_format = 1;
      break;
    case 'O':
      format_set(optarg, FORMAT_QUOTED|FORMAT_ADD);
      have_set_format = 1;
      break;
    case 's':
      format_ordering(optarg, 0);
      break;
    case 'L':
      thread_mode = (thread_mode + 1) % THREAD_MODES;
      break;
    case 'i':
      show_idle = 0;
      break;
    case 'j':
      sysinfo_set(optarg, 0);
      have_set_sysinfo = 1;
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
    case OPT_HELP:
      printf("Usage:\n"
             "  top [OPTIONS]\n"
             "Options:\n"
             "  -d, --delay SECONDS        Set update interval\n"
             "  -i, --idle                 Hide idle processes\n"
             "  -L, --threads              Display threads\n"
             "  -j, --sysinfo SYSPROPS...  Set system information format; see --help-sysinfo\n"
             "  -o, -O, --format PROPS...  Set output format; see --help-format\n"
             "  -s, --sort [+/-]PROPS...   Set ordering; see --help-format\n"
             "  --help                     Display option summary\n"
             "  --version                  Display version string\n"
             "Press 'h' for on-screen help.\n");
      return 0;
    case OPT_HELP_FORMAT:
      printf("The following properties can be used with the -O, -o and -s options:\n"
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
             "Multiple properties can also be specified with -s.  Later properties are\n"
             "used to order processes that match in earlier properties.  To reverse the\n"
             "sense of an ordering, prefix it with '-'.\n");
      return 0;
    case OPT_HELP_SYSINFO:
      printf("The following properties can be used with the -j option:\n"
             "\n");
      help = sysinfo_help();
      while(*help)
        puts(*help++);
      printf("\n"
             "Multiple properties can be specified in one -j option, separated by\n"
             "commas or spaces.\n"
             "\n"
             "Use property=heading to override the heading and property/argument to set\n"
             "an argument.  Headings and arguments must be quoted if they contain\n"
             "spaces or commas, and headings must be quoted if an argument follows.\n");
      return 0;
    case OPT_VERSION:
      printf("%s\n", PACKAGE_VERSION);
      return 0;
    default:
      exit(1);

    }
  }
  /* Set the system info to display */
  if(!have_set_sysinfo) {
    if(rc_top_sysinfo)
      sysinfo_set(rc_top_sysinfo, 0);
    else
      sysinfo_set("time,uptime,processes,load,cpu,mem,swap", 0);
  }
  /* Set the default selection */
  select_default(show_idle ? select_all : select_nonidle, NULL, 0);
  /* Set the default format */
  if(!have_set_format) {
    if(rc_top_format)
      format_set(rc_top_format, FORMAT_QUOTED);
    else {
      format_set("user,pid,nice,rss,pcpu=%C", FORMAT_QUOTED);
      if(!priv_euid)
        format_set("read,write", FORMAT_QUOTED|FORMAT_ADD);
      format_set("tty=TTY,argsbrief=CMD", FORMAT_QUOTED|FORMAT_ADD);
    }
  }
  /* Set up SIGWINCH detection */
  if(pipe(sigpipe) < 0)
    fatal(errno, "pipe");
  if((n = fcntl(sigpipe[1], F_GETFL)) < 0)
    fatal(errno, "fcntl");
  if(fcntl(sigpipe[1], F_SETFL, n | O_NONBLOCK) < 0)
    fatal(errno, "fcntl");
  sa.sa_handler = sighandler;
  sa.sa_flags = 0;
  sigemptyset(&sa.sa_mask);
  if(sigaction(SIGWINCH, &sa, NULL) < 0)
    fatal(errno, "sigaction");
  sigemptyset(&sighandled);
  sigaddset(&sighandled, SIGWINCH);
  if(sigprocmask(SIG_BLOCK, &sighandled, NULL) <0)
    fatal(errno, "sigprocmask");
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
  if(nodelay(stdscr, TRUE) == ERR)
    fatal(0, "nodelay failed");
  /* Loop until quit */
  loop();
  /* Deinitialize curses */
  onfatal = NULL;
  if(endwin() == ERR)
    fatal(0, "endwin failed");
  return 0;
}

// ----------------------------------------------------------------------------

static void sighandler(int sig) {
  unsigned char sigc = sig;
  int save_errno = errno;
  write(sigpipe[1], &sigc, 1);
  errno = save_errno;
}

// ----------------------------------------------------------------------------

/** @brief The main display loop */
static void loop(void) {
  struct procinfo *last = NULL;
  char *ptr, *newline;
  int x, y, maxx, maxy, ystart = 0, ylimit;
  size_t n, ntasks, len, offset;
  taskident *tasks = NULL;
  enum next_action next = NEXT_RESAMPLE;
  const struct help_page *help;
  struct buffer b[1];

  buffer_init(b);
  while(!(next & NEXT_QUIT)) {
    if(next & NEXT_RESAMPLE) {
      /* Get fresh data */
      proc_free(last);
      last = global_procinfo;
      update_last = clock_now();
      global_procinfo = proc_enumerate(last, PROC_PROCESSES|PROC_THREADS);
      sysinfo_reset();
      free(tasks);
      tasks = proc_get_selected(global_procinfo, &ntasks, 
                                thread_mode_flags[thread_mode]);
      next |= NEXT_RESYSINFO|NEXT_RESORT|NEXT_REFORMAT;
    }
    if(next & NEXT_RESELECT) {
      /* Reselect processes to display after selection has changed */
      free(tasks);
      proc_reselect(global_procinfo);
      tasks = proc_get_selected(global_procinfo, &ntasks,
                                thread_mode_flags[thread_mode]);
      next |= NEXT_RESORT|NEXT_REFORMAT;
    }
    if(next & NEXT_RESORT) {
      /* Put processes into order */
      qsort(tasks, ntasks, sizeof *tasks, compare_task);
      next |= NEXT_REDRAW;
    }
    if(next & NEXT_REFORMAT) {
      /* Work out column widths */
      format_columns(global_procinfo, tasks, ntasks);
      next |= NEXT_REDRAW;
    }
    if(next & NEXT_RESYSINFO) {
      /* Start at the top with a blank screen */
      if(erase() == ERR)
        fatal(0, "erase failed");
      x = y = 0;
      getmaxyx(stdscr, maxy, maxx);
      /* System information */
      for(n = 0; !sysinfo_format(global_procinfo, n, b); ++n) {
        ptr = b->base;
        while(*ptr) {
          if((newline = strchr(ptr, '\n')))
            *newline++ = 0;
          len = strlen(ptr);
          if(x && x + len > (size_t)maxx) {
            ++y;
            x = 0;
          }
          if(y >= maxy)
            break;
          if(mvaddnstr(y, x, ptr, maxx - x) == ERR)
            fatal(0, "mvaddstr %d,%d failed", y, x);
          x += strlen(ptr) + 2;
          if(newline) {
            ++y;
            x = 0;
            ptr = newline;
          } else
            break;
        }
      }
      if(x)
        ++y;
      ystart = y;
      next |= NEXT_REDRAW;
    }
    if(next & NEXT_REDRAW) {
      getmaxyx(stdscr, maxy, maxx);
      /* (Re-)draw the process list */
      y = ystart;
      help = input_help.nlines ? &input_help : &help_pages[help_page];
      ylimit = maxy - min(help->nlines, 8);
      move(ystart, 0);
      clrtobot();

      /* Heading */
      if(y < ylimit) {
        attron(A_REVERSE);
        format_heading(global_procinfo, b);
        offset = min(display_offset, b->pos);
        if(mvaddnstr(y, 0, b->base + offset, maxx) == ERR)
          fatal(0, "mvaddstr %d failed", y);
        ++y;
        for(x = strlen(b->base + offset); x < maxx; ++x)
          addch(' ');
        attroff(A_REVERSE);
      }

      /* Processes */
      for(n = 0; n < ntasks && y < ylimit; ++n) {
        format_process(global_procinfo, tasks[n], b);
        offset = min(display_offset, b->pos);
        // curses seems to have trouble with the last position on the screen
        if(mvaddnstr(y, 0, b->base + offset,
                     y == ylimit - 1 ? maxx - 1 : maxx) == ERR)
          fatal(0, "mvaddstr %d failed", y);
        ++y;
      }

      display_help(help);

      /* Input */
      if(input.bufsize) {
        input_draw(&input);
        curs_set(1);
      } else
        curs_set(0);
    }

    /* Display what we've got */    
    if(refresh() == ERR)
      fatal(0, "refresh failed");
    /* See what to do next */
    do {
      next = await();
    } while(next == NEXT_WAIT);
  }
  free(tasks);
  proc_free(global_procinfo);
  proc_free(last);
  free(b->base);
}

/** @brief Handle keyboard input and wait for the next update
 * @return What do to next
 */
static enum next_action await(void) {
  double update_next, started, finished, frac, delta;
  struct timeval tv;
  fd_set fdin;
  int n, ch, ch2, ret;
  unsigned char sig;
  struct winsize ws;

  started = clock_now();
  /* When is the next update? */
  update_next = update_last + update_interval;
  /* Figure out how long to wait until the next update is required */
  delta = update_next - started;
  /* Even if there is no time to wait we still call select() and check
   * for keyboard and signal IO */
  if(delta < 0)
    delta = 0;
  /* Only wait until the next second boundary, so we can keep the
   * clock in the system info up to date */
  frac = ceil(started) - started;
  if(!frac)
    frac = 1.0;
  if(delta > frac)
    delta = frac;
  tv.tv_sec = floor(delta);
  tv.tv_usec = 1000000 * (delta - tv.tv_sec);
  FD_ZERO(&fdin);
  FD_SET(0, &fdin);
  FD_SET(sigpipe[0], &fdin);
  if(sigprocmask(SIG_UNBLOCK, &sighandled, NULL) < 0)
    fatal(errno, "sigprocmask");
  n = select(max(0, sigpipe[0]) + 1, &fdin, NULL, NULL, &tv);
  if(sigprocmask(SIG_BLOCK, &sighandled, NULL) < 0)
    fatal(errno, "sigprocmask");
  if(n < 0) {
    if(errno == EINTR || errno == EAGAIN)
      return NEXT_WAIT;
    fatal(errno, "select");
  }
  /* Handle keyboard input */
  if(FD_ISSET(0, &fdin)) {
    while((ch = getch()) != ERR) {
      if(ch == 27) {
        ch2 = getch();
        if(ch2 != ERR)
          ch = ch2 + ESCBIT;
      }
      if((ret = process_key(ch)) != NEXT_WAIT)
        return ret;
    }
  }
  /* Handle signals */
  if(FD_ISSET(sigpipe[0], &fdin)) {
    n = read(sigpipe[0], &sig, 1);
    if(n > 0) {
      switch(sig) {
      case SIGWINCH:
        if(ioctl(0, TIOCGWINSZ, &ws) < 0)
          fatal(errno, "ioctl TIOCGWINSZ");
        resizeterm(ws.ws_row, ws.ws_col);
        return NEXT_RESYSINFO;
      default:
        fatal(0, "unexpected signal %d", sig);
      }
    } else if(n < 0)
      fatal(errno, "read from sigpipe");
    else if(n == 0)
        fatal(0, "EOF from sigpipe");
  }
  /* If the clock should change redraw the sytsem info (without
   * resampling it). */
  finished = clock_now();
  if(finished >= update_last+update_interval)
    return NEXT_RESAMPLE;
  if(floor(started) != floor(finished))
    return NEXT_RESYSINFO;
  return NEXT_WAIT;
}

// ----------------------------------------------------------------------------

/** @brief Handle keyboard input */
static enum next_action process_command(int ch) {
  char *f;
  switch(ch) {
  case 'q':
  case 'Q':
    return NEXT_QUIT;
  case 'd':
  case 'D':
    collect_input("Delay> ",
                  valid_delay,
                  set_delay,
                  NULL);
    snprintf(input.buffer, input.bufsize, "%g", update_interval);
    break;
  case 'o':
  case 'O':
    f = format_get();
    if(strlen(f) >= sizeof input_buffer) {
      beep();
      free(f);
      break;
    }
    collect_input("Format> ",
                  valid_format,
                  set_format,
                  format_help);
    strcpy(input_buffer, f);
    free(f);
    break;
  case 's':
  case 'S':
    f = format_get_ordering();
    if(strlen(f) >= sizeof input_buffer) {
      beep();
      free(f);
      break;
    }
    collect_input("Sort> ",
                  valid_order,
                  set_order,
                  format_help);
    strcpy(input_buffer, f);
    free(f);
    break;
  case 't':
  case 'T':
    thread_mode = (thread_mode + 1) % THREAD_MODES;
    return NEXT_RESELECT;
  case 'i':
  case 'I':
    show_idle = !show_idle;
    select_clear();
    select_default(show_idle ? select_all : select_nonidle, NULL, 0);
    return NEXT_RESELECT;
  case 'j':
  case 'J':
    f = sysinfo_get();
    if(strlen(f) >= sizeof input_buffer) {
      beep();
      free(f);
      break;
    }
    collect_input("System> ",
                  valid_sysinfo,
                  set_sysinfo,
                  sysinfo_help);
    strcpy(input_buffer, f);
    free(f);
    break;
  case '?':
  case 'h':
  case 'H':
    help_page = (help_page + 1) % NHELPPAGES;
    help_offset = 0;
    return NEXT_REDRAW;
  case 27:
    if(help_page) {
      help_page = 0;
      return NEXT_REDRAW;
    }
    break;
  case 12:                      /* ^L */
    if(redrawwin(stdscr) == ERR)
      fatal(0, "redrawwin failed");
    if(refresh() == ERR)
      fatal(0, "refresh failed");
    break;
  case 2:                       /* ^B */
  case KEY_LEFT:
    if(display_offset > 0) {
      --display_offset;
      return NEXT_REDRAW;
    }
    break;
  case KEY_PPAGE:
    if(display_offset >= 8)
      display_offset -= 8;
    else if(display_offset > 0)
      display_offset = 0;
    return NEXT_REDRAW;
  case 6:                       /* ^F */
  case KEY_RIGHT:
    ++display_offset;
    return NEXT_REDRAW;
  case KEY_NPAGE:
    display_offset += 8;
    return NEXT_REDRAW;
  case 1:                       /* ^A */
  case KEY_HOME:
    display_offset = 0;
    return 1;
  }
  /* If input.bufsize is nonzero then it must have only just got that
   * way, as otherwise we'd have called process_input_key() instead */
  if(input.bufsize) {
    input.len = strlen(input.buffer);
    input.cursor = input.len;
    help_page = 0;
    return NEXT_REDRAW;
  }
  return NEXT_WAIT;
}

// ----------------------------------------------------------------------------

/** @brief Edit some property
 * @param prompt Prompt to display
 * @param validator Callback to check whether input is valid
 * @param setter Callback to commit edited value
 *
 * The caller should fill in the input buffer after this function
 * returns.
 */
static void collect_input(const char *prompt,
                          int (*validator)(const char *),
                          enum next_action (*setter)(const char *),
                          char **(*helper)(void)) {
  size_t n;
  process_key = process_input_key;
  memset(&input, 0, sizeof input);
  input.buffer = input_buffer;
  input.bufsize = sizeof input_buffer;
  input.validate = validator;
  input.prompt = prompt;
  input_set = setter;
  if(helper) {
    input_help.lines = (const char *const *)helper();
    for(n = 0; input_help.lines[n]; ++n)
      ;
    input_help.nlines = n;
  }
}

/** @brief Handle keyboard input while editing */
static enum next_action process_input_key(int ch) {
  switch(ch) {
  case 24:                      /* ^X */
  case 27:                      /* ESC */
    process_key = process_command;
    input.bufsize = 0;
    if(input_help.lines) {
      free_strings((char **)input_help.lines);
      input_help.lines = 0;
      input_help.nlines = 0;
      help_offset = 0;
    }
    return NEXT_REDRAW;
  case 13:                      /* CR */
    if(input.validate(input.buffer)) {
      process_key = process_command;
      input.bufsize = 0;
      if(input_help.lines) {
        free_strings((char **)input_help.lines);
        input_help.lines = 0;
        input_help.nlines = 0;
        help_offset = 0;
      }
      return input_set(input.buffer);
    }
    break;
  case 16:                      /* ^P */
  case KEY_UP:
    if(input_help.lines && help_offset) {
      --help_offset;
      return NEXT_REDRAW;
    }
    break;
  case KEY_PPAGE:
    if(input_help.lines && help_offset) {
      if(help_offset > HELP_SIZE - 3)
        help_offset -= HELP_SIZE - 3;
      else
        help_offset = 0;
      return NEXT_REDRAW;
    }
    break;
  case 14:                      /* ^N */
  case KEY_DOWN:
    if(input_help.lines && help_offset < input_help.nlines - (HELP_SIZE-1)) {
      ++help_offset;
      return NEXT_REDRAW;
    }
    break;
  case KEY_NPAGE:
    if(input_help.lines && help_offset < input_help.nlines - (HELP_SIZE-1)) {
      if(help_offset + HELP_SIZE - 3 < input_help.nlines - (HELP_SIZE-2))
        help_offset += HELP_SIZE - 3;
      else
        help_offset = input_help.nlines - (HELP_SIZE-1);
      return NEXT_REDRAW;
    }
    break;
  default:
    input_key(ch, &input);
    break;
  }
  return NEXT_WAIT;
}

// ----------------------------------------------------------------------------

static int valid_delay(const char *s) {
  double value;
  char *e;
  errno = 0;
  value = strtod(s, &e);
  if(errno)
    return 0;
  if(e == s || *e
     || isnan(value) || isinf(value)
     || value <= 0)
    return 0;
  return 1;
}

static enum next_action set_delay(const char *s) {
  update_interval = strtod(s, NULL);
  /* Force an immediate update */
  return NEXT_RESAMPLE;
}

// ----------------------------------------------------------------------------

static int valid_format(const char *s) {
  return format_set(s, FORMAT_QUOTED|FORMAT_CHECK);
}

static enum next_action set_format(const char *s) {
  format_set(s, FORMAT_QUOTED);
  return NEXT_REFORMAT;
}

// ----------------------------------------------------------------------------

static int valid_order(const char *s) {
  return format_ordering(s, FORMAT_CHECK);
}

static enum next_action set_order(const char *s) {
  format_ordering(s, 0);
  return NEXT_RESORT;
}

// ----------------------------------------------------------------------------

static int valid_sysinfo(const char *s) {
  return sysinfo_set(s, FORMAT_CHECK);
}

static enum next_action set_sysinfo(const char *s) {
  sysinfo_set(s, 0);
  return NEXT_RESYSINFO;
}

// ----------------------------------------------------------------------------

/** @brief Display the current help page */
static void display_help(const struct help_page *page) {
  int y, maxx, maxy, x, ystart;
  size_t line, actual_line;
  size_t lines = min(page->nlines, HELP_SIZE);
  getmaxyx(stdscr, maxy, maxx);
  ystart = maxy - lines;
  move(ystart, 0);
  clrtobot();
  if(lines && input.bufsize)
    --lines;
  for(line = 0; line < lines; ++line) {
    if(line == 0) {
      attron(A_REVERSE);
      actual_line = 0;
    } else
      actual_line = line + help_offset;
    if(actual_line < page->nlines) {
      y = ystart + line;
      if(mvaddnstr(y, 0,
                   page->lines[actual_line],
                   y == maxy - 1 ? maxx - 1 : maxx) == ERR)
        fatal(0, "mvaddstr %d failed", y);
      if(line == 0)
        for(x = strlen(page->lines[0]); x < maxx; ++x)
          if(mvaddch(y, x, ' ') == ERR)
            fatal(0, "mvaddch %d failed", y);
    }
    if(line == 0)
      attroff(A_REVERSE);
  }
}
