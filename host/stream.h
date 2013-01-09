/* daemon/stream.h
 *
 * Abstract streaming interface for ekey protocol.
 *
 * Copyright 2009 Simtec Electronics
 *
 * For licence terms refer to the COPYING file.
 */

#ifndef DAEMON_STREAM_H
#define DAEMON_STREAM_H

#include <unistd.h>

typedef ssize_t (estream_read_fn)(int fd, void *buf, size_t count);
typedef ssize_t (estream_write_fn)(int fd, const void *buf, size_t count);

typedef struct {
    char *uri;

    /* stream info */
    estream_read_fn *estream_read; /** Stream read function. */
    estream_write_fn *estream_write; /** Stream write function. */
    int fd; /** file descriptor passed to functions */

    /* statistics */
    uint64_t bytes_read; /** number of bytes read from the stream */
    uint64_t bytes_written; /** number of bytes written to the stream */
} estream_state_t;

/** Open a stream.
 *
 * @param uri File to open.
 * @return Initialised stream object or NULL on error.
 */
extern estream_state_t *estream_open(const char *uri);

/** Read from a stream.
 *
 * @param state Stream state.
 * @param buf Buffer to read into
 * @param count The length of \a buf.
 * @return The number of bytes placed in \a buf or -1 and errno set.
 */
extern ssize_t estream_read(estream_state_t *state, void *buf, size_t count);

/** Write to a stream.
 *
 * @param state Stream state.
 * @param buf Buffer to write from.
 * @param count The length of \a buf.
 * @return The number of bytes written from \a buf or -1 and errno set.
 */
extern ssize_t estream_write(estream_state_t *state, void *buf, size_t count);

/** Close a stream.
 *
 * Closes a stream and frees any assciated resources.
 *
 * @param state Stream state to close.
 */
extern int estream_close(estream_state_t *state);

#endif /* DAEMON_STREAM_H */
