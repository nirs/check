/*
 * Copyright 2016 Nir Soffer <nsoffer@redhat.com>
 *
 * This copyrighted material is made available to anyone wishing to use,
 * modify, copy, or redistribute it subject to the terms and conditions
 * of the GNU General Public License v2 or (at your option) any later version.
 */

#ifndef CHECK_H
#define CHECK_H

int check_setup(EV_P_ int max_paths);
int check_teardown(EV_P);

void check_start(EV_P_ char *path, int interval);
void check_stop(EV_P_ char *path);

#endif
