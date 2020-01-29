#include <iostream>
#include <string>
#include <chrono>
#include <sched.h>
#include <vector>
#include <bitset>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <sys/timerfd.h>
#include <ev++.h>
#include <err.h>
#include <fcntl.h>

// TODO remove get_budgets
// TODO exception after fork

// maximum supported number of processors
#define MAX_NPROC 8

// paths to cgroups
std::string freezer = "/sys/fs/cgroup/freezer/";
std::string cpuset = "/sys/fs/cgroup/cpuset/";
std::string cgrp = "my_cgroup";

using namespace std::chrono_literals;

// std::chrono to timespec conversions
// https://embeddedartistry.com/blog/2019/01/31/converting-between-timespec-stdchrono/
constexpr timespec timepointToTimespec(
        std::chrono::time_point<std::chrono::steady_clock, std::chrono::nanoseconds> tp)
{
    auto secs = std::chrono::time_point_cast<std::chrono::seconds>(tp);
    auto ns = std::chrono::time_point_cast<std::chrono::nanoseconds>(tp) -
                std::chrono::time_point_cast<std::chrono::nanoseconds>(secs);

    return timespec{secs.time_since_epoch().count(), ns.count()};
}

// ev wrapper around timerfd
namespace ev{
    class timerfd : public io
    {
        private:
            int timer_fd;
        public:
            timerfd() : io()
            {
                // create timer
                // steady_clock == CLOCK_MONOTONIC
                timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
                if( timer_fd < 0 )
                    err(1,"timerfd_create");
            }

            void start (std::chrono::steady_clock::time_point timeout)
            {
                // set timeout
                struct itimerspec timer_value;
                // first launch
                timer_value.it_value = timepointToTimespec(timeout);
                // no periodic timer
                timer_value.it_interval = timespec{0,0};

                if( timerfd_settime( timer_fd, TFD_TIMER_ABSTIME, &timer_value, NULL) == -1 )
                   err(1,"timerfd_settime");

                // set fd to callback
                io::start(timer_fd, ev::READ);
            }
            ~timerfd()
            {
                close(timer_fd);
            }
    };
}

// pid of all processes for easy cleanup
std::vector<pid_t> spawned_processes;

// clean up everything and exit
void kill_procs_and_exit(std::string msg)
{
    std::cerr << msg <<std::endl;
    for(pid_t pid : spawned_processes){
        if( kill(pid, SIGKILL) == -1){
            warn("need to kill process %d manually", pid);
            // TODO clean cgroups
        }
    }
    exit(1);
}

// cpu usage mask
typedef std::bitset<MAX_NPROC> Cpu;

class Process
{
    private:
        std::string name;
        std::vector<std::string> argv;
        std::chrono::steady_clock::time_point start_time;
        std::chrono::nanoseconds budget;
        std::chrono::nanoseconds budget_jitter;
        std::chrono::nanoseconds actual_budget;
        bool completed = false;
        pid_t pid = -1;
        // TODO why ev::timerfd timer doesn't work??? 
        ev::timerfd *timer_ptr = new ev::timerfd;
        int fd_freez_procs = -1;
        int fd_freez_state = -1;
    public:
        Process(std::string name,
                std::vector<std::string> argv,
                std::chrono::steady_clock::time_point start_time,
                std::chrono::nanoseconds budget,
                std::chrono::nanoseconds budget_jitter = std::chrono::nanoseconds(0) )
            : name(name),
              argv(argv),
              start_time(start_time),
              budget(budget),
              budget_jitter(budget_jitter),
              actual_budget(budget)
        {
            timer_ptr->set<Process, &Process::timeout_cb>(this);
            //timer.start(budget.count());
        }

        // testing
        void start_timer(std::chrono::nanoseconds timeout)
        {
            timer_ptr->start(start_time + timeout);
        }

        bool is_completed()
        {
            return completed;
        }

        void exec()
        {
            //TODO pipe
            //TODO cgroup, freeze

            // create new freezer cgroup
            // TODO cpuset
            std::string new_freezer = freezer + name + "/";
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

            // create new process
            pid = fork();
            if( pid == -1 )
                kill_procs_and_exit("fork");

            // launch new process
            if( pid == 0 ){
                // add process to cgroup (echo PID > cgroup.procs)
                char buf1[20];
                sprintf(buf1, "%d", getpid());
                if( write(fd_freez_procs, buf1, 20*sizeof(pid_t)) == -1)
                    kill_procs_and_exit("write");
                close(fd_freez_procs);

                // freeze (echo FROZEN > freezer.state)
                char buf2[7] = "FROZEN"; // automatic null terminator?
                if( write(fd_freez_state, buf2, 6*sizeof(char)) == -1)
                    kill_procs_and_exit("write");
                close(fd_freez_state);

                // cast string to char*
                std::vector<char*> cstrings;
                cstrings.reserve( argv.size()+1 );
            
                for(size_t i = 0; i < argv.size(); ++i)
                    cstrings.push_back(const_cast<char*>( argv[i].c_str() ));
                cstrings.push_back( (char*)NULL );
            
                if( execv( cstrings[0], &cstrings[0] ) == -1)
                    kill_procs_and_exit("execv");
            } else {
                // TODO wait until process created and freezed
                sleep(1);
                spawned_processes.push_back(pid);
            }
        }

        // echo FROZEN > freezer.state
        void frozen()
        {
            if( fd_freez_state == -1)
                kill_procs_and_exit("freezer, launch process first");
            char buf[7] = "FROZEN";
            if( write(fd_freez_state, buf, 6*sizeof(char)) == -1)
                kill_procs_and_exit("write");
        }

        // echo THAWED > freezer.state
        void thawed()
        {
            if( fd_freez_state == -1)
                kill_procs_and_exit("freezer, launch process first");
            char buf[7] = "THAWED";
            if( write(fd_freez_state, buf, 6*sizeof(char)) == -1)
                kill_procs_and_exit("write");
        }

        void recompute_budget()
        {
            std::chrono::nanoseconds rnd_val= budget_jitter * rand()/RAND_MAX;
            actual_budget = budget - budget_jitter/2 + rnd_val;
        }

        std::chrono::nanoseconds get_actual_budget()
        {
            return actual_budget;
        }
        
        void timeout_cb (ev::io &w, int revents)
        {
            if (EV_ERROR & revents)
                err(1,"ev cb: got invalid event");

            std::cout << "timeout " << std::endl;
            // read to have empty fd
            uint64_t buf;
            int ret = read(w.fd, &buf, 10);
            if(ret != sizeof(uint64_t) )
               err(1,"read timerfd");
            w.stop();
        }
};

class Partition
{
    public:
        Partition( std::vector<Process> processes )
            : processes(processes),
              current(&(this->processes[0]))
        { }

        Process* get_current_proc()
        {
            return current;
        }

        // cyclic queue?
        void move_to_next_proc()
        {
            counter++;
            if( counter >= processes.size() )
                counter = 0;
            current = &(this->processes[counter]);
        }

    private:
        std::vector<Process> processes;
        Process* current;
        size_t counter = 0;
};

struct Slice
{
        Partition &sc;
        Partition &be;
        Cpu cpus;
};

struct Window
{
        std::chrono::nanoseconds length;
        std::vector<Slice> slices;
};

struct MajorFrame
{
        std::chrono::nanoseconds length;
        std::vector<Window> windows;
};

void print_help()
{
    printf("help:\n"
           "-h\n"
           "\t print this message\n"
           "-g <NAME>\n"
           "\t name of cgroup with user access,\n"
           "\t need to create /sys/fs/cgroup/freezer/<NAME> and\n"
           "\t /sys/fs/cgroup/cpuset/<NAME> manually, then\n"
           "\t sudo chown -R <user> .../<NAME>\n"
           "\t if not set, \"my_cgroup\" is used\n"
           "TODO -c <FILE>\n"
           "\t path to configuration file\n");
}

int main(int argc, char *argv[]) 
{
    // parse arguments
    int opt;
    while((opt = getopt(argc, argv, "hg:c:")) != -1){
        switch(opt){
            case 'h':
                print_help();
                exit(0);
            case 'g':
                cgrp = std::string(optarg);
                break;
            case 'c':
                break;
            default:
                print_help();
                exit(1);
        }
    }
    // create paths to cgroups
    freezer += cgrp + "/";
    cpuset += cgrp + "/";

    // init random seed
    srand(time(NULL));

    // get start time
    std::chrono::steady_clock::time_point start_time = std::chrono::steady_clock::now();
    //std::cout<<start_time.time_since_epoch().count()<<std::endl;
    //std::cout<<(start_time + 10ns).time_since_epoch().count()<<std::endl; 

    // parse yaml config
    // consistency check

    // forks, pipes, exec, cgroups, ...
    
    // TEST data structures
    Process procA("procA", std::vector<std::string>
            {"./infinite_proc","1000000","hello"}, start_time, 10ms,2ms);
    Process procB("procB", std::vector<std::string>
            {"/bin/echo","foo"}, start_time, 5ms,1ms);
    Process procC("procC", std::vector<std::string>
            {"/bin/echo","best effort"}, start_time, 5ms,1ms);
    //std::cout<< procA.exec() <<std::endl;

    Partition sc = Partition( std::vector<Process> {procA, procB});
    Partition be = Partition( std::vector<Process> {procC});

    Slice s = {.sc = sc, .be = be, .cpus = 1};
    Window w1 = {.length = 1s, .slices = std::vector<Slice> {s} };
    Window w2 = {.length = 500ms, .slices = std::vector<Slice> {s, s} };
    MajorFrame frame = {.length = 60ms, .windows = std::vector<Window> {w1, w2} };

    //std::cout<< frame.get_budgets()[0].count() <<std::endl;
    //std::cout<< frame.windows[0].slices[0].sc.get_budgets()[0].count() <<std::endl;
    Process* proc_ptr = frame.windows[0].slices[0].sc.get_current_proc();
    //proc_ptr->recompute_budget();
    //std::cout<<proc_ptr->get_actual_budget().count()<<std::endl;
    proc_ptr->exec();
    proc_ptr->start_timer(3s);
    proc_ptr->thawed();
    
    // configure linux scheduler
    //struct sched_param sp = {.sched_priority = 99};
    //if (sched_setscheduler( 0, SCHED_FIFO, &sp ) == -1)
        //kill_procs_and_exit("sched_setscheduler");

    // how to use exception here?
    //try{
        //int ret = sched_setscheduler( 0, SCHED_FIFO, &sp );
        //std::cout<<ret<<std::endl;
    //} catch (int e){
        //kill_procs_and_exit("sched_setscheduler");
    //}

    ev::default_loop loop;
    loop.run(0);

    kill_procs_and_exit("exit");

    return 0;
}

