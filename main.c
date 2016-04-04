/*
 * Copyright 2016 Nir Soffer <nsoffer@redhat.com>
 *
 * This copyrighted material is made available to anyone wishing to use,
 * modify, copy, or redistribute it subject to the terms and conditions
 * of the GNU General Public License v2 or (at your option) any later version.
 */

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>  /* STD*_FILNO */

#include <ev.h>

#include "check.h"
#include "reader.h"
#include "log.h"

#define MAX_CMD_ARGS 3
#define MAX_PATHS 128

static void send_event(char *event, char *path, int error, ev_tstamp delay);

int debug_mode;

static int set_nonblocking(int fd)
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

static int split(char *cmd, char *args[], int n)
{
    char *next = cmd;
    int i;
    for (i = 0; i < n && next != NULL; i++) {
        args[i] = strsep(&next, " ");
        log_debug("args[%d]='%s'", i, args[i]);
    }
    return i;
}

void reader_cb(EV_P_ ev_io *w, int revents)
{
    struct reader *r = (struct reader *)w;
    int nread;

    nread = reader_read(r);
    if (nread == -1) {
        perror("ERROR reader_read");
        ev_break(EV_A_ EVBREAK_ALL);
        return;
    }

    if (nread)
        reader_process(r);
}

static void line_received(char *line)
{
    struct ev_loop *loop = EV_DEFAULT;
    char *argv[MAX_CMD_ARGS] = {0};

    split(line, argv, MAX_CMD_ARGS);

    char *cmd = argv[0];
    if (cmd == NULL) {
        log_error("empty command");
        send_event("-", "-", EINVAL, 0);
        return;
    }

    if (strcmp(cmd, "start") == 0) {
        char *path = argv[1];
        if (path == NULL) {
            log_error("path required");
            send_event(cmd, "-", EINVAL, 0);
            return;
        }

        char *interval_string = argv[2];
        if (interval_string == NULL) {
            log_error("interval required");
            send_event(cmd, path, EINVAL, 0);
            return;
        }

        char *endp;
        long interval = strtol(interval_string, &endp, 10);
        if (interval_string == endp || *endp != 0) {
            /* Not converted, or trailing characters */
            log_error("invalid interval: '%s'", interval_string);
            send_event(cmd, path, EINVAL, 0);
            return;
        }
        if (interval < 0 || interval == INT_MAX) {
            log_error("interval out of range: '%s'", interval_string);
            send_event(cmd, path, EINVAL, 0);
            return;
        }

        check_start(EV_A_ path, interval);
    } else if (strcmp(cmd, "stop") == 0) {
        char *path = argv[1];
        if (path == NULL) {
            log_error("path required");
            send_event(cmd, "-", EINVAL, 0);
            return;
        }

        check_stop(EV_A_ path);
    } else {
        log_error("invalid command: '%s'", cmd);
        send_event(cmd, "-", EINVAL, 0);
        return;
    }
}

static void send_event(char *event, char *path, int error, ev_tstamp delay)
{
    log_debug("check: event=%s path=%s error=%d delay=%f",
              event, path, error, delay);

    char buf[1100];
    ssize_t len;

    if (strcmp(event, "check") == 0 && error == 0)
        len = snprintf(buf, sizeof(buf), "%s %s %d %.6f\n",
                       event, path, error, delay);
    else
        len = snprintf(buf, sizeof(buf), "%s %s %d -\n",
                       event, path, error);

    assert(len > 0 && len < (int)sizeof(buf) && "event truncated");

    ssize_t nwritten;
    do {
        nwritten = write(STDOUT_FILENO, buf, len);
    } while (nwritten == -1 && errno == EINTR);

    /* We relay on pipe buffer (64KiB on Linux) if write fail or we get partial
     * write, it means the parent process is not listening, so we rather die.
     * The parent may restart us and try to pay more attention to pipe. */

    if (nwritten == -1) {
        log_error("error writing to fd %d: %s", STDOUT_FILENO, strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (nwritten < len) {
        log_error("parent is not listening, terminating");
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char *argv[])
{
    int err;
    struct reader reader;
    struct ev_loop *loop = EV_DEFAULT;

    if (argc > 1)
        debug_mode = strcmp(argv[1], "-d") == 0;

    log_info("started");

    err = check_setup(EV_A_ MAX_PATHS, send_event);
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

    err = set_nonblocking(STDOUT_FILENO);
    if (err) {
        log_error("Cannot set fd %d nonblocking: %s",
                  STDOUT_FILENO, strerror(errno));
        return 1;
    }

    err = reader_init(&reader, STDIN_FILENO, 4096, line_received);
    if (err) {
        log_error("Cannot initialize reader: %s", strerror(errno));
        return 1;
    }

    ev_io_init(&reader.watcher, reader_cb, reader.fd, EV_READ);
    ev_io_start(EV_A_ &reader.watcher);

    ev_run(EV_A_ 0);

    log_info("terminated");

    reader_destroy(&reader);

    err = check_teardown(EV_A);
    if (err != 0)
        log_error("check_teardown: %s", strerror(errno));

    return 0;
}
