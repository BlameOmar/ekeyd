/* daemon/daemonise.h
 *
 * Damonisation support code
 *
 * Copyright 2009 Simtec Electronics
 *
 * For licence terms refer to the COPYING file.
 */

#ifndef DAEMON_DAEMONISE_H
#define DAEMON_DAEMONISE_H

/** Perform the operations required to sucessfully become a UNIX daemon.
 *
 * @param pidfilename The name of the pid file to write the child pid into.
 * @param close_all_fd Whether the daemonise process should close all fd
 */
extern void do_daemonise(const char *pidfilename, bool close_all_fd);

#endif
