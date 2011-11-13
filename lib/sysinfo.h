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
#ifndef SYSINFO_H
#define SYSINFO_H

/** @file sysinfo.h
 * @brief System information
 */

#include <stddef.h>

struct procinfo;

/** @brief Set system information format
 * @param format Format string
 */
void sysinfo_format(const char *format);

/** @brief Reset cached system information 
 * @return The number of sysinfo elements
 */
size_t sysinfo_reset(void);

/** @brief Get a system information element
 * @param pi Pointer to process information
 * @param n Elemet number
 * @param buffer Where to put header string
 * @param bufsize Size of buffer
 * @return 0 on success, -1 if there are no more elements
 */
int sysinfo_get(struct procinfo *pi, size_t n, char buffer[], size_t bufsize);

/** @brief Display system information help */
void sysinfo_help(void);

#endif
