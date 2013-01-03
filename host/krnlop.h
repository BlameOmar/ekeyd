/* daemon/krnlop.h
 *
 * Kernel /dev/random output stream
 *
 * Copyright 2009 Simtec Electronics
 *
 * For licence terms refer to the COPYING file.
 */

#ifndef DAEMON_KRNLOP_H
#define DAEMON_KRNLOP_H

/** Open the systems kernel pool to accept entropy.
 *
 * @param path The path to the device node to open.
 * @param bpb The number of shannons per byte to claim during insertion.
 * @return The stream handle or NULL and errno set.
 */
estream_state_t *estream_krnl_open(const char *path, int bpb);

#endif
