/*
 * Copyright 2016 Nir Soffer <nsoffer@redhat.com>
 *
 * This copyrighted material is made available to anyone wishing to use,
 * modify, copy, or redistribute it subject to the terms and conditions
 * of the GNU General Public License v2 or (at your option) any later version.
 */

#ifndef LINEIO_H
#define LINEIO_H

typedef void (*received_cb)(char *);

struct lineio {
    ev_io watcher;
    int fd;
    received_cb cb;
    char *end;
    char buf[4096];
};

void lineio_init(struct lineio *lio, int fd, received_cb cb);
void lineio_clear(struct lineio *lio);
void lineio_shift(struct lineio *lio, char *partial_line);
ssize_t lineio_available(struct lineio *lio);
int lineio_read(struct lineio *lio);
void lineio_process(struct lineio *lio);
void lineio_cb(EV_P_ ev_io *w, int revents);

#endif
