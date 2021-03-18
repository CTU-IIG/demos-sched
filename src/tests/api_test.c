#include "demos-sch.h"
#include <err.h>
#include <stdio.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    // disable stdout buffering to avoid synchronization issues for tests
    setbuf(stdout, NULL);

    if (demos_init() == -1) {
        err(1, "demos_init");
    }

    int i = 0;
    while (argc == 1 || ++i < argc) {
        if (argc == 1) {
            printf("new_period\n");
            sleep(1);
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
