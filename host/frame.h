/* daemon/frame.h
 *
 * Framing interface for ekey protocol.
 *
 * Copyright 2009 Simtec Electronics
 *
 * For licence terms refer to the COPYING file.
 */

#ifndef DAEMON_FRAME_H
#define DAEMON_FRAME_H

#define EFRAME_LEN 64

#define SOF0 '*'
#define SOF1 ' '
#define EOF0 13
#define EOF1 10

/** Entropy key packet framer context. */
typedef struct {
    estream_state_t *stream; /**< Stream to read input from. */

    /* current frame info */
    uint8_t frame[EFRAME_LEN]; /**< Frame data. */
    int used; /**< Amount of data currently used in frame. */

    /* statistics */
    uint64_t byte_last; /**< Index of begining of last correct frame */
    uint32_t framing_errors; /**< Number of framing errors */
    uint32_t frames_ok; /**< Number of valid frames. */
} eframe_state_t;

/** Create a new framing context.
 *
 * @param stream_state The stream on which to put the framer.
 * @return The new frame state.
 */
extern eframe_state_t *eframe_open(estream_state_t *stream_state);

/** Close framing context.
 *
 * @param state The frame state to finalise.
 * @return 0
 */
extern int eframe_close(eframe_state_t *state);

/** Read framed data.
 *
 * @param state The frame state to read from.
 */
extern ssize_t eframe_read(eframe_state_t *state);

#endif /* DAEMON_FRAME_H */
