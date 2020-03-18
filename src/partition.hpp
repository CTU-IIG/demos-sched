#ifndef PARTITION_HPP
#define PARTITION_HPP

#include "cgroup.hpp"
#include "demossched.hpp"
#include "process.hpp"
#include <chrono>
#include <functional>
#include <list>

using namespace std::placeholders;

typedef std::list<Process> Processes;

class Partition
{
public:
    // Partition( Processes &&processes, std::string cgroup_name );
    Partition(Cgroup &freezer_parent,
              Cgroup &cpuset_parent,
              Cgroup &events_parent,
              std::string name = "");

    Process &get_current_proc();

    void freeze();
    void unfreeze();

    void add_process(ev::loop_ref loop,
                     std::string argv,
                     std::chrono::nanoseconds budget,
                     std::chrono::nanoseconds budget_jitter = std::chrono::nanoseconds(0));

    void set_cpus(std::string cpus);

    void move_to_first_proc();
    // return false if there is none
    bool move_to_next_unfinished_proc();

    bool is_completed();
    void clear_completed_flag();
    bool is_empty();
    void kill_all();

    void set_complete_cb(std::function<void ()> new_complete_cb);
    void set_empty_cb(std::function<void()> new_empty_cb);

    std::string get_name();

    // protected:
    CgroupFreezer cgf;
    CgroupCpuset cgc;
    Cgroup cge;
    void proc_exit_cb(Process &proc);
    std::function<void()> completed_cb;
private:
    Processes processes;
    Processes::iterator current;
    std::string name;
    size_t proc_count = 0;

    bool completed = false;
    bool empty = true;

    // cyclic queue
    void move_to_next_proc();

    std::vector<std::function<void()>> empty_cbs;
};

#endif // PARTITION_HPP
