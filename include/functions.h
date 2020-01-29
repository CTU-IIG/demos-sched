#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#include <iostream>
#include <vector>
#include <ev++.h>
#include <err.h>

// pid of all processes for easy cleanup
extern std::vector<pid_t> spawned_processes;

// clean up everything and exit
void kill_procs_and_exit(std::string msg);
void print_help();

#endif
