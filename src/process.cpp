#include "process.hpp"

// paths to cgroups
std::string Process::freezer_path = "/sys/fs/cgroup/freezer/my_cgroup/";
std::string Process::cpuset_path = "/sys/fs/cgroup/cpuset/my_cgroup/";
//std::string cgrp = "my_cgroup";

Process::Process(std::string name,
        std::vector<std::string> argv,
        std::chrono::steady_clock::time_point start_time,
        std::chrono::nanoseconds budget,
        std::chrono::nanoseconds budget_jitter )
    : name(name),
    argv(argv),
    start_time(start_time),
    budget(budget),
    budget_jitter(budget_jitter),
    actual_budget(budget)
{
    timer_ptr->set<Process, &Process::timeout_cb>(this);
}

// testing
void Process::start_timer(std::chrono::nanoseconds timeout)
{
    timer_ptr->start(start_time + timeout);
}

bool Process::is_completed()
{
    return completed;
}

// echo FROZEN > freezer.state
void Process::freeze()
{
    if( fd_freez_state == -1)
        kill_procs_and_exit("freezer, launch process first");
    char buf[7] = "FROZEN";
    if( write(fd_freez_state, buf, 6*sizeof(char)) == -1){
        kill_procs_and_exit("frozen write");
    }
}

void Process::unfreeze()
{
    if( fd_freez_state == -1)
        kill_procs_and_exit("freezer, launch process first");
    char buf[7] = "THAWED";
    if( write(fd_freez_state, buf, 6*sizeof(char)) == -1)
        kill_procs_and_exit("thawed write");
}

void Process::recompute_budget()
{
    std::chrono::nanoseconds rnd_val= budget_jitter * rand()/RAND_MAX;
    actual_budget = budget - budget_jitter/2 + rnd_val;
}

std::chrono::nanoseconds Process::get_actual_budget()
{
    return actual_budget;
}

void Process::exec()
{
    //TODO pipe
    //TODO cgroup, freeze

    // create new freezer cgroup
    // TODO cpuset
    std::string new_freezer = freezer_path + name + "/";
    int ret = mkdir( new_freezer.c_str(),
            S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    if( ret == -1 ){
        if( errno == EEXIST ) {
            kill_procs_and_exit("mkdir, process name has to be unique");
        }else{
            kill_procs_and_exit("mkdir");
        }
    }

    // open file descriptors for manipulating cgroups
    fd_freez_procs = open( (new_freezer + "cgroup.procs").c_str(), O_RDWR | O_NONBLOCK);
    if(fd_freez_procs == -1)
        kill_procs_and_exit("open");
    fd_freez_state = open( (new_freezer + "freezer.state").c_str(), O_RDWR | O_NONBLOCK);
    if(fd_freez_state == -1)
        kill_procs_and_exit("open");

    // freeze cgroup
    this->freeze();

    // create new process
    pid = fork();
    if( pid == -1 )
        kill_procs_and_exit("fork");

    // launch new process
    if( pid == 0 ){
        // CHILD PROCESS
        // add process to cgroup (echo PID > cgroup.procs)
        char buf1[20];
        sprintf(buf1, "%d", getpid());
        if( write(fd_freez_procs, buf1, 20*sizeof(pid_t)) == -1)
            kill_procs_and_exit("write");
        close(fd_freez_procs);
        close(fd_freez_state);

        // cast string to char*
        std::vector<char*> cstrings;
        cstrings.reserve( argv.size()+1 );

        for(size_t i = 0; i < argv.size(); ++i)
            cstrings.push_back(const_cast<char*>( argv[i].c_str() ));
        cstrings.push_back( (char*)NULL );

        if( execv( cstrings[0], &cstrings[0] ) == -1)
            kill_procs_and_exit("execv");
        // END CHILD PROCESS
    } else {
        // PARENT PROCESS
        spawned_processes.push_back(pid);
        // END PARENT PROCESS
    }
}

void Process::timeout_cb (ev::io &w, int revents)
{
    if (EV_ERROR & revents)
        err(1,"ev cb: got invalid event");

    std::cout << "timeout " << std::endl;
    // read to have empty fd
    uint64_t buf;
    int ret = read(w.fd, &buf, 10);
    if(ret != sizeof(uint64_t) )
        kill_procs_and_exit("read timerfd");
    w.stop();
}

void Process::set_cgroup_paths(std::string freezer, std::string cpuset)
{
    freezer_path = freezer;
    cpuset_path = cpuset;
}

// WHEN THE DESTRUCTOR IS CALLED???
//Process::~Process()
void Process::clean()
{
    //std::cout<< "destructor " << name <<std::endl;
    std::cout<< "cleaning " << name <<std::endl;
    if( fd_freez_procs == -1 )
        return;
    this->freeze();
    // read pids from fd
    std::vector<pid_t> pids;
    FILE *f = fdopen( fd_freez_procs, "r");
    if( f == NULL )
        err(1,"fdopen, something wrong, need to delete cgroups manually");
    char *line;
    size_t len;
    while( getline(&line, &len, f) > 0 ){
        pids.push_back( atoi(line) );
    }
    fclose(f);
    if(line)
        free(line);
    // kill all processes in cgroup
    for( pid_t pid : pids ){
        std::cout<<pid<<std::endl;
        if( kill(pid, SIGKILL) == -1){
            warn("need to kill process %d manually", pid);
        }
    }

    // thawed cgroup
    this->unfreeze();
    // TODO wait until cgroup empty
    sleep(1);

    close(fd_freez_procs);
    close(fd_freez_state);
    // delete cgroup, TODO cpuset
    if( rmdir( (freezer_path + name).c_str()) == -1)
        err(1,"rmdir, something wrong, need to delete cgroups manually");
}
