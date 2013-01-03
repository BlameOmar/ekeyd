/* frames/frame.h
 *
 * Framing information for eKey protocol
 *
 * Copyright 2009 Simtec Electronics
 *
 * For licence terms refer to the COPYING file.
 */

#ifndef EKEY_FRAMES_FRAME_H
#define EKEY_FRAMES_FRAME_H

/*
 * The following holds true for all frames
 * Frames start with a SOF of '* '
 * Frames end with an EOF of '\r\n'
 * Frames have a frame type indicator which is two characters.
 * Frames have a PEM64 encoded 48 bit MAC (eight characters).
 * Frame payloads are always 48 bytes long.
 * Frames are always 64 bytes from start of SOF to end of EOF.
 * Thus, we have 2 + 2 + N + 8 + 2 => 64, so the payload
 * is always 50 characters.
 *
 */

/**
 * Prepare the framer's MAC skein
 *
 * @param sharedkey The shared secret (32 bytes)
 */
extern void framer_prepare_mac(const unsigned char *sharedkey);

/**
 * Compute a MAC for a frame.
 *
 * @param frame The frame to compute the MAC for. Note, The SOF and
 *              EOF are not considered to be part of the frame.
 * @param mactarget The location to write the computed pem64'd MAC to.
 * @note frame must point at the frame type (i.e. not the SOF) as the
 *       MAC is computed on the frame type and payload only.
 */
extern void framer_compute_mac(const char *frame, char *mactarget);

/**
 * Frame a payload into a frame packet.
 *
 * @param frame The frame to fill out.
 * @param ft1 The first character of the frame type,
 * @param ft2 The second character of the frame type,
 * @param payload The payload (must be 50 bytes).
 *
 * @note If payload is < 50 bytes but NULL terminated, then it will be
 *       padded with ' '.
 * @note The caller is expected to fill out the SOF and EOF which will
 *       not be read nor written by this routine, but which are expected
 *       to be present by other routines.
 * @note \a frame should point at the SOF (or location there-for), this
 *       routine will then fill out from frame[2] with \a ft1...
 */
extern void framer_fill_frame(char *frame, char ft1, char ft2, char *payload);

#endif /* EKEY_FRAMES_FRAME_H */
