/* daemon/lt-rekey.c
 *
 * Entropy key long term re-keying tool.
 *
 * Copyright 2009 Simtec Electronics
 *
 * For licence terms refer to the COPYING file.
 */

#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <getopt.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>

#include "pem.h"
#include "skeinwrap.h"
#include "util.h"
#include "nonce.h"
#include "crc8.h"
#include "stream.h"
#include "frame.h"
#include "packet.h"
#include "keydb.h"

#define STDIN   0

/** Exit code returned when utility is unable to validate command line */
#define EXIT_CODE_CMDLINE 1
/** Exit code returned when utility is unable to read keyring */
#define EXIT_CODE_LOADKEYRING 2
/** Exit code returned when master key is unusable */
#define EXIT_CODE_MASTERKEY 3
/** Exit code returned when entropy key device is inacessible */
#define EXIT_CODE_EKEYERR 4
/** Exit code returned when utility is unable to write updated keyring */
#define EXIT_CODE_WRITEKEYRING 6

#define DEVEKEY "/dev/entropykey/"

/** Conenction state. */
struct econ_state_s {
    uint8_t *mkey; /**< Master key. */
    uint8_t *snum; /**< Serial number */

    estream_state_t *key_stream; /**< The input stream. */
    eframe_state_t *eframer; /**< The framer attached to the stream. */
    epkt_state_t *epkt; /**< The packet handler attached to the framer. */
};


static uint8_t default_session_key[32];
static char reset[] = {0x3};

uint8_t *
calc_mac(uint8_t *snum, uint8_t *mkey, uint8_t *nonce, int nonce_len)
{
    EKeySkein sk_mac;
    uint8_t macbuf[32];
    uint8_t *mac;

    mac = calloc(1, 6);

    PrepareSkein(&sk_mac, snum, mkey, EKEY_SKEIN_PERSONALISATION_LKMS);
    Skein_256_Update(&sk_mac, nonce, nonce_len);
    Skein_256_Final_Pad(&sk_mac, macbuf);

    mac[0] = macbuf[0];
    mac[1] = macbuf[1];
    mac[2] = macbuf[2];
    mac[3] = macbuf[29];
    mac[4] = macbuf[30];
    mac[5] = macbuf[31];
    return mac;
}

static const char *usage =
    "Usage: %s [-d] [-h] [-n] [-f <keyring>] [-m <master>]\n"
    "       [-s <serial>] <path>\n"
    "Entropy key device long term session key tool\n\n"
    "\t-v Print version and exit.\n"
    "\t-h Print this help text and exit.\n"
    "\t-n Do not update the keyring with the result.\n"
    "\t-f The path to the keyring to update.\n"
    "\t-m The master key of the device being updated.\n"
    "\t-s The serial number of the device being updated.\n\n";

int
get_keyring(const char *keyring_filename)
{
    int res;
    res = read_keyring(keyring_filename);
    if (res < 0) {
        fprintf(stderr,
                "Unable to read the keyring file %s (%s).\n",
                keyring_filename,
                strerror(errno));
        return -1;
    }
    return res;
}

char *
strdupcat(const char *s1, const char *s2)
{
    char *rs;
    int s1_l, s2_l;

    s1_l = strlen(s1);
    s2_l = strlen(s2);
    rs = calloc(1, s1_l + s2_l + 1 );

    memcpy(rs, s1, s1_l);
    memcpy(rs + s1_l, s2, s2_l);
    return rs;
}

int
put_keyring(const char *keyring_filename)
{
    int res;
    char *tmp_keyring_filename;

    tmp_keyring_filename = strdupcat(keyring_filename, ".new");

    res = write_keyring(tmp_keyring_filename);
    if (res < 0) {
        fprintf(stderr,
                "Unable to write the keyring file %s (%s).\n",
                tmp_keyring_filename,
                strerror(errno));
        free(tmp_keyring_filename);
        return -1;

    }
    res = rename(tmp_keyring_filename, keyring_filename);
    if (res < 0) {
        fprintf(stderr,
                "Unable to replace keyring file %s with %s(%s).\n",
                keyring_filename,
                tmp_keyring_filename,
                strerror(errno));
    }

    free(tmp_keyring_filename);
    return res;
}

uint8_t *
extract_master_key(const char *s, int len)
{
    uint8_t *mkey = NULL; /** Master key. */
    int res;
    uint8_t crc;

    mkey = malloc(33);
    if (mkey == NULL)
        return NULL;

    res = pem64_decode_bytes(s, len, mkey);
    if (res == 33) {
        /* 33 bytes in pem - last byte should be crc8 checksum */
        crc = crc8(mkey, 32);
        if (crc != mkey[32]) {
            fprintf(stderr,
                    "The provided master key's check digit is incorrect.\n"
		    "Please check for typing errors and character substitution.\n");
            free(mkey);
            return NULL;
        }
    } else if (res != 32) {
        fprintf(stderr,
                "The key given did not decode to the correct length. (%d/32)\n",
                res);
        free(mkey);
        return NULL;
    }
    return mkey;
}

int
main(int argc, char **argv)
{
    uint8_t *mkey = NULL; /** Master key. */
    uint8_t *snum = NULL; /** Serial number */
    char *key_path = NULL;

    estream_state_t *key_stream ; /** The input stream. */
    epkt_state_t *epkt; /* The packet handler attached to the framer. */

    int opt;
    int res;
    uint8_t data[128];
    uint8_t nonce[12];
    uint8_t *mac;
    EKeySkein rekeying_state;
    uint8_t session_key[32];
    int retries;
    char *keyring_filename;
    bool nokeyring = false;

    keyring_filename = strdup(KEYRINGFILE);

    while ((opt = getopt(argc, argv, "vhnf:s:m:")) != -1) {
        switch (opt) {
        case 's': /* set serial number */
            snum = malloc(12);
            res = pem64_decode_bytes(optarg, 16, snum);
            if (res != 12) {
                fprintf(stderr, "The serial number given is not the correct length. (%d/12)\n", res);
                return EXIT_CODE_CMDLINE;
            }
            break;

        case 'm': /* set master key */
            mkey = extract_master_key(optarg, strlen(optarg));
            if (mkey == NULL)
                return EXIT_CODE_CMDLINE;
            break;

        case 'f': /* set keyring filename */
            free(keyring_filename);
            keyring_filename = strdup(optarg);
            break;

        case 'n': /* do not update the keyring */
            nokeyring = true;
            break;

        case 'v': /* print version number */
            printf("%s: Version 1.1\n", argv[0]);
            return 0;

        case 'h':
        default:
            fprintf(stderr, usage, argv[0]);
            return EXIT_CODE_CMDLINE;
        }
    }

    if (optind >= argc) {
        if (snum == NULL) {
            fprintf(stderr, "A device path must be given.\n");
            fprintf(stderr, usage, argv[0]);
            return EXIT_CODE_CMDLINE;
        } else {
            key_path = calloc(1, 17 + strlen(DEVEKEY));
            memcpy(key_path, DEVEKEY, 1 + strlen(DEVEKEY));
            pem64_encode_bytes(snum, 12, key_path + 16);
        }
    } else {
        key_path = strdup(argv[optind]);
    }


    /* load keyring */
    if (nokeyring == false) {
        if (get_keyring(keyring_filename) < 0) {
            free(key_path);
            return EXIT_CODE_LOADKEYRING;
        }
    }

    /* ensure master key */
    if (mkey == NULL) {
        char s[55];
        int sidx;
        int sodx;
        int slen;

        if (isatty(STDIN) == 0) {
            fprintf(stderr, "A master key must be given.\n");
            free(key_path);
            return EXIT_CODE_MASTERKEY;
        }
        printf("Please enter a master key: ");
        if (fgets(s, sizeof(s), stdin) == NULL) {
            perror("fgets");
        }

        /* we must allow for the user entering spaces in the input */
        slen = strlen(s);
        sidx = sodx = 0;
        while ((sidx < slen) && (s[sidx] != 0)) {
            s[sodx] = s[sidx];
            if (s[sidx] != ' ') {
                sodx++;
            }
            sidx++;
        }
        s[sodx] = 0;

        mkey = extract_master_key(s, sodx);
        if (mkey == NULL) {
            free(key_path);
            return EXIT_CODE_MASTERKEY;
	}
    }

    /* open entropy key device */
    key_stream = estream_open(key_path);
    if (key_stream == NULL) {
        perror("Error");
        fprintf(stderr, "Unable to open %s as the entropy key device.\n", key_path);
        free(key_path);
        return EXIT_CODE_EKEYERR;
    }
    free(key_path);

    epkt = epkt_open(eframe_open(key_stream));

    /* reset key */
    estream_write(key_stream, reset, 1);
    epkt_setsessionkey(epkt, NULL, default_session_key);

    /* wait for serial packet */
    retries = 20;
    do {
        res = epkt_read(epkt, data, 128);
        if (res <= 0) {
            if (errno == EWOULDBLOCK)
                continue;

            perror("Unexpected error");
            return 2;

        } else if (epkt->pkt_type == PKTTYPE_SNUM) {
            break;
        }

        /* reset key */
        estream_write(key_stream, reset, 1);
        epkt_setsessionkey(epkt, NULL, default_session_key);
        retries--;

    } while (retries > 0);

    if (retries == 0) {
        fprintf(stderr, "Timeout obtaining serial number from key.\n");
        return 3;
    }

    if (res != 12) {
        fprintf(stderr, "Bad serial number from key.\n");
        return 4;
    }

    if (snum == NULL) {
        /* no serial number */
        snum = malloc(res);
        memcpy(snum, data, res);
    } else {
        /* ensure serial number matches */
        if (memcmp(snum, data, 12) != 0) {
            fprintf(stderr, "Serial number did not match the one specified.\n");
            return 4;
        }
    }

    /* Initialise the MAC checksum using the serial number and the default
     * shared key
     */
    epkt_setsessionkey(epkt, snum, default_session_key);

    /* Prepare a nonce */
    if (fill_nonce(nonce, 12) != true) {
        fprintf(stderr, "Unable to generate nonce.\n");
        return 1;
    }
    close_nonce();

    /* send nonce MAC */
    mac = calc_mac(snum, mkey, nonce, 12);
    data[0] = 'M';
    pem64_encode_bytes(mac, 6, (char *)data + 1);
    estream_write(key_stream, data, 9);

    /* wait for MAC ack packet */
    retries = 20;
    do {
        res = epkt_read(epkt, data, 128);
        if (res <= 0) {
            if (errno == EWOULDBLOCK)
                continue;

            perror("Unexpected error");
            return 2;
        }

        if (epkt->pkt_type == PKTTYPE_LTREKEYMAC)
            break;

        retries--;
    } while (retries > 0);

    if (retries == 0) {
        fprintf(stderr, "Timeout obtaining MAC acknowledgement packet.\n");
        return 3;
    }

    data[0] = 'L';
    data[17] = '.';
    pem64_encode_bytes(nonce, 12, (char *)data + 1);
    estream_write(key_stream, data, 18);

    /* wait for rekey ack packet */
    do {
        res = epkt_read(epkt, data, 128);
        if (res <= 0) {
            if (errno == EWOULDBLOCK)
                continue;

            if (errno == EPROTO) {
                fprintf(stderr, "Provided master key does not match the device's.\n");
                return 2;
            }

            perror("Unexpected error");
            return 2;
        }
    } while (epkt->pkt_type != PKTTYPE_LTREKEY);

    /* calculate new longterm key */
    PrepareSkein(&rekeying_state, snum, &(mkey[0]), EKEY_SKEIN_PERSONALISATION_LRS);
    Skein_256_Update(&rekeying_state, &(data[0]), 32);
    Skein_256_Update(&rekeying_state, nonce, 12);

    Skein_256_Final(&rekeying_state, session_key);

    if (nokeyring == false) {
        add_ltkey(snum, session_key);
        if (put_keyring(keyring_filename) < 0)
            return EXIT_CODE_WRITEKEYRING;
    } else {
        /* just display new key */
        output_key(stdout, snum, session_key);
    }
    return 0;
}
