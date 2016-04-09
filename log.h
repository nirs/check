/*
 * Copyright 2016 Nir Soffer <nsoffer@redhat.com>
 *
 * This copyrighted material is made available to anyone wishing to use,
 * modify, copy, or redistribute it subject to the terms and conditions
 * of the GNU General Public License v2 or (at your option) any later version.
 */

#ifndef LOG_H
#define LOG_H

extern int debug_mode;

void log_printf(const char *fmt, ...);

#define log_level(level, fmt, args...) \
    log_printf(level " " fmt "\n", ##args)

#define log_debug(fmt, args...) \
do { \
    if (debug_mode) \
        log_level("DEBUG", fmt, ##args); \
} while (0)

#define log_info(fmt, args...) log_level("INFO", fmt, ##args)
#define log_error(fmt, args...) log_level("ERROR", fmt, ##args)

#endif
