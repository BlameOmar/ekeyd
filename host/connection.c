/* daemon/connection.c
 *
 * Entropy key daemon connection handling
 *
 * Copyright 2009 Simtec Electronics
 *
 * For licence terms refer to the COPYING file.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <syslog.h>
#include <time.h>

#include "pem.h"
#include "skeinwrap.h"
#include "util.h"
#include "nonce.h"

#include "stream.h"
#include "frame.h"
#include "packet.h"
#include "keydb.h"
#include "connection.h"

/** The maximum number of packets following a keying request before we
 * issue a reset instead of waiting.
 */
#define MAX_PACKETS_BEFORE_RESET 5

/* Number of rekey requests to stay in ESTATE_KEYED_BAD after a bad session keying */
#define MAX_REKEYS_BEFORE_RESET 50

/* The minimum number of bytes in a shannon info frame to allow updates */
#define MIN_SHANNON_SIZE 100

typedef ekey_state_t (*pkt_handler_t)(econ_state_t *state, uint8_t *buf, size_t count);

pkt_handler_t pkt_handlers[ESTATE_SIZE][PKTTYPE_SIZE];

#define SHARED_KEY_DEFAULT "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"

uint8_t default_session_key[32];

/** Null packet handler
 *
 * Handler for packets which have no action in the current state.
 *
 * @param state The current connection state.
 * @param buf The packets data.
 * @param count The length of the data in \a buf.
 * @return The state machine state to change to.
 */
static ekey_state_t
null_pkt_handler(econ_state_t *state, uint8_t *buf, size_t count)
{
    return state->current_state;
}

/** Reset packet handler
 *
 * Handler for packets which cause the reset of the entropy key connection.
 *
 * @param state The current connection state.
 * @param buf The reset packets data, will be unused.
 * @param count The length of the data in \a buf, should be 0.
 * @return The initial state machine state.
 */
static ekey_state_t
reset_pkt_handler(econ_state_t *state, uint8_t *buf, size_t count)
{
    char reset[] = {0x3};
    estream_write(state->key_stream, reset, 1);
    epkt_setsessionkey(state->epkt, state->snum, default_session_key);

    state->con_reset++; /* Update the connection statistics */

#ifdef SBQS_MESSAGES
    printf("DEBUG Connection reset\n");
#endif
    return ESTATE_INIT;
}

/** Bad key packet handler
 *
 * Handler for packets which indicate a bad session key was set up.
 *
 * @param state The current connection state.
 * @param buf The packets data, will be unused.
 * @param count The length of the data in \a buf.
 * @return The unstrusted state machine state.
 */
static ekey_state_t
badkey_pkt_handler(econ_state_t *state, uint8_t *buf, size_t count)
{
    /* session rekeying failed */
    reset_pkt_handler(state, buf, count);

    state->keyreq_counter = 0;

    return ESTATE_KEYED_BAD;
}

/** Key request packet counter
 *
 * Handler for packets which would cause a session keying request if
 *  the previous one had not failed..
 *
 * @param state The current connection state.
 * @param buf The packets data, will be unused.
 * @param count The length of the data in \a buf.
 * @return Either remaining here or resetting.
 */
static ekey_state_t
badkey_count_pkt_handler(econ_state_t *state, uint8_t *buf, size_t count)
{
    char *serialnumber;

    if (state->keyreq_counter++ < MAX_REKEYS_BEFORE_RESET) {
        return state->current_state;
    }

    serialnumber = econ_getsnum(state);
    if (serialnumber == NULL) {
            syslog(LOG_WARNING, "UnknownKey: Retrying keying process.");
    } else {
            syslog(LOG_WARNING, "%s: Retrying keying process.", serialnumber);
            free(serialnumber);
    }

    return reset_pkt_handler(state, buf, count);
}




/** Information packet handler
 *
 * Handler for packets containing information from the entropy device.
 *
 * @param state The current connection state.
 * @param buf The packets data.
 * @param count The length of the data in \a buf.
 * @return The current state machine state as info packets do not change current state.
 */
static ekey_state_t
info_pkt_handler(econ_state_t *state, uint8_t *buf, size_t count)
{
    char *next = (char *)buf;
    char *end;

    uint32_t raw_bytes;
    uint32_t raw_est_l;
    uint32_t raw_est_r;
    uint32_t raw_est_x;
    uint32_t dbsd_bytes_l;
    uint32_t dbsd_est_l;
    uint32_t dbsd_bytes_r;
    uint32_t dbsd_est_r;

    uint32_t fips_frame_num;
    time_t fips_frame_time;

    while (count > 0) {
        switch (next[0]) {
        case 'F': /* fips frame information */
            fips_frame_num = strtoul(next + 1, NULL, 10);
            fips_frame_time = time(NULL);
            if ((fips_frame_time - state->fips_frame_time) > 50) {
                state->fips_frame_rate = ((fips_frame_num - state->fips_frame_num) * 100) / (fips_frame_time - state->fips_frame_time) ;
                state->fips_frame_num = fips_frame_num;
                state->fips_frame_time = fips_frame_time;
            }
            count = 0;
            break;

        case 'S': /* shannon estimate information and gone-bad indicator*/
            raw_bytes = strtoul(next + 1, &next, 10);
            raw_est_l = strtoul(next + 1, &next, 10);
            raw_est_r = strtoul(next + 1, &next, 10);
            raw_est_x = strtoul(next + 1, &next, 10);
            dbsd_bytes_l = strtoul(next + 1, &next, 10);
            dbsd_est_l = strtoul(next + 1, &next, 10);
            dbsd_bytes_r = strtoul(next + 1, &next, 10);
            dbsd_est_r = strtoul(next + 1, &next, 10);
            state->key_badness = *++next;

            if (raw_bytes > MIN_SHANNON_SIZE) {
                state->key_raw_entl = (raw_est_l * 100) / raw_bytes;
                state->key_raw_entr = (raw_est_r * 100) / raw_bytes;
                state->key_raw_entx = (raw_est_x * 100) / raw_bytes;
            }

            if (dbsd_bytes_l > MIN_SHANNON_SIZE) {
                state->key_dbsd_entl = (dbsd_est_l * 100) / dbsd_bytes_l;
            }

            if (dbsd_bytes_r > MIN_SHANNON_SIZE) {
                state->key_dbsd_entr = (dbsd_est_r * 100) / dbsd_bytes_r;
            }

            count = 0;
            break;

        case 'T': /* temprature information */
            state->key_temp = strtoul(next + 1, &end, 10);
            count -= (end - next);
            next = end;
            break;

        case 'V': /* supply volatge information */
            state->key_voltage = strtoul(next + 1, &end, 10);
            count -= (end - next);
            next = end;
            break;

        case ' ': /* information element separator */
            next++;
            count-=1;
            break;

        default:
            count = 0;
            break;
        }
    }
#ifdef SBQS_MESSAGES
    printf("DEBUG Info packet processed\n");
#endif
    return state->current_state;
}

/** Key request packet handler
 *
 * Handler for packets which cause a session keying request. A K packet with a
 * suitable nonce is issued.
 *
 * @param state The current connection state.
 * @param buf The packets data, will be unused.
 * @param count The length of the data in \a buf.
 * @return The session keying state machine state.
 */
static ekey_state_t
keyreq_pkt_handler(econ_state_t *state, uint8_t *buf, size_t count)
{
    char sbuf[32];

    /* fill nonce with apropriate data */
    if (state->nonce == NULL) {
        state->nonce_len = 12;
        state->nonce = malloc(state->nonce_len);
    }

    if (!fill_nonce(state->nonce + sizeof(uint32_t),
                    state->nonce_len - sizeof(uint32_t))) {
        char *serialnumber = econ_getsnum(state);

        if (serialnumber != NULL) {
            syslog(LOG_ERR, "%s: Unable to prepare nonce for keying.  Key no longer trusted.", serialnumber);
            free(serialnumber);
        } else {
            syslog(LOG_ERR, "UnknownKey: Unable to prepare nonce for keying.  Key no longer trusted.");
        }
        return ESTATE_UNTRUSTED;
    }
    *(uint32_t *)state->nonce = state->con_nonces;

    pem64_encode_bytes(state->nonce, 12, sbuf + 1);
    sbuf[0] = 'K';
    sbuf[17] = '.';

    estream_write(state->key_stream, sbuf, 18);

    state->con_nonces++;
    state->keyreq_counter = 0;
#ifdef SBQS_MESSAGES
    printf("DEBUG Issued keying request\n");
#endif
    return ESTATE_SESSION_SENT;
}

/** Key request packet counter
 *
 * Handler for packets which would cause a session keying request if
 * we hadn't already sent one.
 *
 * @param state The current connection state.
 * @param buf The packets data, will be unused.
 * @param count The length of the data in \a buf.
 * @return Either remaining here or resetting.
 */
static ekey_state_t
keyreq_count_pkt_handler(econ_state_t *state, uint8_t *buf, size_t count)
{
    char *serialnumber;

    serialnumber = econ_getsnum(state);
    if (serialnumber == NULL) {
        serialnumber = strdup("UnknownKey");
        if (serialnumber == NULL) {
            /* Complete faliure to allocate serial number string. Either system
             * is unable to fufil the small request or heap is corrupt. Only
             * course of action is to reset the key.
             */
            syslog(LOG_ERR, "Unable to allocate serial number string buffer. Resetting state machine and device.");

            return reset_pkt_handler(state, buf, count);
        }
    }

    if (state->keyreq_counter++ < MAX_PACKETS_BEFORE_RESET) {
        syslog(LOG_WARNING, "%s: Repeated key request (ignored)", serialnumber);
        free(serialnumber);
        return ESTATE_SESSION_SENT;
    }

    syslog(LOG_WARNING, "%s: Too many key requests in a row. Resetting state machine and device.", serialnumber);
    free(serialnumber);

    return reset_pkt_handler(state, buf, count);
}

/** Session rekeying packet handler
 *
 * Handler for session rekeying response packets
 *
 * @param state The current connection state.
 * @param buf A nonce from the key to mix with our state.
 * @param count The length of the nonce in \a buf.
 * @return The keyed session state machine state or resets to connection.
 */
static ekey_state_t
key_pkt_handler(econ_state_t *state, uint8_t *buf, size_t count)
{
    short nonce_len;
    EKeySkein rekeying_state;
    uint8_t session_key[32];

    nonce_len = epkt_get_pemsubcode(state->epkt);
    if (nonce_len != state->nonce_len) {
        /* key read a different length nonce to that transmitted */
        syslog(LOG_ERR, "Mismatched nonce %s", state->key_stream->uri);
        return reset_pkt_handler(state, buf, count);
    }

    PrepareSkein(&rekeying_state, state->snum, &(state->ltkey[0]), EKEY_SKEIN_PERSONALISATION_RS);
    Skein_256_Update(&rekeying_state, &(buf[0]), 32);
    Skein_256_Update(&rekeying_state, state->nonce, state->nonce_len);

    Skein_256_Final(&rekeying_state, session_key);

    epkt_setsessionkey(state->epkt, state->snum, session_key);

    PrepareSkein(&state->session_state, state->snum, session_key, EKEY_SKEIN_PERSONALISATION_EES);

    state->con_rekeys++;

#ifdef SBQS_MESSAGES
    printf("DEBUG Rekeying completed\n");
    printf("ACTIVITY Consuming a single FIPS state\nSIZE 4095\n");
    sbqs_start_timing();
#endif

    return ESTATE_KEYED_FIRST;
}

/** Entropy packet handler
 *
 * Handler for entropy from the device.
 *
 * @param state The current connection state.
 * @param buf The encrypted entropy.
 * @param count The length of the entropy in \a buf.
 * @return The keyed state machine state.
 */
static ekey_state_t
entropy_pkt_handler(econ_state_t *state, uint8_t *buf, size_t count)
{
    EKeySkein session_state;
    short seq_num;
    unsigned char randbuffer_encbytes[32];
    int loop;

    seq_num = epkt_get_pemsubcode(state->epkt);

    /* copy precomputed state */
    memcpy(&session_state, &state->session_state, sizeof(EKeySkein));

    /* update with sequence_number */
    Skein_256_Update(&session_state, state->epkt->subcode, 2);

    /* finalise skein into buffer */
    Skein_256_Final(&session_state, randbuffer_encbytes);

    /* decode data */
    for (loop = 0; loop < 32; ++loop) {
        buf[loop] ^= randbuffer_encbytes[loop];
    }

    /* send data to output stream */
    estream_write(state->op_stream, buf, count);

    /* update statistics */
    state->con_entropy += count;

#ifdef SBQS_MESSAGES
    printf("DONE %d\n", seq_num);
    if (seq_num == 4095) {
        /* When processing in SBQS mode, we should stop now */
        estream_close(state->key_stream);
        state->key_stream = NULL;
        sbqs_end_timing();
        return ESTATE_CLOSE;
    }
#endif

    if (seq_num == 4095)
        return keyreq_pkt_handler(state, NULL, 0);

    return ESTATE_KEYED;
}

/** Serial number packet handler
 *
 * Handler for serial number packets.
 *
 * @param state The current connection state.
 * @param buf The serial number.
 * @param count The length of the serial number in \a buf.
 * @return The session keying required state machine state.
 */
static ekey_state_t
snum_pkt_handler(econ_state_t *state, uint8_t *snum, size_t count)
{
    /* bad serial number length - reset the conenction */
    if (count < 12)
        return reset_pkt_handler(state, snum, count);

    if (state->snum == NULL) {
        /* no serial number */
        state->snum = malloc(count);
        memcpy(state->snum, snum, count);
        state->snum_len = count;
    } else {
        /* ensure serial number matches */
        if (memcmp(state->snum, snum, state->snum_len) != 0) {
            syslog(LOG_ERR, "Serial number given did not match key %s", state->key_stream->uri);
            return ESTATE_UNTRUSTED;
        }
    }

    /* if the serial number is verified to the device, Initialise the MAC
     * checksum using the serial number and the default shared key
     */
    epkt_setsessionkey(state->epkt, state->snum, default_session_key);

    /* ensure we dont leak long term keys */
    if (state->ltkey != NULL)
        free(state->ltkey);

    state->ltkey = snum_to_ltkey(state->snum);

    if (state->ltkey == NULL) {
        /* we cannot generate session keys without the private long term
         * session key.
         */
        syslog(LOG_ERR, "Private key unavailable for %s", state->key_stream->uri);
        return ESTATE_UNTRUSTED;
    }


    return ESTATE_SESSION;
}

/** Sets the default handler for connection state machine.
 *
 * @param state The state being setup.
 */
static pkt_handler_t
default_handler_for(int state)
{
    switch (state) {
    case ESTATE_CLOSE:
    case ESTATE_UNTRUSTED:
    case ESTATE_KEYED_BAD:
        return null_pkt_handler;
    }

    return reset_pkt_handler;
}

/** Initialise connection state machine.
 */
static void
init_states(void)
{
    int state;
    int pkttype;

    /* initialy we populate the entire state system with the reset response */
    for (state = 0; state < ESTATE_SIZE; state++) {
        for (pkttype = 0; pkttype < PKTTYPE_SIZE; pkttype++) {
            pkt_handlers[state][pkttype] = default_handler_for(state);
        }
    }

    pkt_handlers[ESTATE_INIT][PKTTYPE_SNUM] = snum_pkt_handler;

    pkt_handlers[ESTATE_SESSION][PKTTYPE_INFO] = info_pkt_handler;
    pkt_handlers[ESTATE_SESSION][PKTTYPE_KEYREQ] = keyreq_pkt_handler;

    pkt_handlers[ESTATE_SESSION_SENT][PKTTYPE_KEYREQ] = keyreq_count_pkt_handler;
    pkt_handlers[ESTATE_SESSION_SENT][PKTTYPE_INFO] = info_pkt_handler;
    pkt_handlers[ESTATE_SESSION_SENT][PKTTYPE_KEY] = key_pkt_handler;

    pkt_handlers[ESTATE_KEYED_FIRST][PKTTYPE_ENTROPY] = entropy_pkt_handler;
    pkt_handlers[ESTATE_KEYED_FIRST][PKTTYPE_INFO] = info_pkt_handler;
    pkt_handlers[ESTATE_KEYED_FIRST][PKTTYPE_KEYREJECTED] = badkey_pkt_handler;

    pkt_handlers[ESTATE_KEYED_BAD][PKTTYPE_KEYREQ] = badkey_count_pkt_handler;

    pkt_handlers[ESTATE_KEYED][PKTTYPE_ENTROPY] = entropy_pkt_handler;
    pkt_handlers[ESTATE_KEYED][PKTTYPE_INFO] = info_pkt_handler;
    pkt_handlers[ESTATE_KEYED][PKTTYPE_KEYREQ] = keyreq_pkt_handler;
}

/** Create a new connection to an entropy key.
 *
 * @param key_path The path to the key to open.
 * @param op_stream The output stream to send entropy to.
 * @return The new connection state or NULL and errno set.
 */
econ_state_t *
econ_open(const char *key_path, estream_state_t *op_stream)
{
    econ_state_t *state;
    estream_state_t *key_stream;

    init_states();

    key_stream = estream_open(key_path);
    if (key_stream == NULL) {
        perror("Input: ");
        return NULL;
    }

    state = calloc(1, sizeof(econ_state_t));
    if (state == NULL) {
        estream_close(key_stream);
        return NULL;
    }

    state->key_stream = key_stream;
    state->op_stream = op_stream;
    state->eframer = eframe_open(state->key_stream);
    state->epkt = epkt_open(state->eframer);
    state->current_state = ESTATE_INIT;
    state->key_badness = 0;                 /* efm_ok, see control.lua */

    state->con_start = time(NULL);

    return state;
}

/** Run the state machine against the ekey input stream and write to the output
 * entropy stream.
 *
 * @param con_state The connection context.
 * @return The length of the read packet or -1 and errno set.
 */
int econ_run(econ_state_t *con_state)
{
    int res;
    uint8_t data[128];

    if (con_state->current_state == ESTATE_CLOSE) {
        /* State machine is in closedown, do not run. */
        return 0;
    }

    res = epkt_read(con_state->epkt, data, 128);
    if (res == 0) {
        con_state->current_state = ESTATE_CLOSE;
    } else if (res < 0) {
        /* errors */
        switch (errno) {

        case EWOULDBLOCK:
            /* no packet available, try again */
            break;

        default:
            perror("epkt_read");
            con_state->current_state = ESTATE_CLOSE;
            break;
        }
    } else {
        con_state->con_pkts++;
        con_state->current_state = pkt_handlers[con_state->current_state][con_state->epkt->pkt_type](con_state, data, res);
    }

    return res;
}

/** Get a connections state machine state.
 *
 * @param con_state The connection context.
 * @return The state machine state.
 */
ekey_state_t
econ_state(econ_state_t *con_state)
{
    return con_state->current_state;
}

/** Get a connections output file descriptor.
 *
 * @param con_state The connection context.
 * @return The file descriptor.
 */
int
econ_get_rd_fd(econ_state_t *con_state)
{
    return con_state->key_stream->fd;
}

/** Set the a connections serial number.
 *
 * @param con_state The connection context.
 * @param snum The pem encoded serial number.
 */
void
econ_setsnum(econ_state_t *state, const char *snum)
{
    state->snum = malloc(12);
    state->snum_len = pem64_decode_bytes(snum, 16, state->snum);
}

/* exported interface, documented in connection.h */
char *
econ_getsnum(econ_state_t *state)
{
    char *serial_str;

    if (state->snum == NULL)
        return NULL;

    serial_str = calloc(1, pem64_encode_bytes_length(state->snum_len) + 1);
    if (serial_str == NULL)
        return NULL;

    pem64_encode_bytes(state->snum, state->snum_len, serial_str);

    return serial_str;
}

/** Shutdown a connection and free its context.
 *
 * @param state The connection context.
 * @return zero.
 */
int
econ_close(econ_state_t *state)
{
    if (state->epkt != NULL)
        epkt_close(state->epkt);
    if (state->key_stream != NULL)
        estream_close(state->key_stream);
    if (state->snum != NULL)
        free(state->snum);
    if (state->ltkey != NULL)
        free(state->ltkey);
    if (state->nonce != NULL)
        free(state->nonce);
    free(state);
    return 0;
}
