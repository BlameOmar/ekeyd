/* daemon/fds.h
 *
 * Interface to poll and fd handling
 *
 * Copyright 2009 Simtec Electronics
 *
 * For licence terms refer to the COPYING file.
 */

#ifndef DAEMON_FDS_H
#define DAEMON_FDS_H

typedef void (* ekeyfd_pollfunc_t)(int fd, short events, void *pw);

/** Add a filedescriptor to the poll set.
 *
 * @param fd The file descriptor to add.
 * @param events The events to call the poll function for.
 * @param func The private function to call when the fd events occour.
 * @param pw The private data to pass to the poll function.
 * @return The number of file descriptors allocated.
 */
int ekeyfd_add(int fd, short events, ekeyfd_pollfunc_t func, void *pw);

/** Remove a file descriptor from the poll set.
 *
 * @param fd The file descriptor to remove.
 */
int ekeyfd_rm(int fd);

/** Poll all file descriptors for activity.
 *
 * @param timeout How long to wait for activity.
 * @return The number of file descriptors with processed events.
 */
int ekeyfd_poll(int timeout);

/** Set events associated with a file descriptor.
 */
void ekeyfd_set_events(int fd, short events);

/** Clear events associated with a file descriptor.
 */
void ekeyfd_clear_events(int fd, short events);

#endif
