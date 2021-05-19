#include <sched.h>

/**
 * Simple single-threaded CPU stress test.
 * Does not do I/O, syscalls or touch memory much.
 */
int main()
{
    struct sched_param sp = { .sched_priority = 1 };
    if (sched_setscheduler(0, SCHED_FIFO, &sp) == -1) {
        return 1;
    }

    while (1) {
        asm("");
    }
}
