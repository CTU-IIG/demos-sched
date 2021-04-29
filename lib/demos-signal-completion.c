/**
 * Executable version of demos-sch, which allows for simple usage from shell scripts.
 *
 * If using `init: yes` in config file, call this after script init, and then call again
 * whenever you want to signal completion for current window.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>

int main()
{
    char *str = getenv("DEMOS_PARAMETERS");
    if (!str) {
        return 100;
    }

    int fd_completed, fd_new_period;
    if (sscanf(str, "%d,%d,", &fd_completed, &fd_new_period) != 2) {
        return 101;
    }

    uint64_t buf = 1;
    if (write(fd_completed, &buf, sizeof(buf)) != sizeof(buf)) {
        return 1;
    }
    if (read(fd_new_period, &buf, sizeof(buf)) != sizeof(buf)) {
        return 2;
    }
    return 0;
}
