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
#ifndef FORMAT_H
#define FORMAT_H

/** @file format.h
 * @brief Formatting process information
 */

#include <sys/types.h>

struct procinfo;

/** @brief Add to the format list
 * @param f Format string
 *
 * The format list is cumulative.
 */
void format_add(const char *f);

/** @brief Clear the format list
 */
void format_clear(void);

/** @brief Set internal column sizes
 * @param pi Pointer to process information
 * @param pids Process IDs
 * @param npids Number of processes
 */
void format_columns(struct procinfo *pi, const pid_t *pids, size_t npids);

/** @brief Construct the heading
 * @param pi Pointer to process information
 * @param buffer Where to put heading string
 * @param bufsize Size of buffer
 *
 * format_columns() must have been called.
 */
void format_heading(struct procinfo *pi, char *buffer, size_t bufsize);

/** @brief Construct the output for one process
 * @param pi Pointer to process information
 * @param pid Process ID
 * @param buffer Where to put header string
 * @param bufsize Size of buffer
 *
 * format_columns() must have been called.
 */
void format_process(struct procinfo *pi, pid_t pid,
                    char *buffer, size_t bufsize);

/** @brief Display formatting help */
void format_help(void);

#endif
