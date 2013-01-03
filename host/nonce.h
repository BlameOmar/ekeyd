/* daemon/nonce.h
 *
 * nonce generation for entropy key.
 *
 * Copyright 2009 Simtec Electronics
 *
 * For licence terms refer to the COPYING file.
 */

#ifndef DAEMON_NONCE_H
#define DAEMON_NONCE_H

/** Fill a buffer with (pseudo)random data for a nonce.
 *
 * @param buff The buffer to fill.
 * @param count The length of \a buff.
 * @return true if the buffer has been successfully filled, false and errno set
 *         if there was an error.
 */
extern bool fill_nonce(uint8_t *buff, size_t count);


/** Free any resources associated with nonce generation.
 */
extern void close_nonce(void);

#endif /* DAEMON_NONCE_H */
