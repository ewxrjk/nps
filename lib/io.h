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
#ifndef IO_H
#define IO_H

/** @file io.h
 * @brief IO support
 */

#include <stdio.h>
#include <dirent.h>

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

/** @brief Open a file
 * @param mode Mode
 * @param pathp Where to store path (or NULL)
 * @param format Filename format
 * @param ... Format arguments
 * @return Pointer to stream
 *
 * Calls fatal() on error.
 */
FILE *xfopenf(char **pathp, const char *mode, const char *format, ...)
  attribute((format (printf, 3, 4)));

/** @brief Open a file
 * @param mode Mode
 * @param pathp Where to store path (or NULL)
 * @param format Filename format
 * @param ... Format arguments
 * @return Pointer to stream
 *
 */
FILE *fopenf(char **pathp, const char *mode, const char *format, ...)
  attribute((format (printf, 3, 4)));

/** @brief Close a file
 * @param fp Pointer to stream
 * @param path Filename corresponding to stream
 *
 * Calls fatal() on error.
 */
void xfclose(FILE *fp, const char *path);

/** @brief Open a directory
 * @param pathp Where to store path (or NULL)
 * @param format Filename format
 * @param ... Format arguments
 * @return Pointer to directory stream
 */
DIR *opendirf(char **pathp, const char *format, ...)
  attribute((format (printf, 2, 3)));

/** @brief Read from a directory
 * @param dir Name of directory
 * @param dp Pointer to directory stream
 *
 * Calls fatal() on error.
 */
struct dirent *xreaddir(const char *dir, DIR *dp);

/** @brief Create a directory
 * @param path New directory
 * @param mode Permissions
 *
 * Calls fatal() on error.
 */
void xmkdir(const char *path, mode_t mode);

#endif /* IO_H */

