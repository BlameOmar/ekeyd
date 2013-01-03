/* daemon/egd-linux.c
 *
 * Stand-alone EGD => Linux pool entropy transfer agent.
 *
 * Copyright 2009 Simtec Electronics.
 * Copyrithg 2010 Collabora Ltd
 *
 * For licence terms refer to the COPYING file.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

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

#include <linux/types.h>
#include <linux/random.h>
#include <sys/ioctl.h>

#include "daemonise.h"

#define DEFAULT_HOST "127.0.0.1"
#define DEFAULT_PORT "8888"
#define DEFAULT_BLOCKS 4
#define DEFAULT_READTIMEOUT 10

static const char *pidfilename = NULL;
static int random_fd = 0;
static int egd_fd = -1;
static int shannons = 7;
static unsigned int retry_time = 0;
static char *host;
static char *port;

struct pollfd poll_fds[2] = {
    { .events = POLLOUT },
    { .events = POLLIN }
};

#define RND_POLLFD 0
#define EGD_POLLFD 1

static int bytes_waiting = 0; /* Number of bytes waiting on read*/

static void
usage(FILE *stream, char **argv)
{
    fprintf(stream,
            "%s: Usage:\n"                                              \
            "\n"                                                        \
            "%s [options]\n"                                            \
            "\n"                                                        \
            "Valid options are:\n"                                      \
            "\t-h\t\tDisplay this help.\n"                                \
            "\t-v\t\tDisplay the version of this program.\n"              \
            "\t-H <host>\tSet the host IP address to connect to.\n"     \
            "\t-p <postnr>\tSet the port number to connect to.\n"       \
            "\t-b <blocks>\tThe number of 1024 bit blocks to request in\n" \
            "\t\t\t each transaction. (Default %d)\n"           \
            "\t-s <S>\t\tSet the number of shannons per byte to S.\n"     \
            "\t-D <pidf>\tDaemonise, writing PID to pidf.\n"            \
            "\t-r <time>\tRetry connection every time seconds.\n"       \
            "\t-t <time>\tTimeout on read operations from server before\n"\
            "\t\t\t connection retry. (Default %d)\n",
            argv[0], argv[0], DEFAULT_BLOCKS, DEFAULT_READTIMEOUT);
}

static bool
try_open_random(void)
{
    random_fd = open("/dev/random", O_RDWR);
    poll_fds[RND_POLLFD].fd = random_fd;
    return (random_fd != -1);
}

static void
set_egd_blocking(bool block)
{
    int flags;
    if ((flags = fcntl(egd_fd, F_GETFL)) == -1)
        perror("fcntl");
    flags &= ~(O_NONBLOCK);
    if (block == false)
        flags |= O_NONBLOCK;
    if (fcntl(egd_fd, F_SETFL, flags) == -1)
        perror("fcntl");
}

/** attempt to connect to an EGD server.
 *
 * Create a socket and connect it to an EGD server.
 *
 * @param rtime The time between retry attempts, 0 for no retrys.
 * @return true if the connection was sucessful, false if it was not.
 */
static bool
egd_connect(unsigned int rtime)
{
    int con_res = -1;
    int keepalive = 1;
    socklen_t keepalive_len = sizeof(keepalive);
    struct addrinfo *addrs, *addrs_head;
    struct addrinfo hints;
    int r;

    /* Look up port and address */
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_ADDRCONFIG | AI_V4MAPPED;

    if ((r = getaddrinfo(host, port, &hints, &addrs)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(r));
        return false;
    }

    addrs_head = addrs;
    do {
        while (addrs) {
            /* close fd if we already have one open */
            if (egd_fd >= 0) {
                close(egd_fd);
            }
            
            /* create socket */
            egd_fd = socket(addrs->ai_family, addrs->ai_socktype, addrs->ai_protocol);
            if (egd_fd == -1) {
                perror("socket");
            return false;
            }

            /* Set the keepalive option active */
            if (setsockopt(egd_fd, SOL_SOCKET, SO_KEEPALIVE, &keepalive, keepalive_len) < 0) {
                perror("setsockopt");
            }

            con_res = connect(egd_fd, addrs->ai_addr, addrs->ai_addrlen);
            if (con_res == 0) {
                rtime = 0; /* get out of outer loop */
                break;
            }
            addrs = addrs->ai_next;
        }
        if (rtime != 0) {
            sleep(rtime);
            fprintf(stderr, "Retrying connection.\n");
        }
        addrs = addrs_head;
    } while (rtime != 0);
    freeaddrinfo(addrs_head);

    /* Set the keepalive option active */
    if (setsockopt(egd_fd, SOL_SOCKET, SO_KEEPALIVE, &keepalive, keepalive_len) < 0) {
        perror("setsockopt");
    }

    if (con_res != 0) {
        perror("connect");
        close(egd_fd);
        egd_fd = -1;
        return false;
    }

    poll_fds[EGD_POLLFD].fd = egd_fd;

    /* ensure the random pool state is reset */
    bytes_waiting = 0;
    poll_fds[RND_POLLFD].events = POLLOUT;

    return true;
}


static void
issue_read_for_random(int nblocks)
{
    set_egd_blocking(true);
    bytes_waiting += nblocks * 128;
    while (nblocks--)
        if (write(egd_fd, (const void *)"\x02\x80", 2) != 2) {
            perror("write");
            bytes_waiting = 0;
            return;
        }
    set_egd_blocking(false);
}

static void
punt_entropy(void)
{
    static unsigned char buffer[128];
    struct rand_pool_info *rndpool;
    int bread;

    bread = read(egd_fd, buffer, 128);
    if (bread < 0) {
        perror("read");
        exit(1);
    }

    if (bread == 0) {
        /* EOF on the EGD socket is never good, reset the world */
        fprintf(stderr, "EGD Client Daemon reconnecting to server\n");
        syslog(LOG_ERR, "Reconnecting to EGD server");
        if (egd_connect(retry_time) == false) {
            exit(1);
        }
        return;
    }

    rndpool = alloca(sizeof(struct rand_pool_info) + bread);

    rndpool->entropy_count = bread * shannons;
    rndpool->buf_size = bread;
    memcpy(rndpool->buf, buffer, bread);

    if (ioctl(random_fd, RNDADDENTROPY, rndpool) == -1) {
        perror("ioctl");
        exit(1);
    }

    bytes_waiting -= bread;
}

/** core poll loop.
 */
int 
main_poll(int nblocks, int read_timeout)
{
    int poll_ret = 0;
    static int poll_timeout; /* time poll waits for input */

    /* Main poll for activity */
    while (poll_ret != -1) {
        if (bytes_waiting == 0) {
            /* not waiting on data from server*/
            poll_timeout = -1;
 
            /* re-enable polling for kernel input */
            poll_fds[RND_POLLFD].events = POLLOUT;
        } else {
            /* poll timeouts in miliseconds */
            poll_timeout = read_timeout * 1000; 
        }

        poll_ret = poll(poll_fds, 2, poll_timeout);

        if ((poll_ret == -1) && (errno != EINTR)) {
            break;
        }

        if (poll_ret == 0) {
            /* poll timed out. Indicates no server reply to request */
            syslog(LOG_INFO, "EGD server poll timeout, reconnecting");
            egd_connect(retry_time);
            continue;
        }

        if (poll_fds[RND_POLLFD].revents & (POLLERR | POLLHUP | POLLNVAL)) {
            syslog(LOG_INFO, "Linux random device poll error");
            break;
        }

        if (poll_fds[EGD_POLLFD].revents & (POLLERR | POLLHUP | POLLNVAL)) {
            if (retry_time != 0) {
                syslog(LOG_INFO, "EGD server poll error, reconnecting");
                sleep(retry_time);
                egd_connect(retry_time);
                continue;
            } else {
                syslog(LOG_ERR, "EGD server poll error");
                break;
            }
        }

        if (poll_fds[RND_POLLFD].revents & POLLOUT) {
            poll_fds[RND_POLLFD].events = 0;
            issue_read_for_random(nblocks);
        }

        if (poll_fds[EGD_POLLFD].revents & POLLIN) {
            punt_entropy();
        }

    }

    return 0;
}

int
main(int argc, char **argv)
{
    int opt;
    bool daemonise = false;
    int blocks = DEFAULT_BLOCKS;
    int read_timeout = DEFAULT_READTIMEOUT;

    /* setup default host address */
    host = DEFAULT_HOST;
    port = DEFAULT_PORT;

    /* command line option parsing */
    while ((opt = getopt(argc, argv, "vhH:p:D:b:S:r:t:")) != -1) {
        switch (opt) {
        case 'v':
            printf("%s: Version %s\n", argv[0], EKEYD_VERSION_S);
            return 0;
            break;

        case 'h':
            usage(stdout, argv);
            return 0;
            break;

        case 'H':
            host = strdup(optarg);
            break;

        case 'p':
            port = strdup(optarg);
            break;

        case 'D':
            daemonise = true;
            pidfilename = optarg;
            break;

        case 'b':
            blocks = atoi(optarg);
            if (blocks < 1 || blocks > 4) {
                fprintf(stderr, "Number of blocks (%d) is out of range, must be between 1 and 4\n", blocks);
                return 1;
            }
            break;

        case 't':
            read_timeout = atoi(optarg);
            if (read_timeout < 1 || read_timeout > 600) {
                fprintf(stderr, "Read Timeout (%d) is out of range, must be between 1 and 600\n", blocks);
                return 1;
            }
            break;

        case 'S':
            shannons = atoi(optarg);
            if (shannons < 1 || shannons > 8) {
                fprintf(stderr, "Claiming %d shannons per byte is pointless.\n", shannons);
                return 1;
            }
            break;

        case 'r':
            retry_time = strtoul(optarg, NULL, 10);
            break;

        default:
            usage(stderr, argv);
            return 1;
            break;
        }
    }

    if (try_open_random() == false) {
        fprintf(stderr, "Unable to open /dev/random for writing\n");
        return 1;
    }

    /* try initial connect to EGD server */
    egd_connect(0);
    if ((retry_time == 0) && (egd_fd == -1)) {
        fprintf(stderr, "Unable to connect to EGD server.\n");
        return 1;
    }

    if (daemonise)
        do_daemonise(pidfilename, false);

    /* now we are a daemon, start system logging */
    openlog("ekey-egd-linux", LOG_ODELAY, LOG_DAEMON);

    syslog(LOG_INFO, "Starting EGD Client Daemon");

    /* retry EGD server connection if necessary */
    if (egd_fd == -1)
        egd_connect(retry_time);

    /* ignore pipe signal */
    signal(SIGPIPE, SIG_IGN);

    main_poll(blocks, read_timeout);

    if (egd_fd != -1)
        close(egd_fd);

    close(random_fd);

    syslog(LOG_INFO, "EGD Client Daemon Stopping");

    return 0;
}
