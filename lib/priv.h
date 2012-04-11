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
#ifndef PRIV_H
#define PRIV_H
/** @file priv.h
 * @brief Privileged operation support
 *
 * nps was not initially designed with setuid operation in mind.
 *
 * However, the kernel restricts access to certain files in /proc
 * making certain process properties only available to root or the
 * process's own user.  For a system administrator it shouldn't be a
 * big deal to become root to see these properties.  However for
 * "mortal" users the only way to make them available is to run ps/top
 * at higher privilege, and this may cause some installations to want
 * them to be setuid to root.
 *
 * Therefore it is appropriate to take certain measures to ensure that
 * these programs will not give away the keys to the kingdom if the
 * setuid bit is applied.
 *
 * Some of these measures merely enforce "reasonable assumptions",
 * and guard against the risk that these assumptions are implicit
 * in some part of the code.  They are:
 * - the process's argument vector is at least one element long
 * - the usual stdio file descriptors (0, 1 and 2) are open
 *
 * Other measures are necessary for secure operation and ensure that
 * the effective UID of the process is unprivileged except when it
 * needs to be.
 * - the effective UID is set to the real uid on startup
 * - privilege is only elevated again when reading privileged files
 * - privilege is only elevated via the priv_run() function
 * - read_rc() and write_rc() insist that geteuid()=getuid()
 */

#include <sys/types.h>

/** @brief Set up privileged operation
 * @param argc Argument count
 * @param argv Argument list
 *
 * This function should be called from main() before anything else.
 */
void priv_init(int argc, char **argv);

/** @brief Run a function with elevated privilege
 * @param op Function to execute
 * @param u Context pointer
 * @return Value returned from @p op
 *
 * This function is used to ensure that all returns from a privileged
 * function relinquish privilege.
 */
int priv_run(int (*op)(void *u), void *u);

/** @brief Return true if this process is privileged
 * @return Nonzero if this process is privileged
 *
 * A nonzero return means that the process can acquire elevated
 * privilege; not necessarily that it has it at the moment.
 */
int privileged(void);

/** @brief Real UID at startup */
extern uid_t priv_ruid;

/** @brief Effective UID at startup */
extern uid_t priv_euid;

#endif /* PRIV_H */
