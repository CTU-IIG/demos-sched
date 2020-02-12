#ifndef CGROUP_HPP
#define CGROUP_HPP

// mkdir, open
#include <sys/stat.h>
#include <sys/types.h>
// open
#include <fcntl.h>
// write
#include <unistd.h>
#include <iostream>
#include <vector>
#include <err.h>
#include <sys/types.h>
#include <signal.h>
#include <errno.h>
#include <ev++.h>
#include <bitset>
#include "demossched.hpp"

// maximum supported number of processors
#define MAX_NPROC 8
// cpu usage mask
typedef std::bitset<MAX_NPROC> Cpu;

class Cgroup : protected DemosSched
{
public:
    Cgroup(ev::loop_ref loop, bool process_cgrp, std::string cgrp_name = "",
           int fd_cpuset_procs = -1, std::string partition_cgroup_name = "");
    ~Cgroup();
    void add_process(pid_t pid);
    void freeze();
    void unfreeze();

    std::string get_name();
    int get_fd_cpuset_procs();

    void set_cpus(std::string cpus);

    // delete copy constructor
    Cgroup(const Cgroup&) = delete;
    Cgroup& operator=(const Cgroup&) = delete;

    void kill_all();
private:
    static int cgrp_count;
    std::string name;
    int fd_freezer_procs;
    int fd_freezer_state;
    int fd_uni_procs;
    int fd_uni_events;
    int fd_cpuset_procs = -1;
    int fd_cpuset_cpus = -1;
    const std::string freezer_p;
    const std::string cpuset_p;
    const std::string unified_p;
    ev::io procs_w;
    bool populated = false;
    bool killed = false;
    bool deleted = false;
    bool is_process_cgrp;

    ev::loop_ref loop;

    void clean_cb(ev::io &w, int revents);
    void close_all_fd();
    void delete_cgroup();
    void write_pid(pid_t pid, int fd);
    int open_fd(std::string path, int attr);
};

#endif // CGROUP_HPP
