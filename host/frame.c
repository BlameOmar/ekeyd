/* daemon/frame.c
 *
 * Entropy key daemon framing.
 *
 * Copyright 2009 Simtec Electronics
 *
 * For licence terms refer to the COPYING file.
 */

#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "stream.h"
#include "frame.h"

/* exported interface documented in frame.h */
eframe_state_t *
eframe_open(estream_state_t *stream_state)
{
    eframe_state_t *frame_state;

    if (stream_state == NULL)
        return NULL;

    frame_state = calloc(1, sizeof(eframe_state_t));
    if (frame_state == NULL)
        return NULL;

    frame_state->stream = stream_state;

    return frame_state;
}

/* exported interface documented in frame.h */
int
eframe_close(eframe_state_t *state)
{
    if (state != NULL) {
        free(state);
    }
    return 0;
}

/* Read data from input stream finding frames.
 *
 * a frame is of the format:
 * 0) a star
 * 1) a space (character 32 or 0x20)
 * 2) a packet type - one of ISAEW
 * 3) a flag byte indicating type of payload, > for text and ! for binary
 * 4 - 53) payload
 * 54 - 61) MAC
 * 62) CR
 * 63) LF
 *
 * Start Of Frame (SOF) is "* "
 * End Of Frame (EOF) is CRLF
 *
 * read data into buffer
 * if we have at least enough for a packet search for "* " SOF
 * if we found an SOF look for EOF
 * if EOF is before expected position we have a runt (short) frame, discard and
 *   start again
 * if EOF is not found before expected end of frame position, discard SOF and
 *   start again
 * If valid frame found pass up to next stage
 */
ssize_t
eframe_read(eframe_state_t *state)
{
    int rd;
    int avail;
    uint8_t *sof;

    if (state->used == EFRAME_LEN) {
        errno = EINVAL; /* invalid argument buffer is full */
        return -1;
    }

    /* attempt to fill buffer */
    avail = EFRAME_LEN - state->used;
    if (avail > 0) {
        rd = estream_read(state->stream, state->frame + state->used, avail);
        if (rd <= 0) {
            return rd; /* propogate the error */
        }
        state->used += rd;
    }

    if (state->used != EFRAME_LEN)
        goto ewouldblock;

    /* search for SOF */
    sof = memchr(state->frame, SOF0, EFRAME_LEN);
    if (sof == NULL) {
        /* no asterisk so no SOF in frame, clear frame, return error */
        state->used = 0;
        goto ewouldblock;
    }

    if (sof != state->frame) {
        /* possible SOF is not at beginning of frame */
        state->used = EFRAME_LEN - (sof - state->frame);
        memmove(state->frame, sof, state->used);
        goto ewouldblock;
    }

    /* first char of SOF is at buffer start, check for second */
    if (sof[1] != SOF1) {
        /* was not SOF1, find next SOF0 if available */
        goto skipsof0;
    }

    /* at this point we have a valid start of frame, check for EOF */
    if ((state->frame[EFRAME_LEN - 2] != EOF0) ||
        (state->frame[EFRAME_LEN - 1] != EOF1)) {
        /* invalid EOF */
        goto skipsof0;
    }

    /* update statistics */
    state->frames_ok++;
    state->byte_last = state->stream->bytes_read - EFRAME_LEN;

    //printf("Frame: %.60s\n", state->frame + 2);

    return EFRAME_LEN; /* valid frame */

skipsof0:
    sof = memchr(state->frame + 1, SOF0, EFRAME_LEN - 1);

    if (sof == NULL) {
        /* no asterisk so no SOF in frame, clear frame, return error */
        state->used = 0;
    } else {
        /* possible SOF is not at beginning of frame */
        state->used = EFRAME_LEN - (sof - state->frame);
        memmove(state->frame, sof, state->used);
    }
    state->framing_errors++;

ewouldblock:
    errno = EWOULDBLOCK;
    return -1;

}
