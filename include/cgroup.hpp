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
#include "demossched.hpp"


class Cgroup : protected DemosSched
{
public:
    Cgroup(std::string name);
    ~Cgroup();
    void add_process(pid_t pid);
    void freeze();
    void unfreeze();

    // delete copy constructor
    Cgroup(const Cgroup&) = delete;
    Cgroup& operator=(const Cgroup&) = delete;

private:
    int fd_freezer_procs;
    int fd_freezer_state;
    int fd_uni_procs;
    int fd_uni_events;
    int fd_cpuset_procs;
    int fd_cpuset_cpus;
    const std::string freezer_p;
    const std::string cpuset_p;
    const std::string unified_p;

    void clean_cb(ev::stat &w, int revents);
    void close_all_fd();

    static void write_pid(pid_t pid, int fd);
    static void delete_cgroup(std::string path);
    static int open_fd(std::string path, int attr);
};

#endif // CGROUP_HPP
