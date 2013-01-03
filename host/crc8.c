/* daemon/crc8.c
 *
 * CCITT CRC-8 implementation.
 *
 * Copyright 2009 Simtec Electronics
 *
 * For licence terms refer to the COPYING file.
 */

#include <stdlib.h>
#include <stdint.h>

#include "crc8.h"

/* documented in crc8.h */
uint8_t
crc8(const uint8_t *dp, ssize_t size)
{
    uint8_t crc = 0xff;
    int b;

    while (size-- > 0) {
	crc ^= *dp;
	dp++;

	for (b = 0; b < 8; b++) {
	    if (crc & 0x80)
		crc = (crc << 1) ^ 0x31;
	    else
		crc <<= 1;
	}
    }

    return crc;
}
