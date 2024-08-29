#ifndef _LOGGER_H
#define _LOGGER_H

// #define LOCAL_MACHINE
// ------------------ debug --------------------
#ifdef LOCAL_MACHINE
#define debug(...) printf(__VA_ARGS__)
#define PANIC(fmt, ...)                                                                                 \
    do {                                                                                                \
        fprintf(stderr, "\033[1;41mPanic: %s:%d: " fmt "\033[0m\n", __FILE__, __LINE__, ##__VA_ARGS__); \
        _exit(1);                                                                                       \
    } while (0)

#define PANIC_ON(condition, message, ...)        \
    do {                                         \
        if (condition) {                         \
            PANIC("%s", message, ##__VA_ARGS__); \
        }                                        \
    } while (0)

#else
#define debug(...)
#define PANIC(...)
#define PANIC_ON(...)
#endif

#endif