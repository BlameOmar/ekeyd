/* daemon/util.h
 *
 * Utility functions for Ekey Daemon
 *
 * Copyright 2009 Simtec Electronics
 *
 * For licence terms refer to the COPYING file.
 */

#ifndef DAEMON_UTIL_H
#define DAEMON_UTIL_H

/** Format a number of bytes in a hexidecamal textural representation.
 *
 * @param c The bytes to format.
 * @param l The number of byyes to format.
 * @return A string containing the textural representation.
 * @note This is intended for debugging and the returned string is only valid
 * untill the next call to teh function.
 */
extern char *phex(uint8_t *c, int l);

#endif /* DAEMON_UTIL_H */
