#include "cgroup.hpp"

Cgroup::Cgroup(std::string path)
{
    // create new cgroup
    int ret = mkdir( path.c_str(),
            S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    if( ret == -1 ){
        if( errno == EEXIST ) {
            kill_procs_and_exit("mkdir, process name has to be unique");
        }else{
            kill_procs_and_exit("mkdir");
        }
    }

    // open file descriptors for manipulating cgroups
    fd_procs = open( (path + "cgroup.procs").c_str(), O_RDWR | O_NONBLOCK);
    if(fd_procs == -1)
        kill_procs_and_exit("open");

}

void Cgroup::add_process(pid_t pid)
{
    // add process to cgroup (echo PID > cgroup.procs)
    char buf1[20];
    sprintf(buf1, "%d", pid);
    if( write(fd_procs, buf1, 20*sizeof(pid_t)) == -1)
        kill_procs_and_exit("write");
}
