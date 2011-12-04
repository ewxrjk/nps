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
#include "utils.h"
#include <stdlib.h>
#include <assert.h>

struct selector {
  select_function *sfn;
  union arg *args;
  size_t nargs;
};

static size_t nselectors;
static struct selector *selectors;

void select_clear(void) {
  free(selectors);
  selectors = NULL;
  nselectors = 0;
}

void select_add(select_function *sfn, union arg *args, size_t nargs) {
  if(!(nselectors + 1))
    fatal(0, "too many selectors");
  selectors = xrecalloc(selectors, nselectors + 1,  sizeof *selectors);
  selectors[nselectors].sfn = sfn;
  selectors[nselectors].args = args;
  selectors[nselectors].nargs = nargs;
  ++nselectors;
}

static const char *get_operator(const char *ptr,
                                int *operator) {
  int c = *ptr++;
  switch(c) {
  case '~':
    *operator = c; 
    return ptr;
  case '<':
    if(*ptr == '=') {
      *operator = LE;
      return ptr + 1;
    }
    if(*ptr == '>') {
      *operator = NE;
      return ptr + 1;
    }
    *operator = c;
    return ptr;
  case '>':
    if(*ptr == '=') {
      *operator = GE;
      return ptr + 1;
    }
    *operator = c;
    return ptr;
  case '=':
    if(*ptr == '=') {
      *operator = c;
      return ptr + 1;
    } else
      *operator = IDENTICAL;
    return ptr;
  case '!':
    if(*ptr == '=') {
      *operator = NE;
      return ptr + 1;
    }
    break;
  }
  return 0;
}

void select_match(const char *expr) {
  const char *ptr;
  union arg *args;
  int rc, operator;
  char buffer[128];

  for(ptr = expr; 
      *ptr && *ptr != '=' && *ptr != '~' 
        && *ptr != '<' && *ptr != '>' && *ptr != '!';
      ++ptr)
    ;
  if(!*ptr)
    fatal(0, "invalid match expression '%s'", expr);
  args = xmalloc(3 * sizeof *args);
  args[0].string = xstrndup(expr, ptr - expr);
  ptr = get_operator(ptr, &operator);
  if(!ptr)
    fatal(0, "%s: unrecognized match operator\n", expr);
  if(*ptr == ':')
     ++ptr;
  switch(operator) {
  case '~':
    rc = regcomp(&args[1].regex, ptr, REG_ICASE|REG_NOSUB);
    if(rc) {
      regerror(rc, &args[1].regex, buffer, sizeof buffer);
      fatal(0, "regexec: %s", buffer);
    }
    select_add(select_regex_match, args, 2);
    break;
  case IDENTICAL:
    args[1].string = xstrdup(ptr);
    select_add(select_string_match, args, 2);
    break;
  default:
    args[1].operator = operator;
    args[2].string = xstrdup(ptr);
    select_add(select_compare, args, 3);
    break;
  }
}

int select_test(struct procinfo *pi, taskident task) {
  size_t n;
  assert(nselectors > 0);
  for(n = 0; n < nselectors; ++n)
    if(selectors[n].sfn(pi, task, selectors[n].args, selectors[n].nargs))
      return 1;
  return 0;
}

void select_default(select_function *sfn, union arg *args, size_t nargs) {
  if(!nselectors)
    select_add(sfn, args, nargs);
}
