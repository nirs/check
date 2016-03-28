#include <stdio.h>
#include <errno.h>
#include <unistd.h>  /* STD*_FILNO */
#include <fcntl.h>
#include <string.h>
#include <ev.h>

#include "lineio.h"
#include "log.h"

int debug_mode;

static int
set_nonblocking(int fd)
{
    int flags;
    int err;

    flags = fcntl(fd, F_GETFL);
    if (flags == -1)
        return -1;

    err = fcntl(fd, F_SETFL, flags| O_NONBLOCK);
    if (err == -1)
        return -1;

    return 0;
}

static void
line_received(char *line)
{
    log_debug("received line: '%s'", line);
}

int main(int argc, char *argv[])
{
    int err;
    struct lineio lineio;

    if (argc > 1)
        debug_mode = strcmp(argv[1], "-d") == 0;

    err = set_nonblocking(STDIN_FILENO);
    if (err) {
        log_error("Cannot set fd %d nonblocking: %s",
                  STDIN_FILENO, strerror(errno));
        return 1;
    }

    lineio_init(&lineio, STDIN_FILENO, line_received);

    ev_io_init(&lineio.watcher, lineio_cb, lineio.fd, EV_READ);
    ev_io_start(EV_DEFAULT, &lineio.watcher);

    ev_run(EV_DEFAULT, 0);

    log_debug("terminated");

    return 0;
}
