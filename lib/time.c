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
#include <sys/time.h>
#include <ctype.h>

double clock_to_time(unsigned long long ticks) {
  static double boot_time;

  if(!boot_time) {
    FILE *fp;
    double ssb;
    if(!(fp = fopen("/proc/uptime", "r")))
      fatal(errno, "opening /proc/uptime");
    if(fscanf(fp, "%lg", &ssb) != 1)
      fatal(errno, "reading /proc/uptime");
    fclose(fp);
    boot_time = clock_now() - ssb;
  }
  return boot_time + clock_to_seconds(ticks);
}

double clock_to_seconds(unsigned long long ticks) {
  return (double)ticks / sysconf(_SC_CLK_TCK);
}

double clock_now(void) {
  struct timeval tv;
  if(gettimeofday(&tv, NULL) < 0)
    fatal(errno, "gettimeofday");
  return tv.tv_sec + tv.tv_usec / 1000000.0;
}

size_t strfelapsed(const char *format, intmax_t seconds, char buffer[], size_t bufsize) {
  int ch;
  size_t n = 0, i;
  unsigned fill, width, digits, skip0, follower, sign;
  intmax_t value;
  uintmax_t uvalue;
  char formatted[32];

#define PUT(CH) do {                            \
  const int pch = (CH);                         \
  if(n < bufsize - 1)                           \
    buffer[n] = pch;                            \
  ++n;                                          \
} while(0)

  while(*format) {
    ch = *format++;
    if(ch == '%') {
      fill = ' ';
      width = 0;
      skip0 = 0;
      follower = 0;
      digits = 1;
      if(*format == '0')
        fill = '0';
      while(isdigit((unsigned char)*format))
        width = 10 * width + *format++ - '0';
      if(*format == '.') {
        ++format;
        while(isdigit((unsigned char)*format))
          digits = 10 * digits + *format++ - '0';
        if(digits > sizeof formatted)
          digits = sizeof formatted; /* bit of a hack... */
      }
      if(*format == '?') {
        skip0 = 1;
        ++format;
      }
      if(*format == '+') {
        if(*++format)
          follower = *format++;
      }
      if(!*format)
        break;
      switch(*format++) {
      case '%':
        PUT('%');
        value = 0;
        skip0 = 1;
        break;
      case 'd':
        value = seconds / 86400;
        break;
      case 'h':
        value = seconds / 3600;
        break;
      case 'H':
        value = (seconds % 86400) / 3600;
        break;
      case 'm':
        value = seconds / 60;
      case 'M':
        value = (seconds % 3600) / 60;
        break;
      case 'S':
        value = seconds % 60;
        break;
      case 's':
        value = seconds;
        break;
      default:
        value = 0;
        skip0 = 1;
        break;
      }
      if(!value && skip0)
        continue;
      if(value < 0) {
        uvalue = -value;
        sign = '-';
      } else {
        uvalue = value;
        sign = 0;
      }
      /* Convert to decimal generating at least the minimum number of digits */
      i = 0;
      while(i < digits || uvalue) {
        ++i;
        formatted[sizeof formatted - i] = uvalue % 10 + '0';
        uvalue /= 10;
      }
      /* Start with the sign */
      if(sign) {
        PUT('-');
        if(width > 0)
          --width;
      }
      /* Add padding until there's only enough space for the formatted value */
      while(width-- > i)
        PUT(fill);
      while(i > 0)
        PUT(formatted[sizeof formatted - i--]);
      if(follower)
        PUT(follower);
    } else
      PUT(ch);
  }
  PUT(0);
  return n - 1;
}
