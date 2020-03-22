#ifndef DEMOSSCH_H
#define DEMOSSCH_H

/*
 * Initialize interface for demos-sched.
 * @return if initialization failed, return -1
 *         and errno set appropriately, otherwise 0
 * @todo set errno
 */
int demos_init();

/*
 * Signalizes to the scheduler that all work for the current
 * window is completed (see spec.)
 * Block until the new scheduling by demos-sched.
 * @return if failed, return -1 and errno set appropriately,
 *         otherwise return 0
 * @todo make sure, that process cannot be scheduld multiple times in one window
 * @todo set errno
 */
int demos_completed();

#endif // DEMOSSCH_H
