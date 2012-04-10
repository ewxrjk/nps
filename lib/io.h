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
#ifndef IO_H
#define IO_H

/** @file io.h
 * @brief IO support
 */

#include <stdio.h>

/** @brief Error-checking printf wrapper
 * @param format Format string
 * @param ... Arguments
 * @return Number of characters written
 *
 * Calls fatal() on error.
 */
int xprintf(const char *format, ...) attribute((format (printf, 1, 2)));

/** @brief Flushing exit
 * @param rc Exit status
 *
 * If @p rc is not 0, closes @c stdout and exits nonzero if the flush
 * failed.
 */
void xexit(int rc) attribute((noreturn));

/** @brief Open a file
 * @param path Filename
 * @param mode Mode
 * @return Pointer to stream
 *
 * Calls fatal() on error.
 */
FILE *xfopen(const char *path, const char *mode);

/** @brief Close a file
 * @param Pointer to stream
 *
 * Calls fatal() on error.
 */
void xfclose(FILE *fp, const char *path);

#endif /* IO_H */

