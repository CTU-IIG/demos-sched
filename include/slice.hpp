#ifndef SLICE_HPP
#define SLICE_HPP

#include "partition.hpp"
#include "timerfd.hpp"
#include "demossched.hpp"
#include <bitset>

// maximum supported number of processors
#define MAX_NPROC 8

// cpu usage mask
typedef std::bitset<MAX_NPROC> Cpu;

class Slice : protected DemosSched
{
public:
    Slice(Partition &sc, Partition &be, Cpu cpus);

    Partition &sc;
    Partition &be;
    Cpu cpus;
private:
    ev::timerfd timer;
    void timeout_cb();
};

#endif // SLICE_HPP
