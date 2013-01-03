/* daemon/foldback.h
 *
 * Foldback output stream
 *
 * Copyright 2009 Simtec Electronics
 *
 * For licence terms refer to the COPYING file.
 */

#ifndef DAEMON_FOLDBACK_H
#define DAEMON_FOLDBACK_H

/** Create a stream which sends data to the lua state.
 *
 * @return stream object or NULL on error.
 */
estream_state_t *estream_foldback_open(void);

#endif
