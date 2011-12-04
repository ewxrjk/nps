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
#ifndef GENERAL_H
#define GENERAL_H

/** @file general.h
 * @brief General definitions
 */

/** @brief One kilobyte */
#define KILOBYTE 1024

/** @brief One megabyte */
#define MEGABYTE (1024 * KILOBYTE)

/** @brief One gigabyte */
#define GIGABYTE (1024 * MEGABYTE)

/** @brief One terabyte */
#define TERABYTE ((uintmax_t)1024 * GIGABYTE)

/** @brief One petabyte */
#define PETABYTE ((uintmax_t)1024 * TERABYTE)

#endif /* GENERAL_H */
