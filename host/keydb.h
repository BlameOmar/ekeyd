/* daemon/keydb.h
 *
 * Keyring input/management
 *
 * Copyright 2009 Simtec Electronics
 *
 * For licence terms refer to the COPYING file.
 */

#ifndef DAEMON_KEYDB_H
#define DAEMON_KEYDB_H

#include <stdint.h>

/**
 * (re-)Initialise the key database and read a keyring into it.
 *
 * @param fname The file to read the keyring from.
 * @return -1 with errno set on failure, otherwise returns the number of keys read.
 * @note Format is "PEMSerial PEMLTK\n" * nkeys.
 */
extern int read_keyring(const char *fname);

/**
 * Write the current keyring to a file. 
 * 
 * @param fname The filename to write to.
 * @return The number of entries written or -1 and errno set.
 */
int write_keyring(const char *fname);

/**
 * Retrieve a long-term-key by serial number.
 *
 * @param snum The serial number of the LTK to retrieve.
 * @return A pointer to the LTK, or NULL if not found.
 * @note The returned pointer is owned by the caller. It is
 *       the caller's responsibility to free it when it is
 *       finished with it.
 */
extern uint8_t *snum_to_ltkey(const uint8_t *snum);

/** 
 * Add a long term session key to the keyring.
 *
 * @param snum Serial number of the key.
 * @param ltkey The long term session key.
 */
int add_ltkey(const uint8_t *snum, const uint8_t *ltkey);

/**
 * Format a serial number and long term session key for output to a stream.
 *
 * @param snum Serial number of the key.
 * @param ltkey The long term session key.
 */
int output_key(FILE *fh, const uint8_t *snum, const uint8_t *ltkey);

#endif /* DAEMON_KEYDB_H */
