/*
 * Copyright 2016 Nir Soffer <nsoffer@redhat.com>
 *
 * This copyrighted material is made available to anyone wishing to use,
 * modify, copy, or redistribute it subject to the terms and conditions
 * of the GNU General Public License v2 or (at your option) any later version.
 */

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/queue.h>
#include <unistd.h>

#include <ev.h>
#include <libaio.h>

#include "log.h"
#include "check.h"

enum {WAITING, RUNNING, STOPPING};

struct check {
    ev_timer timer;
    char path[1024];
    int interval;
    TAILQ_ENTRY(check) entries;
    int state;
    ev_tstamp start;
    int fd;
    struct iocb iocb;
    void *buf;
};

static struct check * check_new(char *path, int interval);
static void check_free(struct check *ck);
static void check_stopped(struct check *ck);

static void check_cb(EV_P_ ev_timer *w, int revents);

static TAILQ_HEAD(checkhead, check) checkers = TAILQ_HEAD_INITIALIZER(checkers);
static complete_cb complete;
static io_context_t ioctx;

int check_setup(int max_paths, complete_cb cb)
{
    /*
     * - init eventfd
     * - register with loop check_reap
     */

    complete = cb;

    memset(&ioctx, 0, sizeof(ioctx));
    return io_setup(max_paths, &ioctx);
}

int check_teardown(void)
{
    int err = io_destroy(ioctx);
    if (err)
        return err;

    memset(&ioctx, 0, sizeof(ioctx));

    /*
     * - unregister check_reap
     * - close eventfd
     */

    return 0;
}

static struct check * check_new(char *path, int interval)
{
    int saved_errno;

    struct check *ck = malloc(sizeof(*ck));
    if (ck == NULL)
        return NULL;

    strncpy(ck->path, path, sizeof(ck->path) - 1);
    ck->path[sizeof(ck->path) - 1] = 0;

    ck->interval = interval;
    ck->state = WAITING;
    ck->fd = -1;
    ck->buf = NULL;

    /* Consider using min align and min transfer for file based paths */
    int pagesize = sysconf(_SC_PAGESIZE);
    if (pagesize == -1) {
        saved_errno = errno;
        goto error;
    }

    int err = posix_memalign(&ck->buf, pagesize, pagesize);
    if (err) {
        saved_errno = err;
        goto error;
    }

    return ck;

error:
    free(ck->buf);
    free(ck);
    errno = saved_errno;
    return NULL;
}

static void check_free(struct check *ck)
{
    if (ck == NULL)
        return;

    if (ck->fd != -1)
        close(ck->fd);

    free(ck->buf);
    free(ck);
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

    ck = check_new(path, interval);
    if (ck == NULL) {
        log_error("check_new: %s", strerror(errno));
        /* TODO: return error to caller */
        return;
    }

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
            ev_timer_stop(EV_A_ &ck->timer);
            if (ck->state == RUNNING)
                ck->state = STOPPING;
            else
                check_stopped(ck);
            return;
        }
    }

    log_debug("not checking '%s'", path);
}

static void check_stopped(struct check *ck)
{
    log_debug("checker '%s' stopped", ck->path);
    TAILQ_REMOVE(&checkers, ck, entries);
    check_free(ck);
    /* Send stopped event */
    return;
}

static void check_cb(EV_P_ ev_timer *w, int revents)
{
    struct check *ck = (struct check *)w;

    /*
     * - if running, log warning about block check
     * - if fd is closed, open fd
     * - preapre for read
     * - set eventfd
     * - submit io
     * - change state to RUNNING
     */

    complete(ck->path, 0.0);
}
