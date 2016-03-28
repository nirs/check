#ifndef LOG_H
#define LOG_H

extern int debug_mode;

#define log_level(level, fmt, args...) \
do { \
    fprintf(stderr, "%s " fmt "\n", level, ##args); \
} while (0)

#define log_debug(fmt, args...) \
do { \
    if (debug_mode) \
        log_level("DEBUG", fmt, ##args); \
} while (0)

#define log_error(fmt, args...) log_level("ERROR", fmt, ##args)

#endif
