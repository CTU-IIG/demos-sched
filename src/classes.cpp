#include "classes.hpp"

Partition::Partition( std::vector<Process> processes )
    : processes(processes),
    current(&(this->processes[0]))
{ }

Process* Partition::get_current_proc()
{
    return current;
}

// cyclic queue?
void Partition::move_to_next_proc()
{
    counter++;
    if( counter >= processes.size() )
        counter = 0;
    current = &(this->processes[counter]);
}
