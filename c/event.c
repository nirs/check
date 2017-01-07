/*
 * Copyright 2016 Nir Soffer <nsoffer@redhat.com>
 *
 * This copyrighted material is made available to anyone wishing to use,
 * modify, copy, or redistribute it subject to the terms and conditions
 * of the GNU General Public License v2 or (at your option) any later version.
 */

#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "event.h"
#include "log.h"

void event_printf(const char *name, const char *path, int error,
                  const char *fmt, ...)
{
    char buf[2048];
    ssize_t len = 0;
    ssize_t count;

    count = snprintf(buf, sizeof(buf), "%s %s %d ", name, path, error);
    assert(count > 0 && count < (ssize_t)sizeof(buf) && "event truncated");
    len += count;

    va_list ap;
    va_start(ap, fmt);

    count = vsnprintf(buf + len, sizeof(buf) - len, fmt, ap);
    assert(count > 0 && count < (ssize_t)sizeof(buf) - len && "event truncated");
    len += count;

    va_end(ap);

    buf[len++] = '\n';

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
