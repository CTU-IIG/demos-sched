#ifndef DEMOSSCH_H
#define DEMOSSCH_H

/*
 * Initialize interface for demos-sched.
 * @return Zero if successful, -1 otherwise and errno is set appropriately
 * @todo set errno
 */
int demos_init();

/*
 * Signalizes to the scheduler that all work for the current
 * window is completed (see spec.)
 * Block until the new scheduling by demos-sched.
 * @return Zero if successful, -1 otherwise and errno is set appropriately.
 * @todo make sure, that process cannot be scheduld multiple times in one window
 * @todo set errno
 */
int demos_completed();

#endif // DEMOSSCH_H
