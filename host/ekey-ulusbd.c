/* daemon/ekey-ulusbd.c
 *
 * Entropy key userspace USB peuso CDC serial server.
 *
 * Copyright 2009 Simtec Electronics
 *
 * For licence terms refer to the COPYING file.
 */

#include <stdio.h>
#include <stdbool.h>
#include <usb.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <signal.h>
#include <stdint.h>
#include <poll.h>
#include <syslog.h>

#include "daemonise.h"

/*  POSIX.1g requires <sys/un.h> to define a SUN_LEN macro for determining the
 *  length of sockaddr_un. Of course its not available everywhere. This is the
 *  definition from the specification when its not defined.
 */
#ifndef SUN_LEN
#define SUN_LEN(su) (sizeof(*(su)) - sizeof((su)->sun_path) + strlen((su)->sun_path))
#endif

static char namebuffer[1024];
static struct usb_dev_handle *devh;
static int uds_accept_fd = -1;
static int uds_client_fd = -1;
static char *devpath = NULL;
static bool daemonise = false;
static char *pidfilename = NULL;

#define EKEY_IFACE 1
#define EKEY_EP_TOHOST 1
#define EKEY_EP_TODEVICE 3

static void
usage(FILE *stream, char *argv0)
{
    fprintf(stream,
            "Usage: %s -b<bus> -d<device> -p<socketpath>, [-Ppidfilename] [-D]\n" \
            "\n"                                                  \
            "Options:\n"                                          \
            "\t-D\tDaemonise (fork into background)\n"            \
            "\t-v\tDisplay version and exit\n"                    \
            "\t-h\tDisplay this information and exit\n"           \
            , argv0);
}

static bool
find_usb_device(char *busmatch, char *devmatch)
{
    struct usb_bus *bus;
    struct usb_device *dev, *devfound = NULL;
    int r;

    usb_init();
    usb_find_busses();
    usb_find_devices();

    for (bus = usb_get_busses(); bus != NULL; bus = bus->next) {
        if (strcmp(bus->dirname, busmatch) != 0)
            continue;

        for (dev = bus->devices; dev != NULL; dev = dev->next) {
            if (strcmp(dev->filename, devmatch) != 0)
                continue;

            if (dev->descriptor.idVendor != 0x20df ||
                dev->descriptor.idProduct != 0x0001) {

                /* Found the device, but it's not an Entropy Key */
                fprintf(stderr, 
                        "Device at %s/%s is not an Entropy Key\n", 
                        busmatch, devmatch);
                return false;
            }
            devfound = dev;
        }
    }
    if (devfound == NULL) {
        fprintf(stderr, "Unable to find device at %s/%s\n", busmatch, devmatch);
        return false;
    }

    devh = usb_open(devfound);
    if (devh == NULL) {
        fprintf(stderr, 
                "Unable to open Entropy Key at %s/%s\n", busmatch, devmatch);
        return false;
    }

#if LIBUSB_HAS_DETACH_KERNEL_DRIVER_NP
    if ((r = usb_detach_kernel_driver_np(devh, EKEY_IFACE)) != 0) {
        if (r != -ENODATA) {
            fprintf(stderr, 
                    "Unable to detach Entropy Key at %s/%s from kernel\n", 
                    busmatch, devmatch);
            usb_close(devh);
            return false;
        }
    }
#endif

    if ((r = usb_claim_interface(devh, EKEY_IFACE)) != 0) {
        fprintf(stderr, 
                "Unable to claim Entropy Key interface at %s/%s\n", 
                busmatch, devmatch);
        usb_close(devh);
        return false;
    }

    /* Opened, and interface claimed, we found it, return true */
    return true;
}

static bool
prepare_uds(void)
{
    struct sockaddr_un sa;
    size_t namelen = strlen(devpath) + 1;
    unlink(devpath);

    uds_accept_fd = socket(PF_UNIX, SOCK_STREAM, 0);
    if (uds_accept_fd == -1) {
        perror("socket");
        return false;
    }

    sa.sun_family = AF_UNIX;
    if (namelen > sizeof(sa.sun_path)) {
        fprintf(stderr, "Device name (%s) too long (%u, max. %u)\n", devpath,
                (unsigned int)namelen, (unsigned int)sizeof(sa.sun_path));
        /* same return code as config/cmdline syntax error */
        exit(1);
    }
    /* this is effectively a strcpy, but strcpy produces warnings on BSD */
    memcpy(sa.sun_path, devpath, namelen);

#ifdef BSD44SOCKETS
    sa.sun_len = strlen(devpath)
#if defined(EKEY_OS_OPENBSD) || defined(EKEY_OS_MIRBSD)
                 + 1
#endif
                 ;
#endif

    if (bind(uds_accept_fd, (struct sockaddr *)&sa, SUN_LEN(&sa)) == -1) {
        perror("bind");
        close(uds_accept_fd);
        return false;
    }

    if (listen(uds_accept_fd, 5) == -1) {
        perror("listen");
        close(uds_accept_fd);
        return false;
    }

    return true;
}

/* Reader thread reads from Entropy Key and writes to uds */
static void *
reader_thread_func(void *arg)
{
    static char buffer[1024];
    char *bptr;
    int r, r2;
    int fd = (int)((intptr_t)arg);
    int saved_errno;

    /* Drain input buffer */
    while (usb_bulk_read(devh, EKEY_EP_TOHOST, buffer, 1024, 500) > 0);

    do {
        if ((r = usb_bulk_read(devh, EKEY_EP_TOHOST, buffer, 1024, 0)) < 1) {
	    saved_errno = errno;
            switch (saved_errno) {
            case EAGAIN:
                /* Apparently we can get EAGAIN from a still-connected device
                 * which we chose to read from without a timeout.
                 */
                continue;

            default:
                /* Anything else, log and exit. */
                if (daemonise) {
                    syslog(LOG_ERR, "USB device vanished during read?");
                    syslog(LOG_ERR, "ret=%d Errno=%d (%s)", r, saved_errno, strerror(saved_errno));
                } else {
                    fprintf(stderr, "USB device has vanished?\n");
                    fprintf(stderr, "ret=%d Errno=%d (%s)\n", r, saved_errno, strerror(saved_errno));
                }
                close(fd);
                return NULL;
            }
        }
        bptr = buffer;
        if (!daemonise) {
            printf("<<< %.*s", r, buffer);
        }

        while ((uds_client_fd != -1) && (r > 0)) {
            r2 = write(uds_client_fd, bptr, r);
            if (r2 < 1) {
                if (!daemonise) {
                    fprintf(stderr, 
                            "No longer able to write to client on %d\n", 
                            uds_client_fd);
                    break;
                }
            } else {
                /* Client ate some bytes */
                bptr += r2;
                r -= r2;
            }
        }
    } while(1);

    return NULL;
}

static pthread_t
start_reader(int fd)
{
    pthread_t blah;
    pthread_create(&blah, NULL, reader_thread_func, (void *)(intptr_t)fd);
    return blah;
}

static void
unlinksock(void)
{
    unlink(devpath);
    unlink(pidfilename);
}

static void
poll_loop(int readerpipe)
{
    static char buffer[1024];
    int r;
    struct pollfd pfd[3];
    int tempclient;

    pfd[0].fd = uds_accept_fd;
    pfd[1].fd = readerpipe;
    pfd[0].events = POLLIN | POLLERR;
    pfd[1].events = POLLIN | POLLERR;
    /* pfd[2]'s FD will be whatever we accept as uds_client_fd later */
    pfd[2].events = POLLIN | POLLERR;
    pfd[2].revents = 0;

    while (poll(pfd, uds_client_fd == -1 ? 2 : 3, -1) >= 0) {
        if (pfd[0].revents & POLLERR) {
            if (daemonise) {
                syslog(LOG_ERR, "Error on UNIX domain socket");
            } else {
                fprintf(stderr, "Error on UNIX domain socket\n");
            }
            break;
        }
        if (pfd[1].revents) {
            if (daemonise) {
                syslog(LOG_ERR, "Events on pipe to reader thread");
            } else {
                fprintf(stderr, "Events on pipe to reader thread\n");
            }
            break;
        }

        if (pfd[0].revents & POLLIN) {
            tempclient = accept(uds_accept_fd, NULL, 0);
            if (uds_client_fd == -1) {
                uds_client_fd = tempclient;
                if (!daemonise) {
                    printf("New client on %d\n", uds_client_fd);
                }
                pfd[2].fd = uds_client_fd;
                pfd[2].revents = 0;
            } else {
                close(tempclient);
            }
        }

        if (uds_client_fd != -1 && pfd[2].revents & POLLERR) {
            if (!daemonise) {
                printf("Client gone away from %d\n", uds_client_fd);
            }
            close(uds_client_fd);
            uds_client_fd = -1;
        }

        if (uds_client_fd != -1 && pfd[2].revents & POLLIN) {
            r = read(uds_client_fd, buffer, 1024);
            if (r < 1) {
                /* Client vanished? */
                if (!daemonise) {
                    printf("Client on %d vanished?\n", uds_client_fd);
                }
                close(uds_client_fd);
                uds_client_fd = -1;
            } else {
                if (usb_bulk_write(devh, EKEY_EP_TODEVICE, buffer, r, 0) <= 0) {
                    /* USB device went away while we were writing */
                    if (daemonise) {
                        syslog(LOG_ERR, "USB device has vanised during write. Exiting");
                    } else {
                        fprintf(stderr, "USB device has vanished?\n");
                    }
                    exit(6);
                }
            }
        }
    }
}

int
main(int argc, char **argv)
{
    int opt;
    char *busmatch = NULL, *devmatch = NULL;
    int readerpipe[2];
    pthread_t readerthread;

    while ((opt = getopt(argc, argv, "vhb:d:p:DP:")) != -1) {
        switch(opt) {
        case 'v':
            printf("Simtec Entropy Key, Userland USB Daemon. Version 1.1\n");
            return 0;
        case 'h':
            printf("Simtec Entropy Key, Userland USB Daemon. Version 1.1\n");
            usage(stdout, argv[0]);
            return 0;
        case 'b':
            busmatch = optarg;
            break;
        case 'd':
            devmatch = optarg;
            break;
        case 'p':
            devpath = optarg;
            break;
        case 'D':
            daemonise = true;
            break;
        case 'P':
            pidfilename = optarg;
            break;
        default:
            fprintf(stderr, "Unknown option: %c\n", opt);
            usage(stderr, argv[0]);
            return 1;
        }
    }

    if (busmatch == NULL || devmatch == NULL || devpath == NULL) {
        fprintf(stderr, "Bus, device and socket path must all be provided.\n");
        return 1;
    }

    if (daemonise)
        do_daemonise(pidfilename, true);

    if (daemonise) {
        snprintf(namebuffer, 1024, "ekey-ulusbd(%s)", strrchr(devpath, '/') + 1);
        openlog(namebuffer, LOG_ODELAY | LOG_PID, LOG_DAEMON);
    } else {
        printf("Scanning for USB device %s/%s\n", busmatch, devmatch);
    }

    if (find_usb_device(busmatch, devmatch) == false) {
        if (daemonise) {
            syslog(LOG_ERR, 
                   "Unable to locate Simtec Entropy Key at %s/%s", 
                   busmatch, devmatch);
        } else {
            fprintf(stderr, 
                    "Error locating Simtec Entropy Key at %s/%s\n", 
                    busmatch, devmatch);
        }
        return 2;
    }

    if (!daemonise) {
        printf("Entropy Key at %s/%s opened and claimed\n", busmatch, devmatch);
    }

    if (prepare_uds() == false) {
        if (daemonise) {
            syslog(LOG_ERR, 
                   "Unable to prepare UNIX domain socket at %s", devpath);
        } else {
            fprintf(stderr, 
                    "Unable to prepare UNIX domain socket at %s\n", devpath);
        }
        return 3;
    }

    signal(SIGPIPE, SIG_IGN);

    atexit(unlinksock);

    if (pipe(readerpipe) < 0) {
        perror("pipe");
        return 8;
    }

    readerthread = start_reader(readerpipe[1]);
    if (daemonise) {
        syslog(LOG_INFO, "Polling");
    } else {
        fprintf(stdout, "Polling\n");
    }
    poll_loop(readerpipe[0]);
    if (daemonise) {
        syslog(LOG_INFO, "Closing down");
    } else {
        fprintf(stdout, "Closing down\n");
    }
    pthread_kill(readerthread, 9);
    pthread_join(readerthread, NULL);

    return 0;
}

/*
 * Local Variables:
 * c-basic-offset:4
 * End:
 */
