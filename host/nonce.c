/* daemon/nonce.c
 *
 * Entropy key nonce generation
 *
 * Copyright 2009 Simtec Electronics
 *
 * For licence terms refer to the COPYING file.
 */

#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <syslog.h>

#include "nonce.h"

#if defined(EKEY_OS_OPENBSD) || defined(EKEY_OS_MIRBSD)

#include <stdlib.h>

/* exported interface documented in nonce.h */
bool
fill_nonce(uint8_t *buff, size_t count)
{
    arc4random_buf(buff, count);
    return true;
}

/* exported interface documented in nonce.h */
void
close_nonce(void)
{
}

#else

/** File descriptor for the urandom device. */
static int noncefd = -2;

/* exported interface documented in nonce.h */
bool
fill_nonce(uint8_t *buff, size_t count)
{
    int rd;
    bool status = true;

    if (noncefd < 0) {
        if (noncefd == -2) {
            noncefd = open("/dev/urandom", O_RDONLY);
        }
        if (noncefd < 0) {
            syslog(LOG_ERR, "Unable to prepare nonce");
            return false;
        }
    }

    rd = read(noncefd, buff, count);
    if (rd < 0) {
        syslog(LOG_ERR, "Error %s reading nonce data", strerror(errno));
        status = false;
    } else if (rd != count) {
        syslog(LOG_ERR, "Short read of nonce data (%d/%zd)", rd, count);
        status = false;
    }
    return status;
}

/* exported interface documented in nonce.h */
void
close_nonce(void)
{
    if (noncefd >=0 ) {
        close(noncefd);
    }
    noncefd = -2;
}
#endif
