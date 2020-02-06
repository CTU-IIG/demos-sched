#include "slice.hpp"

Slice::Slice(Partition &sc, Partition &be, Cpu cpus)
    : sc(sc)
    , be(be)
    , cpus(cpus)
{
    timer.set(std::bind(&Slice::timeout_cb, this));
    // timer.start(start_time + timeout);
}

void Slice::timeout_cb()
{
    // TODO do all process, partition swtiching stuff
    std::cerr << "slice timeout" << std::endl;
}
