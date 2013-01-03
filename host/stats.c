/* daemon/stats.c
 *
 * Entropy key daemon statistics handling
 *
 * Copyright 2009 Simtec Electronics
 *
 * For licence terms refer to the COPYING file.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "ekeyd.h"
#include "stream.h"
#include "frame.h"
#include "packet.h"
#include "connection.h"
#include "stats.h"

/* exported interface, documented in stats.h */
connection_stats_t *
get_key_stats(OpaqueEkey *ekey)
{
    connection_stats_t *stats;

    if (ekey == NULL)
        return NULL;

    stats = calloc(1, sizeof(connection_stats_t));

    if (stats == NULL)
        return NULL;

    /* values held in ekey structure we already checked is valid */
    stats->con_start = ekey->con_start;
    stats->con_pkts = ekey->con_pkts;
    stats->con_reset = ekey->con_reset;
    stats->con_nonces = ekey->con_nonces;
    stats->con_rekeys = ekey->con_rekeys;
    stats->con_entropy = ekey->con_entropy;

    stats->key_temp = ekey->key_temp;
    stats->key_voltage = ekey->key_voltage;
    stats->key_badness = ekey->key_badness;

    stats->fips_frame_rate = ekey->fips_frame_rate;
    stats->key_raw_entl = ekey->key_raw_entl;
    stats->key_raw_entr = ekey->key_raw_entr;
    stats->key_raw_entx = ekey->key_raw_entx;

    stats->key_dbsd_entl = ekey->key_dbsd_entl;
    stats->key_dbsd_entr = ekey->key_dbsd_entr;

    /* stats held in stream structure */
    if (ekey->key_stream != NULL) {
        stats->stream_bytes_read = ekey->key_stream->bytes_read;
        stats->stream_bytes_written = ekey->key_stream->bytes_written;
    }

    /* stats held in packet structure */
    if (ekey->epkt != NULL) {
        stats->pkt_error = ekey->epkt->pkt_error;
        stats->pkt_ok = ekey->epkt->pkt_ok;

        /* stats held in frame structure */
        if (ekey->epkt->frame != NULL) {
            stats->frame_byte_last = ekey->epkt->frame->byte_last;
            stats->frame_framing_errors = ekey->epkt->frame->framing_errors;
            stats->frame_frames_ok = ekey->epkt->frame->frames_ok;
        }
    }

    return stats;
}
