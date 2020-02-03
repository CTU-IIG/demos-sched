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
    void add_process(pid_t pid);
//private:
    int fd_procs;
};

#endif // CGROUP_HPP
