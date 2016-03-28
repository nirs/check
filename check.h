#ifndef CHECK_H
#define CHECK_H

typedef void (*complete_cb)(char *path, ev_tstamp delay);

void check_set_cb(complete_cb cb);
void check_start(EV_P_ char *path, int interval);
void check_stop(EV_P_ char *path);

#endif
