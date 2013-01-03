/* daemon/stats.h
 *
 * Interface to Entropy Key statistics
 *
 * Copyright 2009 Simtec Electronics
 *
 * For licence terms refer to the COPYING file.
 */

#ifndef DAEMON_STATS_H
#define DAEMON_STATS_H

/** Unified statistics. */
typedef struct {
    uint64_t stream_bytes_read; /**< Number of bytes read from the stream. */
    uint64_t stream_bytes_written; /**< Number of bytes written to the stream. */

    uint64_t frame_byte_last; /**< Index of begining of last correct frame. */
    uint32_t frame_framing_errors; /**< Number of framing errors. */
    uint32_t frame_frames_ok; /**< Number of valid frames. */

    uint32_t pkt_error; /**< Number of packets with an error. */
    uint32_t pkt_ok; /**< Number of ok packets. */

    time_t con_start; /**< Time the connection was started. */

    uint32_t con_pkts; /**< Number of processed packets. */
    uint32_t con_reset; /**< The number of times the connection has encounterd a reset condition. */
    uint32_t con_nonces; /**< The number of times a nonce has been sent. */
    uint32_t con_rekeys; /**< The number of times the session key has been set. */
    uint64_t con_entropy; /**< The number of bytes of entropy recived. */

    int key_temp; /**< Last reported key temerature in deci-kelvin. */
    int key_voltage; /**< Last internal supply voltage reported by key. */
    char key_badness; /**< badness indicator \see control.lua */

    uint32_t fips_frame_rate; /**< Number of fips frames generated. */

    uint32_t key_raw_entl; /**< raw estimated shanons per bit of left input. */
    uint32_t key_raw_entr; /**< raw estimated shanons per bit of right input. */
    uint32_t key_raw_entx; /**< raw estimated shanons per bit after xor. */

    uint32_t key_dbsd_entl; /**< debiased shanons per bit of left input. */
    uint32_t key_dbsd_entr; /**< debiased shanons per bit of right input. */

} connection_stats_t;

/** Create a unified statistics structure from an entropy key connection state.
 *
 * @note The returned structure must be freed by the caller.
 *
 * @param ekey The connection context.
 * @return A statistics structure.
 */
connection_stats_t *get_key_stats(OpaqueEkey *ekey);

#endif /* DAEMON_STATS_H */
