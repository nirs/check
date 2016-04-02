/*
 * Copyright 2016 Nir Soffer <nsoffer@redhat.com>
 *
 * This copyrighted material is made available to anyone wishing to use,
 * modify, copy, or redistribute it subject to the terms and conditions
 * of the GNU General Public License v2 or (at your option) any later version.
 */

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>  /* STD*_FILNO */

#include <ev.h>

#include "lineio.h"
#include "check.h"
#include "log.h"

#define MAX_CMD_ARGS 3
#define MAX_PATHS 128

int debug_mode;

static int
set_nonblocking(int fd)
{
    int flags;
    int err;

    flags = fcntl(fd, F_GETFL);
    if (flags == -1)
        return -1;

    err = fcntl(fd, F_SETFL, flags| O_NONBLOCK);
    if (err == -1)
        return -1;

    return 0;
}

static int
split(char *cmd, char *args[], int n)
{
    char *next = cmd;
    int i;
    for (i = 0; i < n && next != NULL; i++) {
        args[i] = strsep(&next, " ");
        log_debug("args[%d]='%s'", i, args[i]);
    }
    return i;
}

static void
line_received(char *line)
{
    struct ev_loop *loop = EV_DEFAULT;
    char *argv[MAX_CMD_ARGS] = {0};

    split(line, argv, MAX_CMD_ARGS);

    char *cmd = argv[0];
    if (cmd == NULL) {
        log_error("empty command");
        /* TODO: send error to caller? */
        return;
    }

    if (strcmp(cmd, "start") == 0) {
        char *path = argv[1];
        if (path == NULL) {
            log_error("path required");
            /* TODO: send error to caller? */
            return;
        }

        char *interval_string = argv[2];
        if (interval_string == NULL) {
            log_error("interval required");
            /* TODO: send error to caller? */
            return;
        }

        char *endp;
        long interval = strtol(interval_string, &endp, 10);
        if (interval_string == endp || *endp != 0) {
            /* Not converted, or trailing characters */
            log_error("invalid interval: '%s'", interval_string);
            /* TODO: send error to caller? */
            return;
        }
        if (interval < 0 || interval == INT_MAX) {
            log_error("interval out of range: '%s'", interval_string);
            /* TODO: send error to caller? */
            return;
        }

        check_start(EV_A_ path, interval);
    } else if (strcmp(cmd, "stop") == 0) {
        char *path = argv[1];
        if (path == NULL) {
            log_error("path required");
            /* TODO: send error to caller? */
            return;
        }

        check_stop(EV_A_ path);
    } else {
        log_error("invalid command: '%s'", cmd);
        /* TODO: send error to caller? */
        return;
    }
}

static void
check_complete(char *path, int error, ev_tstamp delay)
{
    /* TODO: send response to parent */
    if (error)
        log_error("check complete: path=%s error=%d", path, error);
    else
        log_info("check complete: path=%s error=%d delay=%.6f",
                 path, error, delay);
}

int main(int argc, char *argv[])
{
    int err;
    struct lineio lineio;
    struct ev_loop *loop = EV_DEFAULT;

    if (argc > 1)
        debug_mode = strcmp(argv[1], "-d") == 0;

    log_info("started");

    err = check_setup(EV_A_ MAX_PATHS, check_complete);
    if (err != 0) {
        log_error("check_setup: %s", strerror(errno));
        return 1;
    }

    err = set_nonblocking(STDIN_FILENO);
    if (err) {
        log_error("Cannot set fd %d nonblocking: %s",
                  STDIN_FILENO, strerror(errno));
        return 1;
    }

    lineio_init(&lineio, STDIN_FILENO, line_received);

    ev_io_init(&lineio.watcher, lineio_cb, lineio.fd, EV_READ);
    ev_io_start(EV_A_ &lineio.watcher);

    ev_run(EV_A_ 0);

    log_info("terminated");

    err = check_teardown(EV_A);
    if (err != 0)
        log_error("check_teardown: %s", strerror(errno));

    return 0;
}
