#ifndef CLASSES_HPP
#define CLASSES_HPP

#include "Process.h"
#include "functions.h"


// maximum supported number of processors
#define MAX_NPROC 8

// cpu usage mask
typedef std::bitset<MAX_NPROC> Cpu;

class Partition
{
    public:
        Partition( std::vector<Process> processes )
            : processes(processes),
            current(&(this->processes[0]))
    { }

        Process* get_current_proc()
        {
            return current;
        }

        // cyclic queue?
        void move_to_next_proc()
        {
            counter++;
            if( counter >= processes.size() )
                counter = 0;
            current = &(this->processes[counter]);
        }

    private:
        std::vector<Process> processes;
        Process* current;
        size_t counter = 0;
};

struct Slice
{
    Partition &sc;
    Partition &be;
    Cpu cpus;
};

struct Window
{
    std::chrono::nanoseconds length;
    std::vector<Slice> slices;
};

struct MajorFrame
{
    std::chrono::nanoseconds length;
    std::vector<Window> windows;
};

#endif
