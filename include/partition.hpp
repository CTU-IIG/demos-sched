#ifndef PARTITION_HPP
#define PARTITION_HPP

#include "process.hpp"
#include "cgroup.hpp"
#include "demossched.hpp"
#include <list>
#include <chrono>

typedef std::list<Process> Processes;

class Partition
{
public:
    //Partition( Processes &&processes, std::string cgroup_name );
    Partition(ev::loop_ref loop,
              std::string freezer_path,
              std::string cpuset_path,
              std::string events_path,
              std::string name = "" );
    //~Partition();

    Process & get_current_proc();

    void freeze();
    void unfreeze();

    // TODO somehow call Process() without copying
    void add_process(ev::loop_ref loop,
                     std::vector<char *> argv,
                     std::chrono::nanoseconds budget,
                     std::chrono::nanoseconds budget_jitter = std::chrono::nanoseconds(0));

    void set_cpus(std::string cpus);
    //std::string get_name();

    void move_to_first_proc();
    // return false if there is none
    bool move_to_next_unfinished_proc();

    bool is_completed();
    void clear_completed_flag();

    bool is_empty();

//protected:
    CgroupFreezer cgf;
    CgroupCpuset cgc;
    CgroupEvents cge;
    void proc_exit_cb(Process &proc);
private:
    Processes processes;
    Processes::iterator current;
    std::string name;

    bool completed = false;
    bool empty = true;

    // cyclic queue
    void move_to_next_proc();

};

#endif // PARTITION_HPP
