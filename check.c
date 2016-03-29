/*
 * Copyright 2016 Nir Soffer <nsoffer@redhat.com>
 *
 * This copyrighted material is made available to anyone wishing to use,
 * modify, copy, or redistribute it subject to the terms and conditions
 * of the GNU General Public License v2 or (at your option) any later version.
 */

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/queue.h>
#include <ev.h>

#include "log.h"
#include "check.h"

struct check {
    ev_timer timer;
    char path[1024];
    int interval;
    TAILQ_ENTRY(check) entries;
};

static void check_init(struct check *ck, char *path, int interval);
static void check_cb(EV_P_ ev_timer *w, int revents);

static TAILQ_HEAD(checkhead, check) checkers = TAILQ_HEAD_INITIALIZER(checkers);
static complete_cb complete;

void check_set_cb(complete_cb cb)
{
    complete = cb;
}

static void check_init(struct check *ck, char *path, int interval)
{
    strncpy(ck->path, path, sizeof(ck->path) - 1);
    ck->path[sizeof(ck->path) - 1] = 0;
    ck->interval = interval;
}

void check_start(EV_P_ char *path, int interval)
{
    struct check *ck;

    log_info("start checking path '%s' every %d seconds", path, interval);

    TAILQ_FOREACH(ck, &checkers, entries) {
        if (strcmp(ck->path, path) == 0) {
            log_error("already checking path '%s'", path);
            /* TODO: return error to caller */
            return;
        }
    }

    ck = malloc(sizeof(*ck));
    assert(ck);

    check_init(ck, path, interval);

    ev_timer_init(&ck->timer, check_cb, 0, ck->interval);
    ev_timer_start(EV_A_ &ck->timer);

    TAILQ_INSERT_TAIL(&checkers, ck, entries);
}

void check_stop(EV_P_ char *path)
{
    struct check *ck;

    log_info("stop checking path '%s'", path);

    TAILQ_FOREACH(ck, &checkers, entries) {
        if (strcmp(ck->path, path) == 0) {
            TAILQ_REMOVE(&checkers, ck, entries);
            ev_timer_stop(EV_A_ &ck->timer);
            free(ck);
            return;
        }
    }

    log_debug("not checking '%s'", path);
}

static void check_cb(EV_P_ ev_timer *w, int revents)
{
    struct check *ck = (struct check *)w;

    /* Fake it for now */
    complete(ck->path, 0.0);
}
