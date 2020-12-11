#ifndef PARTITION_HPP
#define PARTITION_HPP

#include "cgroup.hpp"
#include "cpu_set.hpp"
#include "demossched.hpp"
#include "process.hpp"
#include <chrono>
#include <functional>
#include <list>

using namespace std::placeholders;

typedef std::list<Process> Processes;

/**
 * Container for a group of processes that are scheduled as a single unit. Only
 * a single process from each partition can run at a time - processes are ran
 * sequentially, starting from the first one (but see below).
 *
 * There are 2 types of partitions (depending on slice configuration):
 * In safety-critical partitions, execution in each window restarts from the first
 * process, even if not all processes had chance to run in the last window;
 * in best-effort partitions, execution is continued from last unfinished process.
 */
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
                     bool has_initialization);

    void exec_processes();

    void set_cpus(const cpu_set cpus);

    void move_to_first_proc();
    // return false if there is none
    bool move_to_next_unfinished_proc();

    bool is_completed();
    void clear_completed_flag();
    bool is_empty();
    void kill_all();

    void set_complete_cb(std::function<void()> new_complete_cb);
    void set_empty_cb(std::function<void()> new_empty_cb);

    std::string get_name() const;

    // protected:
    CgroupCpuset cgc;
    // cgf and cge are read by Process constructor in process.cpp
    CgroupFreezer cgf;
    Cgroup cge;
    void proc_exit_cb(Process &proc);
    std::function<void()> completed_cb = nullptr;

private:
    Processes processes = {};
    Processes::iterator current_proc;
    std::string name;
    size_t proc_count = 0;

    bool completed = false;
    bool empty = true;

    // cyclic queue
    void move_to_next_proc();

    std::vector<std::function<void()>> empty_cbs = {};
};

#endif // PARTITION_HPP
