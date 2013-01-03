/* frames/pem.h
 *
 * PEM64 encoding for framing.
 *
 * Copyright 2009 Simtec Electronics
 *
 * For licence terms refer to the COPYING file.
 */

#ifndef EKEY_FRAMES_PEM_H
#define EKEY_FRAMES_PEM_H

/*
 * PEM64 encoding takes bytes and encodes them into characters.
 * Each (up-to) three byte input produces four characters of output.
 * It is up to the caller to ensure buffers are the right sizes.
 */

/**
 * Encode some bytes in PEM64.
 *
 * @param inbytes The buffer to encode.
 * @param nbytes The number of bytes to encode.
 * @param outtext The buffer to fill.
 * @return The number of characters inserted into the output buffer.
 */
extern int pem64_encode_bytes(const unsigned char *inbytes,
                              int nbytes,
                              char *outtext);

/**
 * Calculate resulting length of encoding bytes in PEM64.
 *
 * @param nbytes The number of bytes to encode.
 * @return The number of characters required to encode the output bytes.
 */
extern int pem64_encode_bytes_length(int nbytes);

/**
 * Decode some text from PEM64.
 *
 * @param intext The buffer text to decode.
 * @param nchars The number of characters in the buffer.
 * @param outbytes The buffer to fill.
 * @return The number of bytes written into outbytes.
 */
extern int pem64_decode_bytes(const char *intext,
                              int nchars,
                              unsigned char *outbytes);

/** Determine how many chars will be needed to encode N bytes */
#define PEM64_CHARS_NEEDED(nbytes) ((((nbytes) + 2) / 3) * 4)

/** Determine how many bytes (may) be needed to decode N chars */
#define PEM64_BYTES_NEEDED(nchars) (((nchars) / 4) * 3)

extern void pem64_encode_12bits(const short value, char *outtext);
extern short pem64_decode_12bits(const char *intext);

#endif /* EKEY_FRAMES_PEM_H */
