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
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/eventfd.h>
#include <sys/queue.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <ev.h>
#include <libaio.h>

#include "log.h"
#include "check.h"

#define REAP_RUNNING 10

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

static struct check * check_lookup(char *path);
static struct check * check_new(char *path, int interval);
static void check_free(struct check *ck);
static void check_cb(EV_P_ ev_timer *w, int revents);
static void check_reap(EV_P_ ev_io *w, int revents);
static int check_open_fd(struct check *ck);
static int check_submit(struct check *ck);
static void check_completed(struct check *ck, int error, ev_tstamp when);
static void check_stopped(struct check *ck);

static TAILQ_HEAD(checkhead, check) checkers = TAILQ_HEAD_INITIALIZER(checkers);
static int checkers_count;
static int running;
static complete_cb complete;
static io_context_t ioctx;
static int ioeventfd = -1;
static ev_io ioeventfd_watcher;
static int pagesize;
static int max_checkers;

int check_setup(EV_P_ int max_paths, complete_cb cb)
{
    int saved_errno;

    max_checkers = max_paths;
    complete = cb;

    /* Consider using min align and min transfer for file based paths */
    pagesize = sysconf(_SC_PAGESIZE);
    if (pagesize == -1)
        return -1;

    ioeventfd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    if (ioeventfd == -1)
        return -1;

    ev_io_init(&ioeventfd_watcher, check_reap, ioeventfd, EV_READ);
    ev_io_start(EV_A_ &ioeventfd_watcher);

    memset(&ioctx, 0, sizeof(ioctx));
    int err = io_setup(max_checkers, &ioctx);
    if (err) {
        saved_errno = -err;
        goto error;
    }

    return 0;

error:
    ev_io_stop(EV_A_ &ioeventfd_watcher);
    close(ioeventfd);
    ioeventfd = -1;
    errno = saved_errno;
    return -1;
}

int check_teardown(EV_P)
{
    ev_io_stop(EV_A_ &ioeventfd_watcher);

    if (ioeventfd != -1) {
        close(ioeventfd);
        ioeventfd = -1;
    }

    int err = io_destroy(ioctx);
    if (err) {
        errno = -err;
        return -1;
    }

    memset(&ioctx, 0, sizeof(ioctx));

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

    log_info("start checking path '%s' every %d seconds", path, interval);

    struct check *ck = check_lookup(path);
    if (ck) {
        log_error("already checking path '%s'", path);
        /* TODO: return error to caller */
        return;
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
    checkers_count++;
}

void check_stop(EV_P_ char *path)
{
    log_info("stop checking path '%s'", path);

    struct check *ck = check_lookup(path);
    if (ck == NULL) {
        log_debug("not checking '%s'", path);
        return;
    }

    ev_timer_stop(EV_A_ &ck->timer);

    if (ck->state == RUNNING) {
        ck->state = STOPPING;
        /* io_cancel fail with EINVAL with both iSCSI and NFS, so we can only
         * wait. */
        log_debug("checker '%s' is running, waiting until io completes",
                  ck->path);
    } else if (ck->state == WAITING) {
        check_stopped(ck);
    } else if (ck->state == STOPPING) {
        log_debug("stopping '%s' in progress", ck->path);
    } else {
        assert(0 && "invalid state during stop");
    }
}

static struct check *check_lookup(char *path)
{
    struct check *ck;
    TAILQ_FOREACH(ck, &checkers, entries) {
        if (strcmp(ck->path, path) == 0)
            return ck;
    }
    return NULL;
}

static void check_cb(EV_P_ ev_timer *w, int revents)
{
    struct check *ck = (struct check *)w;

    if (ck->state == WAITING) {
        ck->state = RUNNING;
        ck->start = ev_time();
        running++;
        if (ck->fd == -1) {
            if (check_open_fd(ck))
                return;
        }
        check_submit(ck);
        /* If we have large number of running requests, some are likely to be
         * ready now. */
        if (running > REAP_RUNNING)
            check_reap(EV_A_ &ioeventfd_watcher, 0);
    } else if (ck->state == RUNNING) {
        ev_tstamp elapsed = ev_time() - ck->start;
        log_error("checker '%s' blocked for %.6f seconds", ck->path, elapsed);
    } else {
        assert(0 && "invalid state during check callback");
    }
}

static void check_reap(EV_P_ ev_io *w, int revents)
{
    eventfd_t nevents;
    int err;

    do {
        err = eventfd_read(ioeventfd, &nevents);
    } while (err == -1 && errno == EINTR);

    if (err) {
        if (errno == EAGAIN)
            return;
        log_error("eventfd_read: %s", strerror(errno));
        return;
    }

    /* We don't have timing information per request, assume all of them
     * finished now. */
    ev_tstamp now = ev_time();

    int nreap = (int)nevents;
    assert(nreap <= checkers_count && "unexpected number of events");
    log_debug("received %d events on eventfd", nreap);

    struct io_event events[nreap];
    int nready;

    do {
        nready = io_getevents(ioctx, 0, nreap, events, NULL);
    } while (nready == -EINTR);

    if (nready < 0) {
        log_error("io_getevents: %s", strerror(-nready));
        return;
    }

    log_debug("reaped %d io events", nready);

    for (int i = 0; i < nready; i++) {
        struct check *ck = (struct check *)(events[i].obj->data);
        if (ck->state == STOPPING) {
            check_stopped(ck);
        } else if (ck->state == RUNNING) {
            /* Partial read (res < bufsize) is ok */
            int res = events[i].res;
            if (res < 0)
                log_error("checking '%s' failed: %s",
                          ck->path, strerror(-res));
            check_completed(ck, res < 0 ? -res : 0, now);
        } else {
            assert(0 && "invalid state during reap");
        }
    }
}

static int check_open_fd(struct check *ck)
{
    log_debug("opening '%s'", ck->path);

    ev_tstamp start;
    if (debug_mode)
        start = ev_time();

    ck->fd = open(ck->path, O_RDONLY | O_DIRECT | O_NONBLOCK);

    if (ck->fd == -1) {
        int saved_errno = errno;
        log_error("error opening '%s': %s", ck->path, strerror(errno));
        check_completed(ck, saved_errno, 0);
        return -1;
    }

    if (debug_mode) {
        ev_tstamp elapsed = ev_time() - start;
        log_debug("openned '%s' in %.6f seconds", ck->path, elapsed);
    }

    return 0;
}

static int check_submit(struct check *ck)
{
    log_debug("submitting io for '%s'", ck->path);

    io_prep_pread(&ck->iocb, ck->fd, ck->buf, pagesize, 0);
    io_set_eventfd(&ck->iocb, ioeventfd);
    ck->iocb.data = ck;

    ev_tstamp start;
    if (debug_mode)
        start = ev_time();

    struct iocb *ios[1] = {&ck->iocb};
    int nios = io_submit(ioctx, 1, ios);

    if (nios < 0) {
        log_error("io_submit: %s", strerror(-nios));
        check_completed(ck, -nios, 0);
        return -1;
    }

    /* Not sure if this can fail, so lets assert */
    assert(nios == 1 && "partial submit");

    if (debug_mode) {
        ev_tstamp elapsed = ev_time() - start;
        log_debug("submitted io for '%s' in %.6f seconds",
                  ck->path, elapsed);
    }

    return 0;
}

static void check_completed(struct check *ck, int error, ev_tstamp when)
{
    ck->state = WAITING;
    running--;
    assert(running >= 0 && "negative number of running requests");
    ev_tstamp delay = error == 0 ? when - ck->start : 0;
    complete(ck->path, error, delay);
}

static void check_stopped(struct check *ck)
{
    log_debug("checker '%s' stopped", ck->path);

    if (ck->state == STOPPING) {
        running--;
        assert(running >= 0 && "negative number of running requests");
    }

    TAILQ_REMOVE(&checkers, ck, entries);

    checkers_count--;
    assert(checkers_count >= 0 && "negative number of checkers");

    check_free(ck);
    /* Send stopped event */
    return;
}
