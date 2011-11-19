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
  selectors = xrecalloc(selectors, nselectors + 1,  sizeof *selectors);
  selectors[nselectors].sfn = sfn;
  selectors[nselectors].args = args;
  selectors[nselectors].nargs = nargs;
  ++nselectors;
}

int select_test(struct procinfo *pi, pid_t pid) {
  size_t n;
  assert(nselectors > 0);
  for(n = 0; n < nselectors; ++n)
    if(selectors[n].sfn(pi, pid, selectors[n].args, selectors[n].nargs))
      return 1;
  return 0;
}

void select_default(select_function *sfn, union arg *args, size_t nargs) {
  if(!nselectors)
    select_add(sfn, args, nargs);
}
