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
