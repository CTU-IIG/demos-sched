#ifndef CGROUP_HPP
#define CGROUP_HPP

// mkdir, open
#include <sys/stat.h>
#include <sys/types.h>
// open
#include <fcntl.h>
// write
#include <unistd.h>
// kill all
#include "functions.hpp"


class Cgroup
{
public:
    Cgroup(std::string path);
    ~Cgroup();
    void add_process(pid_t pid);
    void freeze();
    void unfreeze();

    // delete copy constructor
    Cgroup(const Cgroup&) = delete;
    Cgroup& operator=(const Cgroup&) = delete;

//private:
    int fd_procs;
    int fd_state;
    std::string freezer_path;
};

#endif // CGROUP_HPP
