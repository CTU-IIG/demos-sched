#include "cgroup.hpp"

Cgroup::Cgroup(std::string path)
    : freezer_path(path)
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

    // open file descriptor for processes in cgroup
    fd_procs = open( (freezer_path + "cgroup.procs").c_str(), O_RDWR | O_NONBLOCK);
    if(fd_procs == -1)
        kill_procs_and_exit("open");

    // open file descriptor for (un)freezing cgroup
    fd_state = open( (freezer_path + "freezer.state").c_str(), O_RDWR | O_NONBLOCK);
    if(fd_state == -1)
        kill_procs_and_exit("open");

}

Cgroup::~Cgroup()
{
    std::cerr<< "destructor " + freezer_path <<std::endl;
    if( fd_procs == -1 ){
        return;
    }

    this->freeze();

    // read pids from fd
    std::vector<pid_t> pids;
    FILE *f = fdopen( fd_procs, "r");
    if( f == NULL )
        err(1,"fdopen, something wrong, need to delete cgroups manually");
    char *line;
    size_t len;
    while( getline(&line, &len, f) > 0 ){
        pids.push_back( atoi(line) );
    }
    fclose(f);
//    if(line)
//        free(line);
    // kill all processes in cgroup
    for( pid_t pid : pids ){
        std::cout<<pid<<std::endl;
        if( kill(pid, SIGKILL) == -1){
            warn("need to kill process %d manually", pid);
        }
    }
    this->unfreeze();
    //TODO wait until cgroup empty
    sleep(1);

    close(fd_procs);
    close(fd_state);

    // delete cgroup, TODO cpuset
    if( rmdir( (freezer_path).c_str()) == -1)
        err(1,"rmdir, something wrong, need to delete cgroups manually");
}

void Cgroup::add_process(pid_t pid)
{
    // add process to cgroup (echo PID > cgroup.procs)
    char buf1[20];
    sprintf(buf1, "%d", pid);
    if( write(fd_procs, buf1, 20*sizeof(pid_t)) == -1)
        kill_procs_and_exit("write");
}

void Cgroup::freeze()
{
    char buf[7] = "FROZEN";
    if( write(fd_state, buf, 6*sizeof(char)) == -1){
        kill_procs_and_exit("frozen write");
    }
}

void Cgroup::unfreeze()
{
    char buf[7] = "THAWED";
    if( write(fd_state, buf, 6*sizeof(char)) == -1){
        kill_procs_and_exit("frozen write");
    }
}
