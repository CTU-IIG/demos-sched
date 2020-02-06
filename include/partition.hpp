#ifndef PARTITION_HPP
#define PARTITION_HPP

#include "process.hpp"
#include "cgroup.hpp"
#include "demossched.hpp"
#include <list>

typedef std::list<Process> Processes;

class Partition : protected DemosSched
{
    public:
        Partition( Processes &&processes, std::string cgroup_name );
        Process & get_current_proc();
        // cyclic queue
        void move_to_next_proc();

    private:
        Processes processes;
        Processes::iterator current;
        Cgroup cgroup;
};

#endif // PARTITION_HPP
