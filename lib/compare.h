/*
 * This file is part of nps.
 * Copyright (C) 2011, 13 Richard Kettlewell
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
#ifndef COMPARE_H
#define COMPARE_H

/** @file compare.h
 * @brief Comparison utilities
 */

/** @brief Most recent task enumeration */
extern struct taskinfo *global_taskinfo;

/** @brief Shim for use with qsort() */
int compare_task(const void *av, const void *bv);

/** @brief Multimodal comparison operator
 * @param a String to compare
 * @param b String to compare
 * @return -1, 0 or 1
 *
 * Numeric parts of the strings are compared as numbers; strings parts
 * as strings.
 */
int qlcompare(const char *a, const char *b);

#endif /* COMPARE_H */
