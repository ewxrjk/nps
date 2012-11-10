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
#ifndef USER_H
#define USER_H

#include <sys/types.h>

/** @brief Override user lookup file */
extern const char *forceusers;

/** @brief Override group lookup file */
extern const char *forcegroups;

/** @brief Look up a user
 * @param uid User ID
 * @return User information or NULL
 */
const char *lookup_user_by_id(uid_t uid);

/** @brief Look up a user
 * @param name User name
 * @return User ID
 */
uid_t lookup_user_by_name(const char *name);

/** @brief Look up a user
 * @param uid User ID
 * @return User information or NULL
 */
const char *lookup_group_by_id(gid_t gid);

/** @brief Look up a group
 * @param name Group name
 * @return Group ID
 */
uid_t lookup_group_by_name(const char *name);

#endif /* USER_H */

