#ifndef FREEZER_HPP
#define FREEZER_HPP

#include "cgroup.hpp"

// open
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
// write
#include <unistd.h>
// kill all
#include "functions.hpp"

class Freezer : public Cgroup
{
public:
    Freezer(std::string path);
    void freeze();
    void unfreeze();
//private:
    int fd_state;
};

#endif // FREEZER_HPP
