/* daemon/keydb.c
 *
 * Entropy key keys database handling.
 *
 * Copyright 2009 Simtec Electronics
 *
 * For licence terms refer to the COPYING file.
 */

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>

#include "keydb.h"
#include "pem.h"

/** Keyring entry. */
struct snum_to_key_s {
    struct snum_to_key_s *next; /**< Next entry. */
    uint8_t snum[12]; /**< Serial number. */
    uint8_t ltkey[32]; /**< Long term session key. */
};

static struct snum_to_key_s *ents = NULL;

/** Find a long term saession key from a serial number.
 *
 * @param snum The serial number to find the key for.
 * @return The keyring entry or NULL if no matching serial number is found.
 */
static struct snum_to_key_s *
find_ltkey(const uint8_t *snum)
{
    struct snum_to_key_s *ent = ents;

    while (ent != NULL) {
        if (memcmp(ent->snum, snum, 12) == 0)
            break;
        ent = ent->next;
    }
    return ent;
}

/* exported interface documented in keydb.h */
uint8_t *
snum_to_ltkey(const uint8_t *snum)
{
    struct snum_to_key_s *ent;
    uint8_t *key = NULL;

    ent = find_ltkey(snum);
    if (ent != NULL) {
        key = malloc(32);
        if (key != NULL) {
            memcpy(key, ent->ltkey, 32);
        }
    }
    return key;
}

/* exported interface documented in keydb.h */
int
output_key(FILE *fh, const uint8_t *snum, const uint8_t *ltkey)
{
    uint8_t data[128];

    pem64_encode_bytes(snum, 12, (char *)data);
    data[16] = ' ';
    pem64_encode_bytes(ltkey, 32, (char *)data + 17);
    data[61] = 0;
    return fprintf(fh, "%s\n", data);
}

/* exported interface documented in keydb.h */
int
add_ltkey(const uint8_t *snum, const uint8_t *ltkey)
{
    struct snum_to_key_s *ent;

    ent = find_ltkey(snum);
    if (ent == NULL) {
        /* no current entry for that serial number */
        ent = calloc(1, sizeof(struct snum_to_key_s));
        memcpy(ent->snum, snum, 12);
        ent->next = ents;
        ents = ent;
    }

    memcpy(ent->ltkey, ltkey, 32);

    return 0;
}

/* exported interface documented in keydb.h */
int
write_keyring(const char *fname)
{
    int fd;
    FILE *fh;
    struct snum_to_key_s *base = ents;
    char *fname2 = malloc(strlen(fname) + 5);
    
    strcpy(fname2, fname);
    strcat(fname2, ".tmp");
    
    fd = open(fname2, O_CREAT | O_EXCL | O_WRONLY, 0600);
    if (fd == -1) {
        /* Unable to open keyring */
        perror("open");
        free(fname2);
        return -1;
    }
    
    fh = fdopen(fd, "w");
    if (fh == NULL) {
        free(fname2);
        close(fd);
        return -1;
    }
    
    fprintf(fh, "# Do not edit this directly, this file is managed by ekey-setkey\n");

    while (base) {
        output_key(fh, base->snum, base->ltkey);
        base = base->next;
    }

    fflush(fh);
    fclose(fh);
    
    if (rename(fname2, fname) == -1) {
        perror("rename");
        free(fname2);
        unlink(fname2);
        return -1;
    }
    
    free(fname2);
    
    return 0;
}

/* exported interface documented in keydb.h */
int
read_keyring(const char *fname)
{
    FILE *fh;
    char line[512];
    char snumpem[17];
    char ltkeypem[45];
    int res;
    int keys = 0;
    uint8_t snum[12];
    uint8_t ltkey[33];
    struct snum_to_key_s *base = ents;

    fh = fopen(fname, "r");
    if (fh == NULL) {
        /* Unable to open keyring */
        return -1;
    }

    /* Blow the old keyring out of the water */
    while (base) {
        struct snum_to_key_s *next = base->next;
        free(base);
        base = next;
    }

    ents = NULL;

    while (fgets(line, 512, fh) != NULL) {
        res = sscanf(line,
                     " %16[A-Za-z0-9+/=] %44[A-Za-z0-9+/=]",
                     snumpem,
                     ltkeypem);

        if (res == 2) {
            pem64_decode_bytes(snumpem, 16, snum);
            pem64_decode_bytes(ltkeypem, 44, ltkey);
            add_ltkey(snum, ltkey);
            keys++;
        }
    }

    fclose(fh);

    return keys;
}
