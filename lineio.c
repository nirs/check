/*
 * Copyright 2016 Nir Soffer <nsoffer@redhat.com>
 *
 * This copyrighted material is made available to anyone wishing to use,
 * modify, copy, or redistribute it subject to the terms and conditions
 * of the GNU General Public License v2 or (at your option) any later version.
 */

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>  /* read */
#include <string.h>  /* strsep */
#include <ev.h>

#include "lineio.h"
#include "log.h"

void lineio_init(struct lineio *lio, int fd, received_cb cb)
{
    lio->fd = fd;
    lio->cb = cb;
    lineio_clear(lio);
}

void lineio_clear(struct lineio *lio)
{
    lio->end = lio->buf;
    *lio->end = 0;
}

void lineio_shift(struct lineio *lio, char *partial_line)
{
    assert(partial_line > lio->buf && partial_line < lio->end);

    ssize_t len = lio->end - partial_line;
    memmove(lio->buf, partial_line, len);
    lio->end = lio->buf + len;
    *lio->end = 0;
}

ssize_t lineio_available(struct lineio *lio)
{
    return lio->buf + sizeof(lio->buf) - lio->end - 1;
}

int lineio_read(struct lineio *lio)
{
    ssize_t nread;

    do {
        nread = read(lio->fd, lio->end, lineio_available(lio));
    } while (nread == -1 && errno == EINTR);

    if (nread == -1) {
        if (errno == EAGAIN)
            return 0;
        return -1;
    }

    if (nread == 0) {
        errno = ECONNRESET;
        return -1;
    }

    lio->end += nread;
    *lio->end = 0;

    log_debug("read %ld bytes len=%ld", nread, lio->end - lio->buf);

    return nread;
}

void lineio_process(struct lineio *lio)
{
    char *next = lio->buf;
    char *line;

    for (;;) {
        line = strsep(&next, "\n");

        if (next == NULL) {

            if (line == lio->buf && lineio_available(lio) == 0) {
                /* Buffer full without newline in sight - drop entire buffer */
                log_error("discarding excessive long line");
                lineio_clear(lio);
                /* TODO: send error to caller */
                return;
            }

            if (line == lio->end) {
                /* No more data to process */
                log_debug("all data processed");
                lineio_clear(lio);
                return;
            }

            if (line > lio->buf) {
                /* Shit partial line to start fo buffer */
                log_debug("shift partial line: '%s'", line);
                lineio_shift(lio, line);
                return;
            }

            /* Partial line at start of buffer, wait for more data */
            log_debug("partial data, waiting for more data");
            return;
        }

        lio->cb(line);
    }
}

void lineio_cb(EV_P_ ev_io *w, int revents)
{
    struct lineio *lio = (struct lineio *)w;
    int nread;

    nread = lineio_read(lio);
    if (nread == -1) {
        perror("ERROR lineio_read");
        ev_break(EV_A_ EVBREAK_ALL);
        return;
    }

    if (nread)
        lineio_process(lio);
}
