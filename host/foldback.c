/* daemon/foldback.c
 *
 * Entropy key foldback output
 *
 * Copyright 2009 Simtec Electronics
 *
 * For licence terms refer to the COPYING file.
 */

#include <stdint.h>
#include <sys/types.h>
#include <stdlib.h>

#include "stream.h"
#include "foldback.h"
#include "lstate.h"

static ssize_t
foldback_write(int fd, const void *buf, size_t count)
{
    return lstate_foldback_entropy(buf, count);
}

/* exported interface, documented in foldback.h */
estream_state_t *
estream_foldback_open(void)
{
    estream_state_t *stream_state;

    stream_state = calloc(1, sizeof(estream_state_t));
    if (stream_state == NULL)
        return NULL;

    stream_state->estream_read = NULL;
    stream_state->estream_write = foldback_write;

    return stream_state;
}
