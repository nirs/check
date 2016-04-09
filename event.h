/*
 * Copyright 2016 Nir Soffer <nsoffer@redhat.com>
 *
 * This copyrighted material is made available to anyone wishing to use,
 * modify, copy, or redistribute it subject to the terms and conditions
 * of the GNU General Public License v2 or (at your option) any later version.
 */

#ifndef EVENT_H
#define EVENT_H

/* Event format: NAME PATH ERRNO [DATA|-] */
void event_printf(const char *name, const char *path, int error,
                  const char *fmt, ...);

#endif
