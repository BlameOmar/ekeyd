/* daemon/fds.c
 *
 * Entropy key file descriptor poll handling.
 *
 * Copyright 2009 Simtec Electronics
 *
 * For licence terms refer to the COPYING file.
 */

#include <poll.h>

#include "fds.h"

#define MAX_EKEY_POLLFD 256

typedef struct {
    ekeyfd_pollfunc_t func;
    void *pw;
} ekeyfd_pollent_t;

static struct pollfd ekeyfd_pollfd[MAX_EKEY_POLLFD];
static ekeyfd_pollent_t ekeyfd_funcs[MAX_EKEY_POLLFD];
static int ekeyd_lastfd = 0;

/* exported interface, documented in fds.h */
int
ekeyfd_add(int fd, short events, ekeyfd_pollfunc_t func, void *pw)
{
    ekeyfd_pollfd[ekeyd_lastfd].fd = fd;
    ekeyfd_pollfd[ekeyd_lastfd].events = events;
    ekeyfd_funcs[ekeyd_lastfd].func = func;
    ekeyfd_funcs[ekeyd_lastfd].pw = pw;
    ekeyd_lastfd++;
    return ekeyd_lastfd;
}

/* exported interface, documented in fds.h */
int
ekeyfd_rm(int fd)
{
    int fdloop;
    for (fdloop = 0 ; fdloop < ekeyd_lastfd ; fdloop++) {
        if (ekeyfd_pollfd[fdloop].fd == fd) {
            ekeyd_lastfd--;
            if (fdloop != ekeyd_lastfd) {
                ekeyfd_pollfd[fdloop].fd = ekeyfd_pollfd[ekeyd_lastfd].fd;
                ekeyfd_pollfd[fdloop].events = ekeyfd_pollfd[ekeyd_lastfd].events;
                ekeyfd_funcs[fdloop].func = ekeyfd_funcs[ekeyd_lastfd].func;
                ekeyfd_funcs[fdloop].pw = ekeyfd_funcs[ekeyd_lastfd].pw;
            }
        }
    }
    return 0;
}

/* exported interface, documented in fds.h */
void
ekeyfd_set_events(int fd, short events)
{
    int fdloop;
    for (fdloop = 0 ; fdloop < ekeyd_lastfd ; fdloop++) {
        if (ekeyfd_pollfd[fdloop].fd == fd) {
            ekeyfd_pollfd[fdloop].events |= events;
        }
    }
}

/* exported interface, documented in fds.h */
void
ekeyfd_clear_events(int fd, short events)
{
    int fdloop;
    for (fdloop = 0 ; fdloop < ekeyd_lastfd ; fdloop++) {
        if (ekeyfd_pollfd[fdloop].fd == fd) {
            ekeyfd_pollfd[fdloop].events &= ~events;
        }
    }
}


/* exported interface, documented in fds.h */
int
ekeyfd_poll(int timeout)
{
    int rdy;
    int fdcnt = 0;
    int fdloop = 0;

    if (ekeyd_lastfd == 0) {
        return 0;
    }

    rdy = poll(ekeyfd_pollfd, ekeyd_lastfd, timeout);

    if (rdy > 0) {
        for (fdloop = 0 ; ((fdcnt < rdy) && (fdloop < ekeyd_lastfd)) ; fdloop++) {
            if (ekeyfd_pollfd[fdloop].revents != 0) {
                fdcnt++;
                ekeyfd_funcs[fdloop].func(ekeyfd_pollfd[fdloop].fd,
                                          ekeyfd_pollfd[fdloop].revents,
                                          ekeyfd_funcs[fdloop].pw);
            }
        }


    }
    return rdy;
}
