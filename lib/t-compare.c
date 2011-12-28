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
#include "compare.h"
#include <assert.h>

int main() {
  assert(qlcompare("", "") == 0);
  assert(qlcompare("a", "") == 1);
  assert(qlcompare("0", "") == 1);
  assert(qlcompare("", "A") == -1);
  assert(qlcompare("", "99") == -1);
  assert(qlcompare("a", "b") == -1);
  assert(qlcompare("B", "A") == 1);
  assert(qlcompare("0", "1") == -1);
  assert(qlcompare("1", "0") == 1);
  assert(qlcompare("2", "10") == -1);
  assert(qlcompare("10", "2") == 1);
  assert(qlcompare("foo2", "foo10") == -1);
  assert(qlcompare("99a", "99b") == -1);
  return 0;
}
