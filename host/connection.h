/* daemon/ekeyd.h
 *
 * Ekey conenction handler
 *
 * Copyright 2009 Simtec Electronics
 *
 * For licence terms refer to the COPYING file.
 */

#ifndef DAEMON_CONNECTION_H
#define DAEMON_CONNECTION_H

#include "frame.h"
#include "packet.h"

typedef struct econ_state_s econ_state_t;

typedef enum {
    ESTATE_INIT = 0, /** Initial state. */
    ESTATE_CLOSE, /** Connection should be closed. */
    ESTATE_UNTRUSTED, /** Conection is untrusted. */
    ESTATE_SESSION, /** Connection requires keying for session. */
    ESTATE_SESSION_SENT, /** keying packet sent, waiting for session. */
    ESTATE_KEYED_FIRST, /** State for first packet after session key issued */
    ESTATE_KEYED_BAD, /** The session key setting failed */
    ESTATE_KEYED, /** Connection is active and session key is ok. */
    ESTATE_SIZE
} ekey_state_t;

/** The state of a connected ekey. */
struct econ_state_s {
    uint8_t *ltkey; /**< Long term key. */

    estream_state_t *key_stream; /**< The input stream. */
    estream_state_t *op_stream; /**< The output stream. */
    eframe_state_t *eframer; /**< The framer attached to the stream. */
    epkt_state_t *epkt; /**< The packet handler attached to the framer. */

    ekey_state_t current_state; /**< The current state in the connections FSM. */
    uint8_t *snum; /**< Serial number */
    int snum_len; /**< Length of serial number in bytes. */

    EKeySkein session_state; /**< Current session keys skein state. */

    uint8_t *nonce; /**< The session key nonce. */
    int nonce_len; /**< The length of the session nonce. */
    uint32_t keyreq_counter; /**< The number of times we've ignored a keyreq packet */
  
    /* Statistics */
    time_t con_start; /**< time connection was started */
    uint32_t con_pkts; /**< number of processed packets */
    uint32_t con_reset; /**< The number of times the connection has encounterd a reset condition. */
    uint32_t con_nonces; /**< The number of times a nonce has been sent. */
    uint32_t con_rekeys; /**< The number of times the session key has been set. */
    uint64_t con_entropy; /**< The number of bytes of entropy recived. */
    uint32_t key_temp; /**< Last reported key temerature in deci-kelvin */
    uint32_t key_voltage; /**< Last internal supply voltage reported by key. */
    uint32_t fips_frame_rate; /**< fips frame rate. */
    uint32_t fips_frame_num; /**< Number of last fips frames generated. */
    time_t fips_frame_time; /**< Time of last fips frame. */

    uint32_t key_raw_entl; /**< raw estimated shanons per bit of left input */
    uint32_t key_raw_entr; /**< raw estimated shanons per bit of right input */
    uint32_t key_raw_entx; /**< raw estimated shanons per bit after xor */

    uint32_t key_dbsd_entl; /**< debiased shanons per bit of left input */
    uint32_t key_dbsd_entr; /**< debiased shanons per bit of left input */
  
    char key_badness; /**< badness indicator \see control.lua */
  
};



econ_state_t *econ_open(const char *key_path, estream_state_t *op_stream);
int econ_run(econ_state_t *con_state);
ekey_state_t econ_state(econ_state_t *con_state);
int econ_get_rd_fd(econ_state_t *con_state);
void econ_setsnum(econ_state_t *state, const char *snum);
int econ_close(econ_state_t *con_state);

/** Get the a connections pem encoded serial number. 
 *
 * This obtains a PEM64 encoded string representation of the serial number for
 * the passed connection state. The returned string is null terminated and
 * allocated on the heap. The caller owns the allocated memory and must deal
 * with it apropriately. If memory cannot be allocated for the string or there
 * is no serial number associated with the connection NULL is returned.
 *
 * @param state The connection context.
 * @return A PEM64 encoded string or NULL on error.
 */
char *econ_getsnum(econ_state_t *state);

#endif
