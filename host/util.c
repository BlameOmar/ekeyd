/* daemon/util.c
 *
 * Entropy key daemon utility functions
 *
 * Copyright 2009 Simtec Electronics
 *
 * For licence terms refer to the COPYING file.
 */

#include <stdint.h>

#include "util.h"

static char hexa[16] = "0123456789ABCDEF";

/* exported interface, documented in util.h */
char *
phex(uint8_t *c, int l)
{
    static char text[128];
    int loop;
    for (loop = 0 ; loop < l ; loop++) {
        text[(loop * 2) + 1] = hexa[(c[loop] & 0xF)];
        text[loop * 2] = hexa[(c[loop] & 0xF0) >> 4];
    }
    text[loop*2] = 0;
    return text;
}
