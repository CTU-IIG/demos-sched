#include "demos-sch.h"
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#ifdef DEMOS_LIB_VERBOSE
#define LOG_DEBUG(...)                                                                             \
    do {                                                                                           \
        fprintf(stderr, "[DEMOS] ");                                                               \
        fprintf(stderr, __VA_ARGS__);                                                              \
        fprintf(stderr, "\n");                                                                     \
    } while (0)
#else
#define LOG_DEBUG(...)                                                                             \
    do {                                                                                           \
    } while (0)
#endif

static int fd_completed, fd_new_period;
// indicates if demos_init_ran was already called
static bool demos_init_ran = false;
static bool initialization_pending;

int demos_init()
{
    // save that we already ran `demos_init()`
    demos_init_ran = true;

    char *str = getenv("DEMOS_PARAMETERS");
    if (!str) {
        LOG_DEBUG("Error: Environment variable DEMOS_PARAMETERS is missing");
        errno = ENOKEY;
        return -1;
    }

    // need to use tmp int, scanf doesn't know boolean
    int tmp_init_flag;
    if (sscanf(str, "%d,%d,%d", &fd_completed, &fd_new_period, &tmp_init_flag) != 3) {
        LOG_DEBUG("Error: Failed to load configuration from DEMOS_PARAMETERS environment variable");
        errno = EBADMSG;
        return -1;
    }

    initialization_pending = (bool)tmp_init_flag;
    LOG_DEBUG("Process %s an initialization window",
              initialization_pending ? "has" : "does not have");

    LOG_DEBUG("Process library set up");
    return 0;
}

int demos_completed()
{
    // to make the library usage as easy as possible, we check
    //  if `demos_init()` was already ran and if not, we run
    //  it ourselves in this first call to `demos_completed()`
    if (!demos_init_ran) {
        LOG_DEBUG("Calling `demos_init()` in first run of `demos_completed()`");
        int s = demos_init();
        if (s != 0) return s;
    }

    if (initialization_pending) {
        LOG_DEBUG("Ignoring call to `demos_completed()` during initialization window");
        return 0;
    }

    LOG_DEBUG("Notifying scheduler of completion and suspending process...");

    uint64_t buf = 1;
    // notify demos
    if (write(fd_completed, &buf, sizeof(buf)) == -1) {
        return -1;
    }

    // block until become readable
    if (read(fd_new_period, &buf, sizeof(buf)) == -1) {
        return -1;
    }

    LOG_DEBUG("Process resumed");
    return 0;
}

int demos_initialization_completed()
{
    if (!demos_init_ran) {
        LOG_DEBUG("Calling `demos_init()` from `demos_initialization_completed()`");
        int s = demos_init();
        if (s != 0) return s;
    }

    if (!initialization_pending) {
        LOG_DEBUG(
          "Warning: Called `demos_initialization_completed()` without pending initialization");
    }

    initialization_pending = false;
    LOG_DEBUG("Initialization completed");
    return demos_completed();
}