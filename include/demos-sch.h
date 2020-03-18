#ifndef DEMOSSCH_H
#define DEMOSSCH_H

/*
 * Initialize interface for demos-sched.
 * @return -1 if initialization failed, otherwise 0
 */
int demos_init();

/*
 * Signalizes to the scheduler that all work for the current
 * window is completed (see spec.)
 * Block until the new scheduling by demos-sched.
 * @return 0
 * @todo make sure, that process cannot be scheduld multiple times in one window
 */
int demos_completed();

#endif // DEMOSSCH_H
