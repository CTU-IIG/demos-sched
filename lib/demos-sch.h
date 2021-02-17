#ifndef DEMOSSCH_H
#define DEMOSSCH_H

/**
 * Initialize interface for demos-sched.
 *
 * Will be called automatically by `demos_completed()` on first
 * invocation if not called explicitly before.
 *
 * @return 0 if successful, -1 otherwise and `errno` is set appropriately
 */
int demos_init();

/**
 * Signalizes to the scheduler that initialization is completed
 * and that process may now be scheduled according to configuration.
 *
 * If initialization is not expected now, `errno` is set to `EWOULDBLOCK` and the function
 * immediately returns -1. This happens either when the process does not have initialization
 * enabled (`init: yes`) in the DEmOS configuration file or this function was already called.
 *
 * @return 0 if successful, -1 otherwise and `errno` is set appropriately
 */
int demos_initialization_completed();

/**
 * Signalizes to the scheduler that all work for the current window is completed (see spec.)
 * Blocks until the process is scheduled again by demos-sched.
 *
 * @return 0 if successful, -1 otherwise and `errno` is set appropriately.
 *
 * @todo make sure, that process cannot be scheduled multiple times in one window
 * @todo set errno
 */
int demos_completed();

#endif // DEMOSSCH_H
