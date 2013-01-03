/* daemon/krnlop.c
 *
 * Entropy key kernel output
 *
 * Copyright 2009 Simtec Electronics
 *
 * For licence terms refer to the COPYING file.
 */

#include <stdint.h>
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>

#include "stream.h"
#include "krnlop.h"

static int krnlop_bpb;

#if defined(EKEY_OS_LINUX)

/* Linux kernel entropy injection */

#include <alloca.h>
#include <linux/types.h>
#include <linux/random.h>
#include <sys/ioctl.h>

static ssize_t
krnl_write(int fd, const void *buf, size_t count)
{
    struct rand_pool_info *rndpool;
    rndpool = alloca(sizeof(struct rand_pool_info) + count);

    rndpool->entropy_count = count * krnlop_bpb;
    rndpool->buf_size = count;
    memcpy(rndpool->buf, buf, count);

    if (ioctl(fd, RNDADDENTROPY, rndpool) == -1) {
        perror("ioctl");
        return -1;
    }
    return count;
}

estream_state_t *
estream_krnl_open(const char *path, int bpb)
{
    estream_state_t *stream_state = NULL;
    int fd;

    fd = open(path, O_RDWR | O_NOCTTY);
    if (fd < 0)
        return NULL;

    stream_state = calloc(1, sizeof(estream_state_t));
    if (stream_state == NULL) {
        close(fd);
        return NULL;
    }

    stream_state->fd = fd;

    stream_state->estream_read = read;
    stream_state->estream_write = krnl_write;

    krnlop_bpb = bpb;

    return stream_state;
}

#elif defined(EKEY_OS_OPENBSD) || defined(EKEY_OS_MIRBSD)

/* OpenBSD and MirBSD kernel entropy injection */

#include <sys/ioctl.h>
#include <dev/rndvar.h>
#include <dev/rndioctl.h>
#include <errno.h>

static ssize_t
krnl_write(int fd, const void *buf, size_t count)
{
    size_t ofs = 0;
    ssize_t n;
    unsigned int u;

    while (ofs < count) {
        n = write(fd, (const char *)buf + ofs, count - ofs);
        if (n == -1) {
            if (errno == EINTR)
                continue;
            perror("write");
            return -1;
        }
        ofs += n;
    }

    /* from MirOS: src/libexec/cprng/cprng.c,v 1.14 */
    u = count * krnlop_bpb;
    if (ioctl(fd, RNDADDTOENTCNT, &u) == -1) {
        perror("ioctl");
        return -1;
    }
    return count;
}

estream_state_t *
estream_krnl_open(const char *path, int bpb)
{
    estream_state_t *stream_state = NULL;
    int fd;

    fd = open(path, O_RDWR | O_NOCTTY);
    if (fd < 0)
        return NULL;

    stream_state = calloc(1, sizeof(estream_state_t));
    if (stream_state == NULL) {
        close(fd);
        return NULL;
    }

    stream_state->fd = fd;

    stream_state->estream_read = read;
    stream_state->estream_write = krnl_write;

    krnlop_bpb = bpb;

    return stream_state;
}

#else

/* Default implementation */

estream_state_t *
estream_krnl_open(const char *path, int bpb)
{
    estream_state_t *stream_state = NULL;

    krnlop_bpb = bpb;

    return stream_state;
}

#endif
