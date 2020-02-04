#ifndef CLASSES_H
#define CLASSES_H

#include <bitset>

#include "process.hpp"
#include <list>
#include <chrono>


// maximum supported number of processors
#define MAX_NPROC 8

// cpu usage mask
typedef std::bitset<MAX_NPROC> Cpu;

typedef std::list<Process> Processes;

class Partition
{
    public:
        Partition( Processes &&processes );
        Process & get_current_proc();
        // cyclic queue
        void move_to_next_proc();

    private:
        Processes processes;
        Processes::iterator current;
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
