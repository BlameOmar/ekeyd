/* daemon/ekeyd.h
 *
 * Interface to Entropy Keys
 *
 * Copyright 2009 Simtec Electronics
 *
 * For licence terms refer to the COPYING file.
 */

#ifndef DAEMON_EKEYD_H
#define DAEMON_EKEYD_H

#include <stdbool.h>

/** An opaque handle to an entropy key. */
typedef struct econ_state_s OpaqueEkey;

#define EKEY_STATUS_UNKNOWN		0
#define EKEY_STATUS_GOODSERIAL		1
#define EKEY_STATUS_UNKNOWNSERIAL	2
#define EKEY_STATUS_BADKEY		3
#define EKEY_STATUS_GONEBAD		4
#define EKEY_STATUS_KEYED		5
#define EKEY_STATUS_KEYCLOSED		6

/**
 * Add an ekey to the set of keys from which we read entropy.
 *
 * @param devpath The path to the device node for this key.
 * @param serial The serial number to lock this node to, or NULL if
 *               it is acceptable to find any key on the device.
 * @return The newly created ekey structure representing this key.
 * @note This routine will only return NULL in out-of-memory situations.
 *       If the device node is not found, the structure is still created,
 *       and placed into the "Node not found" error state.
 */
extern OpaqueEkey *add_ekey(const char *devpath, const char *serial);

/**
 * Free an ekey structure, removing it from poll interfaces, etc.
 *
 * @param ekey The ekey structure to remove. After calling this, the
 *             caller should forget all pointers to this ekey structure
 *             as the memory will be freed.
 */
extern void kill_ekey(OpaqueEkey *ekey);

/**
 * Retrieve the status of an ekey structure.
 *
 * @param ekey The ekey structure to interrogate.
 * @return < 0 if an OS error has prevented the ekey from
 *         being used (e.g. -ENOENT).
 *         Otherwise one of the EKEY_STATUS_* defines.
 */
extern int query_ekey_status(OpaqueEkey *ekey);

/**
 * Retrieve the serial number associated with an ekey.
 *
 * @param ekey The ekey structure to interrogate.
 * @return a PEM64 encoded serial number string or NULL if the ekey has not got
 *         a serial number.
 * @note The returned string is allocated on teh heap and teh caller must free
 *         the storage.
 */
extern char *retrieve_ekey_serial(OpaqueEkey *ekey);

/**
 * Open an output stream for writing entropy to a file.
 *
 * @param fname The file name to open (must pre-exist).
 * @return true on success, false on failure, with errno set.
 */
extern bool open_file_output(const char *fname);

/**
 * Open an output stream for writing entropy to the kernel.
 *
 * @param bits_per_byte The number of bits per byte to claim entropy on.
 * @return true on success, false on failure, with errno set.
 */
extern bool open_kernel_output(int bits_per_byte);

/**
 * Open the "foldback" output stream which writes the entropy back into
 * the config state for Lua to handle.
 *
 * @return true on success, false on failure, with errno set.
 */
extern bool open_foldback_output(void);

#endif /* DAEMON_EKEYD_H */
