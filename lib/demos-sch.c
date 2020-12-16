#include "demos-sch.h"
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#ifdef DEMOS_LIB_VERBOSE
#define LOG(...)                                                                                   \
    do {                                                                                           \
        fprintf(stderr, "[DEMOS] ");                                                               \
        fprintf(stderr, __VA_ARGS__);                                                              \
        fprintf(stderr, "\n");                                                                     \
    } while (0)
#else
#define LOG(...)                                                                                   \
    do {                                                                                           \
    } while (0)
#endif

static int fd_completed, fd_new_period;

int demos_init()
{
    char *str = getenv("DEMOS_FDS");
    if (!str) {
        LOG("Error: Environment variable DEMOS_FDS is missing");
        errno = ENOKEY;
        return -1;
    }

    if (sscanf(str, "%d,%d", &fd_completed, &fd_new_period) != 2) {
        LOG("Error: Failed to load configuration from DEMOS_FDS environment variable");
        errno = EBADMSG;
        return -1;
    }

    LOG("Process library set up.");
    return 0;
}

int demos_completed()
{
    LOG("Notifying scheduler of completion and suspending process...");

    uint64_t buf = 1;
    // notify demos
    if (write(fd_completed, &buf, sizeof(buf)) == -1)
        return -1;

    // block until become readable
    if (read(fd_new_period, &buf, sizeof(buf)) == -1)
        return -1;

    LOG("Process resumed");
    return 0;
}
