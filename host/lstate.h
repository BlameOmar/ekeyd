/* daemon/lstate.h
 *
 * Entropy Key Daemon, Lua state for configuration and fiddling.
 *
 * Copyright 2009 Simtec Electronics
 *
 * For licence terms refer to the COPYING file.
 */

#ifndef DAEMON_LSTATE_H
#define DAEMON_LSTATE_H

#include <stdbool.h>

#include "ekeyd.h"

/**
 * Initialise the Lua state for configuration and fiddling.
 *
 * This loads the socket and posix extensions, prepares the
 * initial state and loads the basic control code into it
 * ready for running the configuration file.
 *
 * Errors reported to stderr.
 *
 * @return true on success, false on failure.
 */
extern bool lstate_init(void);

/**
 * Destroy the Lua configuration state.
 *
 * Remove the Lua configuration state from memory, closing down
 * all control sockets etc.
 */
extern void lstate_finalise(void);

/**
 * Run the configuration file through to completion.
 *
 * This loads, compiles and runs the configuration file specified in
 * \a conffile.
 *
 * Errors reported to stderr.
 *
 * @param conffile The configuration file path to read from.
 * @return true on success, false on failure.
 *
 * @note It is up to the caller to ensure the result is still sane afterwards.
 * @note Also, on a false return, the caller should call ::lstate_finalise.
 */
extern bool lstate_runconfig(const char *conffile);

/**
 * Pass in some bytes to a control interface.
 *
 * This takes additional bytes from any control interface and adds
 * them to the lua state. If the resultant accumulated input has a
 * newline in it then it will be removed, compiled and run.
 *
 * Errors sent to the appropriate control interface.
 *
 * @note This should be called each time a FD passed to
 *       ::lstate_cb_newfd goes readable.
 */
extern void lstate_controlbytes(void);

/**
 * Tell the configuration state about a change in availability of a key.
 *
 * This tells the configuration state about an ekey changing state.
 * Typically this will be if a key is found, goes bad, etc.
 *
 * @note The \a ekey parameter must be an ekey previously returned by
 *       the configuration state having called ::add_ekey.
 * @note This means you must not call this function until ::add_ekey has
 *       fully returned. Otherwise confusion will ensue.
 *
 * @param ekey The Entropy Key whose state has changed.
 */
extern void lstate_inform_about_key(OpaqueEkey *ekey);

/**
 * Ask whether or not the config required a lack of daemonisation (e.g. debug)
 *
 * @return true if we should daemonise.
 */
extern bool lstate_request_daemonise(void);

/**
 * Inform the state about entropy.
 *
 * This is used for the foldback output method to allow the state to
 * provide the output mechanism such as an EGD implementation.
 *
 * @param eblock Block of entropy bytes.
 * @param ecount Number of bytes in \a eblock.
 * @return The number of bytes take from \a eblock.
 *
 * @note The state must previously have requested foldback output.
 */
extern ssize_t lstate_foldback_entropy(const unsigned char *eblock, unsigned int ecount);

/************************************** Callbacks ***************************/

/**
 * Callback to ask the core to monitor a FD.
 *
 * This is called to ask the core daemon to monitor a FD on behalf of
 * the lua configuration state. If one of those FDs goes readable (or
 * error) then ::lstate_controlbytes should be called.
 *
 * @note If the core returns false, the configuration state will revert
 *       whatever just happened.
 *
 * @param fd The FD to monitor.
 * @return true if the FD is added to the core's poll, false if it couldn't be.
 */
extern bool lstate_cb_newfd(int fd);

/**
 * Callback to ask the core to stop monitoring a FD.
 *
 * This is called to ask the core daemon to stop monitoring a FD on
 * behalf of the lua configuration state. If this FD isn't in the poll
 * then the core is asked to silently swallow the issue.
 *
 * @param fd The FD to stop monitoring.
 */
extern void lstate_cb_delfd(int fd);

/**
 * Callback to ask the core to monitor a FD for writing.
 *
 * This is called when the lua configuration state needs the core to
 * also monitor a FD for writing.
 *
 * @note Only ever called on a FD already added with ::lstate_cb_newfd
 *
 * @param fd The FD to also monitor for writing.
 */
extern void lstate_cb_writefd(int fd);

/**
 * Callback to ask the core to stop monitoring a FD for writing.
 *
 * This is called when the lua configuration state no longer requires
 * the core to monitor a FD for writing. Instead the core should
 * revert to only monitoring that FD for read/error states.
 *
 * @note Only ever called on a FD already set for write monitoring.
 *
 * @param fd The FD to no longer monitor for writing.
 */
extern void lstate_cb_nowritefd(int fd);

#endif
