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
#include <getopt.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>  /* STD*_FILNO */

#include <ev.h>

#include "check.h"
#include "event.h"
#include "log.h"
#include "reader.h"

#define MAX_CMD_ARGS 3
#define EXIT_USAGE 2
#define VERSION "0.1"

int debug_mode;
static long max_paths = 128;
static ev_signal sigint_watcher;
static ev_signal sigterm_watcher;
static ev_signal sighup_watcher;
static int exit_code = 0;

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
        log_error("error reading from fd %d: %s", r->fd, strerror(errno));
        exit(EXIT_FAILURE);
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
        event_printf("-", "-", EINVAL, "-");
        return;
    }

    if (strcmp(cmd, "start") == 0) {
        char *path = argv[1];
        if (path == NULL) {
            log_error("path required");
            event_printf(cmd, "-", EINVAL, "-");
            return;
        }

        char *interval_string = argv[2];
        if (interval_string == NULL) {
            log_error("interval required");
            event_printf(cmd, path, EINVAL, "-");
            return;
        }

        char *endp;
        long interval = strtol(interval_string, &endp, 10);
        if (interval_string == endp || *endp != 0) {
            /* Not converted, or trailing characters */
            log_error("invalid interval: '%s'", interval_string);
            event_printf(cmd, path, EINVAL, "-");
            return;
        }
        if (interval < 0 || interval == INT_MAX) {
            log_error("interval out of range: '%s'", interval_string);
            event_printf(cmd, path, EINVAL, "-");
            return;
        }

        check_start(EV_A_ path, interval);
    } else if (strcmp(cmd, "stop") == 0) {
        char *path = argv[1];
        if (path == NULL) {
            log_error("path required");
            event_printf(cmd, "-", EINVAL, "-");
            return;
        }

        check_stop(EV_A_ path);
    } else {
        log_error("invalid command: '%s'", cmd);
        event_printf(cmd, "-", EINVAL, "-");
    }
}

static long read_aio_max_nr(void)
{
    const char *path = "/proc/sys/fs/aio-max-nr";
    long result = -1;

    FILE *fp = fopen(path, "r");

    if (fp == NULL) {
        if (errno != ENOENT) {
            log_error("error opening %s: %s", path, strerror(errno));
            exit(EXIT_FAILURE);
        }
        log_debug("%s not found", path);
        return -1;
    }

    char buf[22];
    char *p = fgets(buf, sizeof(buf), fp);
    if (p == NULL) {
        log_debug("cannot read from %s", path);
        goto cleanup;
    }

    result = strtol(buf, NULL, 10);

cleanup:
    fclose(fp);

    return result;
}

static void usage(void)
{
    puts(
"check - path checking helper\n"
"\n"
"usage:\n"
"  check [-h] [-v] [--debug-mode] [--paths PATHS]\n"
"\n"
"arguments:\n"
"  -h, --help           show this help message and exit\n"
"  -V, --version        show version and exit\n"
"  -d, --debug-mode     enable debug-mode (default disabled)\n"
"  -p, --max-paths PATHS\n"
"                       maximum number of paths (default 128)"
    );
}

static void parse_args(int argc, char *argv[])
{
    struct option long_options[] = {
        {"help",        no_argument,       0, 'h' },
        {"version",     no_argument,       0, 'V' },
        {"debug-mode",  no_argument,       0, 'd' },
        {"max-paths",   required_argument, 0, 'p' },
        {0, 0, 0, 0 }
    };

    while (1) {
        int c;
        int option_index = 0;

        c = getopt_long(argc, argv, "hVdp:", long_options, &option_index);
        if (c == -1)
            break;

        switch (c) {
        case 'h':
            usage();
            exit(EXIT_SUCCESS);
        case 'V':
            printf("check version %s\n", VERSION);
            exit(EXIT_SUCCESS);
        case 'd':
            debug_mode = 1;
            break;
        case 'p':
            max_paths = strtol(optarg, NULL, 10);
            long aio_max_nr = read_aio_max_nr();
            if (max_paths < 0 || (aio_max_nr > 0 && max_paths > aio_max_nr)) {
                log_error("max_paths out of range (1-%d)\n", aio_max_nr);
                usage();
                exit(EXIT_USAGE);
            }
            break;
        case '?':
        default:
            usage();
            exit(EXIT_USAGE);
        }
    }

    log_debug("using max_paths=%d, debug_mode=%d", max_paths, debug_mode);
}

static void exit_cb(EV_P_ ev_signal *w, int revents)
{
    log_info("received signal %d", w->signum);
    exit_code = 128 + w->signum;
    ev_break(EV_A_ EVBREAK_ALL);
}

void setup_signals(void)
{
    struct ev_loop *loop = EV_DEFAULT;

    ev_signal_init(&sigint_watcher, exit_cb, SIGINT);
    ev_signal_start(EV_A_ &sigint_watcher);

    ev_signal_init(&sigterm_watcher, exit_cb, SIGTERM);
    ev_signal_start(EV_A_ &sigterm_watcher);

    ev_signal_init(&sighup_watcher, exit_cb, SIGHUP);
    ev_signal_start(EV_A_ &sighup_watcher);
}

int main(int argc, char *argv[])
{
    int err;
    struct reader reader;
    struct ev_loop *loop = EV_DEFAULT;

    log_info("started (version %s)", VERSION);

    parse_args(argc, argv);

    setup_signals();

    err = check_setup(EV_A_ max_paths);
    if (err != 0) {
        log_error("check_setup: %s", strerror(errno));
        return 1;
    }

    err = set_nonblocking(STDIN_FILENO);
    if (err) {
        log_error("cannot set fd %d nonblocking: %s",
                  STDIN_FILENO, strerror(errno));
        return 1;
    }

    err = set_nonblocking(STDOUT_FILENO);
    if (err) {
        log_error("cannot set fd %d nonblocking: %s",
                  STDOUT_FILENO, strerror(errno));
        return 1;
    }

    err = set_nonblocking(STDERR_FILENO);
    if (err) {
        log_error("cannot set fd %d nonblocking: %s",
                  STDERR_FILENO, strerror(errno));
        return 1;
    }

    err = reader_init(&reader, STDIN_FILENO, 4096, line_received);
    if (err) {
        log_error("cannot initialize reader: %s", strerror(errno));
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

    return exit_code;
}
