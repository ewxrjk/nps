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
#include <signal.h>
#include <stdio.h>

const char *signame(int sig, char buffer[], size_t bufsize) {
  switch(sig) {
  case SIGHUP: return "HUP";
  case SIGINT: return "INT";
  case SIGQUIT: return "QUIT";
  case SIGILL: return "ILL";
  case SIGTRAP: return "TRAP";
  case SIGABRT: return "ABRT";
#if SIGIOT != SIGABRT
  case SIGIOT: return "IOT";
#endif
  case SIGBUS: return "BUS";
  case SIGFPE: return "FPE";
  case SIGKILL: return "KILL";
  case SIGUSR1: return "USR1";
  case SIGSEGV: return "SEGV";
  case SIGUSR2: return "USR2";
  case SIGPIPE: return "PIPE";
  case SIGALRM: return "ALRM";
  case SIGTERM: return "TERM";
  case SIGSTKFLT: return "STKFLT";
#if SIGCLD != SIGCHLD
  case SIGCLD: return "CLD";
#endif
  case SIGCHLD: return "CHLD";
  case SIGCONT: return "CONT";
  case SIGSTOP: return "STOP";
  case SIGTSTP: return "TSTP";
  case SIGTTIN: return "TTIN";
  case SIGTTOU: return "TTOU";
  case SIGURG: return "URG";
  case SIGXCPU: return "XCPU";
  case SIGXFSZ: return "XFSZ";
  case SIGVTALRM: return "VTALRM";
  case SIGPROF: return "PROF";
  case SIGWINCH: return "WINCH";
#if SIGPOLL != SIGIO
  case SIGPOLL: return "POLL";
#endif
  case SIGIO: return "IO";
  case SIGPWR: return "PWR";
  case SIGSYS: return "SYS";
  default:
    snprintf(buffer, bufsize, "%d", sig);
    return buffer;
  }
}
