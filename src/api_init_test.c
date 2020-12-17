#include "demos-sch.h"
#include <err.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

// to make it visible in CPU trace
void busy_sleep(unsigned int time_s)
{
    time_t end_time = time_s + time(0);
    while (end_time > time(0)) {
    }
}

void csleep(unsigned int time_s)
{
    if (getenv("TEST_BUSY_SLEEP")) {
        busy_sleep(time_s);
    } else {
        sleep(time_s);
    }
}

int main(int argc, char *argv[])
{
    int i = 0;

    if (demos_init() == -1) {
        err(1, "demos_init");
    }

    printf("starting initialization (5 second wait)\n");
    // this shouldn't do anything, as we haven't finished init yet
    if (demos_completed() == -1) {
        err(1, "demos_completed");
    }
    csleep(5);
    printf("initialization finished, signalling...\n");
    if (demos_initialization_completed() == -1) {
        err(1, "demos_completed");
    }

    while (argc == 1 || ++i < argc) {
        if (argc == 1) {
            printf("new_period\n");
            csleep(1);
            printf("completed\n");
        } else {
            printf("%s\n", argv[i]);
        }

        if (demos_completed() == -1) {
            err(1, "demos_completed");
        }
    }

    return 0;
}
