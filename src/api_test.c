#include "demos-sch.h"
#include <err.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

int main()
{
    if (demos_init() == -1)
        err(1, "demos_init");

    while (true) {
        printf("new_period\n");
        sleep(1);
        printf("completed\n");
        if (demos_completed() == -1)
            err(1, "demos_completed");
    }

    return 0;
}
