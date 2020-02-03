#include "freezer.hpp"

Freezer::Freezer(std::string path) : Cgroup (path)
{
    // open file descriptors for (un)freezing cgroup
    fd_state = open( (path + "freezer.state").c_str(), O_RDWR | O_NONBLOCK);
    if(fd_state == -1)
        kill_procs_and_exit("open");
}

void Freezer::freeze()
{
    char buf[7] = "FROZEN";
    if( write(fd_state, buf, 6*sizeof(char)) == -1){
        kill_procs_and_exit("frozen write");
    }
}

void Freezer::unfreeze()
{
    char buf[7] = "THAWED";
    if( write(fd_state, buf, 6*sizeof(char)) == -1){
        kill_procs_and_exit("frozen write");
    }
}
