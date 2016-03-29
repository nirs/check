/*
 * Copyright 2016 Nir Soffer <nsoffer@redhat.com>
 *
 * This copyrighted material is made available to anyone wishing to use,
 * modify, copy, or redistribute it subject to the terms and conditions
 * of the GNU General Public License v2 or (at your option) any later version.
 */

#include <libaio.h>
#include <assert.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <stdio.h>
#include <errno.h>
#include <sched.h>
#include <unistd.h>

#define BUFSIZE 4096

static void setup_priority(void)
{
	struct sched_param sched_param = {0};
	int rv = 0;

	rv = sched_get_priority_max(SCHED_RR);
    assert(rv >= 0);

	sched_param.sched_priority = rv;
	rv = sched_setscheduler(0, SCHED_RR|SCHED_RESET_ON_FORK, &sched_param);
    assert(rv == 0);
}

static void drop_privileges(void)
{
    int rv;

    rv = setgid(36);
    assert(rv == 0);

    rv = setuid(36);
    assert(rv == 0);
}

static void difftimespec(struct timespec *start, struct timespec *end, struct timespec *elapsed)
{
    elapsed->tv_sec = end->tv_sec - start->tv_sec;
    elapsed->tv_nsec = end->tv_nsec - start->tv_nsec;

    if (elapsed->tv_nsec < 0) {
        elapsed->tv_nsec += 1000000000;
        elapsed->tv_sec += 1;
    }
}


int main(int argc, char *argv[])
{
    int err;
    io_context_t ctx;
    struct iocb cb;
    struct iocb *cbs[1];
    void *buf;
    int fd;
    struct timespec timeout = {30, 0};
    struct timespec start = {0};
    struct timespec end = {0};
    struct timespec elapsed = {0};
    struct timespec delay = {2, 0};
    struct io_event events[1];
    long result;

    if (argc != 2) {
        fprintf(stderr, "Usage: monitord path\n");
        return 2;
    }

    fd = open(argv[1], O_RDONLY | O_DIRECT);
    assert(fd != -1);

    memset(&ctx, 0, sizeof(ctx));
    err = io_setup(32, &ctx);
    assert(err == 0);

    err = posix_memalign(&buf, 4096, BUFSIZE);
    assert(err == 0);

    cbs[0] = &cb;

    setup_priority();
    drop_privileges();

    printf("timestamp,latency,status\n");

    for (;;) {
        memset(buf, 0, BUFSIZE);
        io_prep_pread(&cb, fd, buf, BUFSIZE, 0);

        clock_gettime(CLOCK_REALTIME, &start);

        err = io_submit(ctx, 1, cbs);
        assert(err == 1);

        do {
            err = io_getevents(ctx, 1, 1, events, &timeout);
        } while (err == -EINTR);

        clock_gettime(CLOCK_REALTIME, &end);

        assert(err == 1);
        assert(events[0].obj == &cb);

        /**
         * res is the number bytes read
         * - typically BUFSIZE.
         * - when reaching end of file, partial read: 0-BUFSIZE
         * - when request fails: -EIO)
         **/
        result = (long)events[0].res;

        if (result < 0)
            fprintf(stderr, "Read error: %s\n", strerror(-result));
        else if (result < BUFSIZE)
            fprintf(stderr, "Partial read: %ld\n", result);

        difftimespec(&start, &end, &elapsed);
        printf("%0ld.%09ld,%0ld.%09ld,%d\n",
               end.tv_sec, end.tv_nsec,
               elapsed.tv_sec, elapsed.tv_nsec,
               result == BUFSIZE);

        err = clock_nanosleep(CLOCK_MONOTONIC, 0, &delay, NULL);
        assert(err == 0);
    }

    return 0;
}
