/*
 * Copyright 2016 Nir Soffer <nsoffer@redhat.com>
 *
 * This copyrighted material is made available to anyone wishing to use,
 * modify, copy, or redistribute it subject to the terms and conditions
 * of the GNU General Public License v2 or (at your option) any later version.
 */

#ifndef READER_H
#define READER_H

typedef void (*received_cb)(char *);

struct reader {
    ev_io watcher;
    int fd;
    received_cb cb;
    size_t bufsize;
    ssize_t len;
    char *buf;
};

int reader_init(struct reader *r, int fd, size_t bufsize, received_cb cb);
void reader_destroy(struct reader *r);
void reader_clear(struct reader *r);
void reader_shift(struct reader *r, char *partial_line);
ssize_t reader_available(struct reader *r);
int reader_read(struct reader *r);
void reader_process(struct reader *r);

#endif
