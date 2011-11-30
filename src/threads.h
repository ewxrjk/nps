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
#ifndef THREADS_H
#define THREADS_H

/** @brief Thread mode */
static int thread_mode;

/** @brief Mapping of thread mode to selection flags */
static unsigned thread_mode_flags[] = {
  PROC_PROCESSES,
  PROC_THREADS,
  PROC_PROCESSES|PROC_THREADS
};

#define THREAD_MODES (sizeof thread_mode_flags / sizeof *thread_mode_flags)

#endif /* THREADS_H */
