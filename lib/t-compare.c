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
  assert(qlcompare("16K", "16383") == 1);
  assert(qlcompare("16K", "16384") == 0);
  assert(qlcompare("16K", "16385") == -1);
  assert(qlcompare("16M", "16383K") == 1);
  assert(qlcompare("16M", "16384K") == 0);
  assert(qlcompare("16M", "16385K") == -1);
  assert(qlcompare("16G", "16383M") == 1);
  assert(qlcompare("16G", "16384M") == 0);
  assert(qlcompare("16G", "16385M") == -1);
  assert(qlcompare("16T", "16383G") == 1);
  assert(qlcompare("16T", "16384G") == 0);
  assert(qlcompare("16T", "16385G") == -1);
  assert(qlcompare("16P", "16383T") == 1);
  assert(qlcompare("16P", "16384T") == 0);
  assert(qlcompare("16P", "16385T") == -1);
  assert(qlcompare("16383T", "16P") == -1);
  assert(qlcompare("16384T", "16P") == 0);
  assert(qlcompare("16385T", "16P") == 1);
  return 0;
}
