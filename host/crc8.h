/* daemon/ekeyd.h
 *
 * Interface to Entropy Keys
 *
 * Copyright 2009 Simtec Electronics
 *
 * For licence terms refer to the COPYING file.
 */

#ifndef DAEMON_CRC8_H
#define DAEMON_CRC8_H

#include <unistd.h>

uint8_t crc8(const uint8_t *dp, ssize_t size);

#endif
