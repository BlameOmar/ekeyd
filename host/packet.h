/* daemon/packet.h
 *
 * Packetisation layer for ekey protocol
 *
 * Copyright 2009 Simtec Electronics
 *
 * For licence terms refer to the COPYING file.
 */

#ifndef DAEMON_PACKET_H
#define DAEMON_PACKET_H

#include "skeinwrap.h"

typedef enum {
    PKTTYPE_NONE = 0,
    PKTTYPE_KEYREJECTED, /** Packets MAC was rejected. */
    PKTTYPE_SNUM, /**< Serial number. */
    PKTTYPE_INFO, /**< Informational. */
    PKTTYPE_WARN, /**< Warning. */
    PKTTYPE_ENTROPY, /**< Entropy data. */
    PKTTYPE_KEYREQ, /**< Key request. */
    PKTTYPE_KEY, /**< Session key response. */
    PKTTYPE_LTREKEYMAC, /**< Long term rekey mac successful. */
    PKTTYPE_LTREKEY, /**< Long termrekey successful. */
    PKTTYPE_SIZE /* used to size state array, *must* be last */
} pkt_type_t;

/** Entropy key packet processor state. */
typedef struct {
    pkt_type_t pkt_type; /**< The type of the current packet. */
    uint8_t subcode[2]; /**< The sub-code of the current packet. */
    int data_len; /**< The length of the current packet. */

    eframe_state_t *frame; /**< The framer to read from. */

    EKeySkein *sk_mac; /**< The precomputed MAC skein. */

    /* statistics */
    uint32_t pkt_error; /**< The number of packet errors. */
    uint32_t pkt_ok; /**< The number of successfully processed packets. */
} epkt_state_t;

#define PKT_CLASS_ASCII '>'
#define PKT_CLASS_BINARY '!'

#ifndef EPROTO
/* OpenBSD lacks EPROTO (Protocol error) */
#define EPROTO	71
#endif

/** Create a packet processor state read for use.
 *
 * @param frame_state The framer to attach teh packet processor to.
 * @return The new packet processing state or NULL and errno set on error.
 */
extern epkt_state_t *epkt_open(eframe_state_t *frame_state);

/** Finish packet processor state read for use.
 *
 * @param state The packet processing state.
 */
extern int epkt_close(epkt_state_t *state);

/** Read a complete packet.
 *
 * Read a complete packet and decode it into its elements.
 *
 * @param state The packet processing state.
 * @param buf The buffer to place the result data in.
 * @param count The length of \a buf.
 * @return The length of the data placed in \a buf or -1 and errno set.
 */
extern ssize_t epkt_read(epkt_state_t *state, uint8_t *buf, size_t count);

/** Obtain the two subcode bytes.
 *
 * Read the two subcode bytes of teh current packet.
 *
 * @param state The packet processing state.
 */
extern short epkt_get_pemsubcode(epkt_state_t *state);

/** Set the session key to be used for MAC generation.
 */
extern void epkt_setsessionkey(epkt_state_t *state, uint8_t *snum, uint8_t *sharedkey);

#endif /* DAEMON_PACKET_H */
