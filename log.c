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
#include <unistd.h>

#include "log.h"

void log_printf(const char *fmt, ...)
{
    va_list argp;

    va_start(argp, fmt);

    char buf[2048];
    ssize_t len = vsnprintf(buf, sizeof(buf), fmt, argp);
    assert(len > 0 && "error formatting log message");

    va_end(argp);

    if (len >= (int)sizeof(buf)) {
        len = sizeof(buf);
        buf[len - 1] = '\n';
    }

    ssize_t nwritten;
    do {
        nwritten = write(STDERR_FILENO, buf, len);
    } while (nwritten == -1 && errno == EINTR);

    /* Ignore partial writes, logging is not critical */
}
