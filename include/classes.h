#ifndef CLASSES_H
#define CLASSES_H

#include <bitset>

#include "Process.h"
#include "functions.h"


// maximum supported number of processors
#define MAX_NPROC 8

// cpu usage mask
typedef std::bitset<MAX_NPROC> Cpu;

class Partition
{
    public:
        Partition( std::vector<Process> processes );
        Process* get_current_proc();
        // cyclic queue?
        void move_to_next_proc();

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
