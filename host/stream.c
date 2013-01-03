/* daemon/stream.c
 *
 * Entropy key daemon stream handling
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
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <syslog.h>

#include "stream.h"

/*  POSIX.1g requires <sys/un.h> to define a SUN_LEN macro for determining the
 *  length of sockaddr_un. Of course its not available everywhere. This is the
 *  definition from the specification when its not defined.
 */
#ifndef SUN_LEN
#define SUN_LEN(su) (sizeof(*(su)) - sizeof((su)->sun_path) + strlen((su)->sun_path))
#endif

/* exported function documented in stream.h */
estream_state_t *
estream_open(const char *uri)
{
    estream_state_t *stream_state;
    int fd;
    struct stat sbuf;
    struct termios settings;

    /* Attempt to stat the file */
    if (stat(uri, &sbuf) == -1) {
        return NULL;
    }

    if (S_ISSOCK(sbuf.st_mode)) {
        /* Open the file as a socket */
        struct sockaddr_un saddr;
        size_t namelen = strlen(uri) + 1;

        if (namelen > sizeof(saddr.sun_path)) {
            fprintf(stderr, "Device name (%s) too long (%u, max. %u)\n", uri,
                    (unsigned int)namelen, (unsigned int)sizeof(saddr.sun_path));
            return NULL;
        }
        fd = socket(PF_UNIX, SOCK_STREAM, 0);
        if (fd != -1) {
            saddr.sun_family = AF_UNIX;
            /* this is effectively a strcpy, but strcpy produces warnings
             * on BSD; the check above validates this operation
             */
            memcpy(saddr.sun_path, uri, namelen);
#ifdef BSD44SOCKETS
            saddr.sun_len = strlen(uri)
#if defined(EKEY_OS_OPENBSD) || defined(EKEY_OS_MIRBSD)
                            + 1
#endif
                            ;
#endif

            if (connect(fd, (struct sockaddr *)&saddr, SUN_LEN(&saddr)) == -1) {
                perror("connect");
                close(fd);
                fd = -1;
            }
        }
    } else if (S_ISCHR(sbuf.st_mode)) {
        /* Open the file as a character device/tty */
        fd = open(uri, O_RDWR | O_NOCTTY);
        if ((fd != -1) && (isatty(fd))) {
            if (tcgetattr(fd, &settings) == 0 ) {
                settings.c_cflag &= ~(CSIZE | CSTOPB | PARENB | CLOCAL |
                                      CREAD | PARODD | CRTSCTS);
                settings.c_iflag &= ~(BRKINT | IGNPAR | PARMRK | INPCK |
                                      ISTRIP | INLCR | IGNCR | ICRNL | IXON |
                                      IXOFF  | IXANY | IMAXBEL);
                settings.c_iflag |= IGNBRK;
                settings.c_oflag &= ~(OPOST | OCRNL | ONOCR | ONLRET);
                settings.c_lflag &= ~(ISIG | ICANON | IEXTEN | ECHO |
                                      ECHOE | ECHOK | ECHONL | NOFLSH |
                                      TOSTOP | ECHOPRT | ECHOCTL | ECHOKE);
                settings.c_cflag |= CS8 | HUPCL | CREAD | CLOCAL;
#ifdef EKEY_FULL_TERMIOS
                settings.c_cflag &= ~(CBAUD);
		settings.c_iflag &= ~(IUTF8 | IUCLC);
                settings.c_oflag &= ~(OFILL | OFDEL | NLDLY | CRDLY | TABDLY |
                                      BSDLY | VTDLY | FFDLY | OLCUC );
                settings.c_oflag |= NL0 | CR0 | TAB0 | BS0 | VT0 | FF0;
                settings.c_lflag &= ~(XCASE);
#endif
                settings.c_cflag |= B115200;
                if (tcsetattr(fd, TCSANOW, &settings) < 0) {
                    /* Failed to set TTY attributes? */
                    syslog(LOG_INFO, "Failed to set TTY attributes on %s", uri);
                }
            }
            tcflush(fd, TCIOFLUSH);
        }
    } else {
        /* open the file as a plain file and seek to the end */
        fd = open(uri, O_RDWR | O_NOCTTY);
        lseek(fd, 0, SEEK_END);
    }

    if (fd < 0)
        return NULL;

    stream_state = calloc(1, sizeof(estream_state_t));
    if (stream_state == NULL)
        return NULL;

    stream_state->uri = strdup(uri);
    stream_state->fd = fd;
    stream_state->estream_read = read;
    stream_state->estream_write = write;

    return stream_state;
}

/* exported function documented in stream.h */
ssize_t
estream_read(estream_state_t *state, void *buf, size_t count)
{
    ssize_t rd = state->estream_read(state->fd, buf, count);
    if (rd > 0)
        state->bytes_read += rd;
    return rd;
}

/* exported function documented in stream.h */
ssize_t
estream_write(estream_state_t *state, void *buf, size_t count)
{
    ssize_t wr = state->estream_write(state->fd, buf, count);
    if (wr > 0) {
        state->bytes_written += wr;
    } else {
        syslog(LOG_ERR, "Write error %zd on %s ", wr, state->uri);
    }
    return wr;
}

/* exported function documented in stream.h */
int
estream_close(estream_state_t *state)
{
    close(state->fd);
    free(state->uri);
    free(state);
    return 0;
}
