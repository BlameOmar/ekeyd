/* daemon/daemonise.c
 *
 * Daemonisation support code
 *
 * Copyright 2009-2011 Simtec Electronics
 *
 * For licence terms refer to the COPYING file.
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>

#include "daemonise.h"

/* exported interface documented in daemonise.h */
void
do_daemonise(const char *pidfilename, bool close_all_fd)
{
    int fd;
    int pid;
    FILE *pidfile;
    int pipefds[2];
    int fdlimit;
    int cfd;

    if (pipe(pipefds) == -1) {
	perror("pipe");
	exit(1);
    }

    switch ((pid = fork())) {
    case 0:
        /* Child */
        break; /* logic below */

    case -1:
        /* Error */
        perror("fork");
        exit(1);
        break;

    default:
        /* Parent */
        close(pipefds[1]);
        /* wait until child closes pipe */
        if (read(pipefds[0], &pid, 1) < 0)
            perror("read");
        exit(0);
    }

    /* Make stdin/out/err all be /dev/null */
    fd = open("/dev/null", O_RDWR);
    if (fd == -1) {
	perror("/dev/null open");
	exit(1);
    }

    dup2(fd, 0);
    dup2(fd, 1);
    dup2(fd, 2);
    close(fd);
    if (chdir("/") == -1) {
	perror("chdir");
	exit(1);
    }

    if (close_all_fd) {
        /* ensure any other fd are closed */
        fdlimit = sysconf(_SC_OPEN_MAX);
        for (cfd = 3; cfd < fdlimit ; cfd++) {
            if ((cfd != pipefds[0]) && 
                (cfd != pipefds[1])) {
                close(cfd);
            }
        }
    }

    /* Ensure our process group is better */
    setsid();
    setpgid(0, 0);

    /* Second fork() ensures we can never regain a controlling tty */
    switch ((pid = fork())) {
    case -1:
        /* Error */
        exit(1);

    case 0:
        /* Child */
        close(pipefds[0]);
        close(pipefds[1]); /* Signals outer-parent that we're done */
        break;

    default:
        /* Parent */
        if (pidfilename != NULL) {
            pidfile = fopen(pidfilename, "w");
            if (pidfile == NULL) {
                perror("open");
                kill(pid, SIGQUIT);
                exit(2);
            }
            fprintf(pidfile, "%d\n", pid);
            fclose(pidfile);
        }
        exit(0);
    }

}
