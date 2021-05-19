#include "demos-sch.h"
#include <err.h>

int main()
{
    // initialize the library, checking for errors
    if (demos_init() < 0) {
        err(1, "demos_init");
    }

    // initialize the program here

    // signal that initialization is completed, also checking for errors
    if (demos_initialization_completed() < 0) {
        err(1, "demos_initialization_completed");
    }

    while (1) {
        // do work for current window

        // yield the remaining budget for this window (signal completion),
        //  still checking for errors :)
        if (demos_completed() < 0) {
            err(1, "demos_completed");
        }
    }
}
