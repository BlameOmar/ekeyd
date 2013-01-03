/* daemon/packet.c
 *
 * Entropy key packet handling
 *
 * Copyright 2009 Simtec Electronics
 *
 * For licence terms refer to the COPYING file.
 */

#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "pem.h"
#include "skeinwrap.h"
#include "util.h"

#include "stream.h"
#include "frame.h"
#include "packet.h"

/* exported interface documented in packet.h */
epkt_state_t *
epkt_open(eframe_state_t *frame_state)
{
    epkt_state_t *pkt_state;

    if (frame_state == NULL)
        return NULL;

    pkt_state = calloc(1, sizeof(epkt_state_t));
    if (pkt_state == NULL)
        return NULL;

    pkt_state->frame = frame_state;

    return pkt_state;
}

/* exported interface documented in packet.h */
int
epkt_close(epkt_state_t *state)
{
    if (state != NULL) {
        eframe_close(state->frame);

        if (state->sk_mac != NULL)
            free(state->sk_mac);

        free(state);
    }
    return 0;
}

/* exported interface documented in packet.h */
void
epkt_setsessionkey(epkt_state_t *state, uint8_t *snum, uint8_t *sessionkey)
{
    if (snum == NULL)
        return;

    if(sessionkey == NULL)
        return;

    if (state->sk_mac != NULL)
        free(state->sk_mac);

    state->sk_mac = calloc(1, sizeof(EKeySkein));

    PrepareSkein(state->sk_mac, snum, sessionkey, EKEY_SKEIN_PERSONALISATION_PMS);
}

/** Check the current packets MAC is valid.
 *
 * @param state The packet processing state.
 * @return true if the MAC is valid false if it is incorrect.
 */
static bool
verify_mac(epkt_state_t *state)
{
    EKeySkein pkt_mac_sk;
    uint8_t macbuf[32];
    uint8_t pkt_mac[6];
    uint8_t *pem_mac;
    uint8_t *frame;

    if (state->sk_mac == NULL)
        return false;

    frame = state->frame->frame;
    pem_mac = frame + 54;

    /* decode PEM encoded MAC */
    pem64_decode_bytes((char *)pem_mac, 8, pkt_mac);

    memcpy(&pkt_mac_sk, state->sk_mac, sizeof(pkt_mac_sk));
    Skein_256_Update(&pkt_mac_sk, frame + 2, 52);
    Skein_256_Final_Pad(&pkt_mac_sk, macbuf);

    if ((memcmp(macbuf, pkt_mac, 3) != 0) ||
        (memcmp(macbuf + 29, pkt_mac + 3, 3) != 0)) {
        return false;
    }

    return true;
}

/** Verify incoming packet is well formed.
 *
 * @param state The packet processing state.
 * @return zero if the packet is valid or -1 and errno set.
 */
static int
verify_packet(epkt_state_t *state)
{
    uint8_t *frame;

    frame = state->frame->frame;

    if ((frame[3] != PKT_CLASS_ASCII) && (frame[3] != PKT_CLASS_BINARY)) {
        state->pkt_error++;
        errno = EINVAL; /* unknown packet class */
        return -1;
    }

    if ((state->sk_mac == NULL) && (state->pkt_type == PKTTYPE_SNUM)) {
        /* serial number packets MAC dont have to pass if the skein hasnt
         * been initialised because we dont have the serial number yet!
         */
        state->pkt_ok++;
        return 0;
    }

    if (verify_mac(state) == false) {
        state->pkt_error++;
        state->pkt_type = PKTTYPE_KEYREJECTED; /* packet failed mac */
    }  else {
        state->pkt_ok++;
    }

    return 0;
}

/** Determine the packet type.
 *
 * Decode the packet type from the frame data.
 *
 * @param state A frame state.
 * @return The packet type.
 */
static pkt_type_t
get_pkt_type(eframe_state_t *state)
{
    switch (state->frame[2]) {
    case 'S':
        return PKTTYPE_SNUM;

    case 'I':
        return PKTTYPE_INFO;

    case 'W':
        return PKTTYPE_WARN;

    case 'E':
        return PKTTYPE_ENTROPY;

    case 'k':
        return PKTTYPE_KEYREQ;

    case 'K':
        return PKTTYPE_KEY;

    case 'M':
        return PKTTYPE_LTREKEYMAC;

    case 'L':
        return PKTTYPE_LTREKEY;

    default:
        return PKTTYPE_NONE;
    }

    return PKTTYPE_NONE;
}

/** Extract and decode the contents of a packet.
 *
 * @param state The packet state.
 * @param buf The buffer to place the output data in.
 * @param count the size of \a buf.
 * @return The length of the decoded data or -1 and errno set.
 */
static ssize_t
decode_packet(epkt_state_t *state, uint8_t *buf, size_t count)
{
    uint8_t *frame;
    int len;

    frame = state->frame->frame;

    if (frame[3] == PKT_CLASS_BINARY) {
        /* decode PEM encoded data */
        //printf("decoding: %.48s\n", frame + 6);
        len = pem64_decode_bytes((char *)(frame + 6), 48, buf);
        //printf("decoded: %s\n", phex(buf,len));
        state->subcode[0] = frame[4];
        state->subcode[1] = frame[5];
    } else {
        /* ASCII packet find the end of data */
        for (len = 50; len > 0 ; len--) {
            if (frame[len + 3] != ' ')
                break;
        }
        //printf("copying: %.*s\n", len,frame + 4);
        memcpy(buf, frame + 4, len);
        buf[len] = 0; /* null terminate the string */
        state->data_len = len;
        state->subcode[0] = 0;
        state->subcode[1] = 0;
    }

    return len;
}

/* exported interface documented in packet.h */
short
epkt_get_pemsubcode(epkt_state_t *state)
{
    return pem64_decode_12bits((char *)state->subcode);
}

/* exported interface documented in packet.h */
ssize_t
epkt_read(epkt_state_t *state, uint8_t *buf, size_t count)
{
    int frame_len;

    frame_len = eframe_read(state->frame);
    if (frame_len <= 0) {
        return frame_len; /* propogate error */
    }

    /* Clear used data from framer context. This means the frame data is only
     * safe to reference until the next call to eframe_read()
     */
    state->frame->used = 0;

    /* find the type of packet we are dealing with */
    state->pkt_type = get_pkt_type(state->frame);
    if (state->pkt_type == PKTTYPE_NONE) {
        errno = EPROTO;
        return -1;
    }

    /* verify the packets type and MAC */
    if (verify_packet(state) < 0) {
        return -1; /* propogate error */
    }

    /* decode packets data */
    return decode_packet(state, buf, count);
}
