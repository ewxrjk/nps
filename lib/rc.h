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
#include <config.h>

#ifndef RC_H
#define RC_H

/** @file rc.h
 * @brief RC file support
 */

/** @brief ps -f format */
extern char *rc_ps_f_format;

/** @brief Default ps format */
extern char *rc_ps_format;

/** @brief ps -l format */
extern char *rc_ps_l_format;

/** @brief Configured value for top update interval */
extern char *rc_top_delay;

/** @brief Default top format */
extern char *rc_top_format;

/** @brief Default top ordering */
extern char *rc_top_order;

/** @brief Default top system info */
extern char *rc_top_sysinfo;

/** @brief Read the RC file, if one exists */
void read_rc(void);

/** @brief (Re-)write the RC file with current settings */
void write_rc(void);

#endif /* RC_H */
