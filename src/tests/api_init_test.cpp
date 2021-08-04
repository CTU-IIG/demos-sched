#include "demos-sch.h"
#include <chrono>
#include <cstdio>
#include <err.h>
#include <thread>

using namespace std::chrono_literals;
using std::this_thread::sleep_for;

int main()
{
    // make stdout line-buffered to avoid synchronization issues for tests
    setvbuf(stdout, nullptr, _IOLBF, 0);

    if (demos_init() == -1) {
        err(1, "demos_init");
    }

    printf("init start\n");

    // this shouldn't do anything, as we haven't finished init yet
    if (demos_completed() == -1) {
        err(1, "demos_completed");
    }

    sleep_for(60ms);

    printf("init done\n");
    if (demos_initialization_completed() == -1) {
        err(1, "demos_initialization_completed");
    }

    return 0;
}
