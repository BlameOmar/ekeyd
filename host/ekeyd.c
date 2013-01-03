/* daemon/ekeyd.c
 *
 * Entropy key main daemon
 *
 * Copyright 2009-2011 Simtec Electronics
 *
 * For licence terms refer to the COPYING file.
 */

#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>
#include <poll.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <syslog.h>

#include "nonce.h"
#include "stream.h"
#include "krnlop.h"
#include "foldback.h"
#include "connection.h"
#include "fds.h"
#include "ekeyd.h"

#include "lstate.h"
#include "daemonise.h"

#ifndef EKEYD_VERSION_S
#error "tool verison not set"
#endif


#if defined(EKEY_OS_OPENBSD) || defined(EKEY_OS_MIRBSD)
#define EKEYD_DEV_RANDOM "/dev/srandom"
#else
#define EKEYD_DEV_RANDOM "/dev/random"
#endif

static bool lua_fd_ready = false;
static estream_state_t *output_stream;

void
lua_fd_activity(int fd, short events, void *pw)
{
    lua_fd_ready = true;
}

bool
lstate_cb_newfd(int fd)
{
    ekeyfd_add(fd, POLLIN, lua_fd_activity, NULL);
    return true;
}

void
lstate_cb_delfd(int fd)
{
    ekeyfd_rm(fd);
}

void
lstate_cb_writefd(int fd)
{
    ekeyfd_set_events(fd, POLLOUT);
}

void
lstate_cb_nowritefd(int fd)
{
    ekeyfd_clear_events(fd, POLLOUT);
}

void ekey_fd_activity(int fd, short events, void *pw)
{
    econ_state_t *econ = pw;
    econ_run(econ);
    if (econ_state(econ) == ESTATE_CLOSE) {
        ekeyfd_rm(econ_get_rd_fd(econ));
        estream_close(econ->key_stream);
        econ->key_stream = NULL;
        lstate_inform_about_key(econ);
    }
}

/** lua interface called to add an ekey */
OpaqueEkey *
add_ekey(const char *devpath, const char *serial)
{
    econ_state_t *econ;

    if (output_stream == NULL) {
        errno = EWOULDBLOCK;
        return NULL;
    }

    econ = econ_open(devpath, output_stream);

    if (econ == NULL)
        return NULL;

    if (serial != NULL)
        econ_setsnum(econ, serial);

    ekeyfd_add(econ_get_rd_fd(econ), POLLIN, ekey_fd_activity, econ);

    syslog(LOG_INFO, "Attached new entropy key %s", devpath);

    return econ;
}

void
kill_ekey(OpaqueEkey *ekey)
{
    char *serialnumber = econ_getsnum(ekey);

    if (serialnumber == NULL) {
        if (ekey->key_stream != NULL) {
            ekeyfd_rm(econ_get_rd_fd(ekey));
            syslog(LOG_INFO, "Detaching entropy key %s", ekey->key_stream->uri);
        } else {
            syslog(LOG_INFO, "Detaching Unknown Entropy key");
        }
    } else {
        if (ekey->key_stream != NULL) {
            ekeyfd_rm(econ_get_rd_fd(ekey));
            syslog(LOG_INFO, "Detaching entropy key %s (%s)",
                   ekey->key_stream->uri, serialnumber);
        } else {
            syslog(LOG_INFO, "Detaching entropy key %s", serialnumber);
        }
        free(serialnumber);
    }
    econ_close(ekey);
}


int query_ekey_status(OpaqueEkey *ekey)
{
    switch (econ_state(ekey)) {
    case ESTATE_CLOSE:
        return EKEY_STATUS_KEYCLOSED;

    case ESTATE_UNTRUSTED:
        return EKEY_STATUS_BADKEY;

    case ESTATE_SESSION:
    case ESTATE_SESSION_SENT:
        return EKEY_STATUS_GOODSERIAL;

    case ESTATE_KEYED_FIRST:
    case ESTATE_KEYED:
        return EKEY_STATUS_KEYED;

    default:
        return EKEY_STATUS_UNKNOWN;
    }
}

/* exported interface documented in ekeyd.h */
char *
retrieve_ekey_serial(OpaqueEkey *ekey)
{
    return econ_getsnum(ekey);
}

bool
open_file_output(const char *fname)
{
    if (output_stream != NULL) {
        errno = EADDRINUSE;
        return false;
    }

    output_stream = estream_open(fname);

    return (output_stream != NULL);
}

bool
open_kernel_output(int bits_per_byte)
{
    static char fname[] = EKEYD_DEV_RANDOM;
        
    if (output_stream != NULL) {
        errno = EADDRINUSE;
        return false;
    }

    output_stream = estream_krnl_open(fname, bits_per_byte);

    return (output_stream != NULL);
}

bool
open_foldback_output(void)
{
    if (output_stream != NULL) {
        errno = EADDRINUSE;
        return false;
    }

    output_stream = estream_foldback_open();

    return (output_stream != NULL);
}

static const char *usage=
    "Usage: %s [-f <configfile>] [-p <pidfile>] [-v] [-h]\n"
    "Entropy Key Daemon\n\n"
    "\t-f Read configuration from configfile\n"
    "\t-p Write pid to pidfile\n"
    "\t-v Display version and exit\n"
    "\t-h Display this help and exit\n\n";

int main(int argc, char **argv)
{
    int res;
    int opt;
    char *configfile;
    char *pidfile;

    configfile = strdup(CONFIGFILE);
    pidfile = strdup(PIDFILE);

    while ((opt = getopt(argc, argv, "vhf:p:")) != -1) {
        switch (opt) {
        case 'f':
            free(configfile);
            configfile = strdup(optarg);
            break;

        case 'p':
            free(pidfile);
            pidfile = strdup(optarg);
            break;

        case 'v':
            printf("%s: Version %s\n", argv[0], EKEYD_VERSION_S);
            return 0;

        case 'h':
        default:
            fprintf(stderr, usage, argv[0]);
            return 1;
        }
    }

    if (optind != argc) {
        fprintf(stderr, "Unexpected argument\n");
        fprintf(stderr, usage, argv[0]);
        return 1;
    }


    if (lstate_init() == false) {
        return 1;
    }

    if (!lstate_runconfig(configfile)) {
        /* Failed to run the configuration */
        return 1;
    }

    /* Everything is good, daemonise */
    if (lstate_request_daemonise())
        do_daemonise(pidfile, false);

    /* now we are a daemon, start system logging */
    openlog("ekeyd", LOG_ODELAY, LOG_DAEMON);

    syslog(LOG_INFO, "Starting Entropy Key Daemon");

    while (true) {
        res = ekeyfd_poll(-1);
        if (res == 0)
            break; /* no more fd open, finish */

        if (res < 0) {
            if ((errno == EINTR) || (errno == EWOULDBLOCK))
                continue; /* these errors are ok and the poll is retried */

            syslog(LOG_ERR, "Unhandled error in poll %s, exiting", strerror(errno));

            break;
        }

        if (lua_fd_ready) {
            lstate_controlbytes();
            lua_fd_ready = false;
        }
    }

    lstate_finalise();

    estream_close(output_stream);

    close_nonce();

    unlink(pidfile);

    free(configfile);
    free(pidfile);

    syslog(LOG_INFO, "Entropy Key Daemon Stopping");

    closelog();

    return 0;
}
