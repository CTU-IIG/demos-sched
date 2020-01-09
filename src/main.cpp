#include <iostream>
#include <sched.h>
#include <vector>
#include <bitset>
#include <unistd.h>
#include <time.h>
#include <sys/timerfd.h>
#include <ev++.h>

// number of processors in system, need to know at compile time
#define NPROC 8

#define handle_error(msg) do { perror(msg); exit(EXIT_FAILURE); } while (0)

using namespace std;

// what should be used as time_type?
typedef double time_type;

// cpu usage mask
typedef bitset<NPROC> Cpu;

class Process
{
    public:
        //Process(int argc, char **argv, time_type budget, time_type budget_jitter)
        Process(vector<string> argv, time_type budget, time_type budget_jitter)
        {
            //this->argv = vector<string>( argv, argv + argc );
            this->argv = argv;
            this->budget = budget;
            this->budget_jitter = budget_jitter;
            this->actual_budget = budget;
            this->completed = false;
        }

        bool is_completed()
        {
            return completed;
        }

        int exec()
        {
            // cast string to char*
            vector<char*> cstrings;
            cstrings.reserve( argv.size()+1 );
            
            for(int i = 0; i < argv.size(); ++i)
                cstrings.push_back(const_cast<char*>( argv[i].c_str() ));
            cstrings.push_back( (char*)NULL );
            
            return execv( cstrings[0], &cstrings[0] );
        }

        void recompute_budget()
        {
            actual_budget = budget - budget_jitter/2 + budget_jitter*((time_type) rand()/RAND_MAX);
        }

        time_type get_budget()
        {
            return actual_budget;
        }
        
    private:
        vector<string> argv;
        time_type budget;
        time_type budget_jitter;
        time_type actual_budget;
        bool completed;

};

class Partition
{
    public:
        Partition( vector<Process> processes )
        {
            this->processes = processes;
            this->current = &(this->processes[0]);
            this->counter = 0;
        }

        Process* get_current_proc()
        {
            return current;
        }

        // cyclic queue?
        void next_proc()
        {
            counter++;
            if( counter >= processes.size() )
                counter = 0;
            current = &(this->processes[counter]);
        }

    private:
        vector<Process> processes;
        Process* current;
        int counter;
};

struct Slice
{
        Partition sc;
        Partition be;
        Cpu cpus;
};

struct Window
{
        time_type length;
        vector<Slice> slices;
};

struct MajorFrame
{
        time_type length;
        vector<Window> windows;
};

// example of ev++ usage:
// https://gist.github.com/koblas/3364414
class EvTimerfd
{
    private:
        int timer_fd;
        ev::io timerfd_watcher;
        struct timespec period;
        struct timespec start_time;
        int timer_num;
    public:
        EvTimerfd( struct timespec start_time,
                   struct timespec period,
                   int timer_num)
        {
            this->timer_num = timer_num;

            this->period.tv_sec = period.tv_sec;
            this->period.tv_nsec = period.tv_nsec;
            this->start_time.tv_sec = start_time.tv_sec;
            this->start_time.tv_nsec = start_time.tv_nsec;

            // configure timer
            this->timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
            if( this->timer_fd < 0 )
                handle_error("timerfd_create");

            struct itimerspec timer_value;
            // first launch
            timer_value.it_value.tv_sec = start_time.tv_sec + period.tv_sec;
            long nsec = start_time.tv_nsec + period.tv_nsec;
            // timespec nsec workaround
            if( nsec > 999999999){
                timer_value.it_value.tv_sec++;
                nsec -= 1000000000;
            }
            timer_value.it_value.tv_nsec = nsec;
            // period
            timer_value.it_interval.tv_sec = period.tv_sec;
            timer_value.it_interval.tv_nsec = period.tv_nsec;
            // set timer
            if( timerfd_settime( this->timer_fd, TFD_TIMER_ABSTIME, &timer_value, NULL) == -1 )
                handle_error("timerfd_settime");

            // configure ev watcher
            timerfd_watcher.set<EvTimerfd, &EvTimerfd::timeout_cb>(this);
            timerfd_watcher.start(this->timer_fd, ev::READ);
        }

        void timeout_cb (ev::io &w, int revents)
        {
            if (EV_ERROR & revents)
                handle_error("ev cb: got invalid event");

            cout << "timer "<< timer_num <<" expired" << endl;
        
            // read to have empty fd
            uint64_t buf;
            int ret = read(timer_fd, &buf, 10);
            if(ret != sizeof(uint64_t) )
                handle_error("read timerfd");
        }
};


int main(int argc, char *argv[]) 
{
    // configure linux scheduler
    struct sched_param sp = {.sched_priority = 99};
    sched_setscheduler( 0, SCHED_FIFO, &sp );

    // init random seed
    srand(time(NULL));

    // parse yaml config
    // consistency check

    // forks, pipes, exec, cgroups, ...
    
    // TEST data structures
    Process procA(vector<string> {"/bin/echo","hello","world"}, 10,2);
    Process procB(vector<string> {"/bin/echo","foo"}, 5,1);
    Process procC(vector<string> {"/bin/echo","best effort"}, 5,1);
    //cout<< procA.exec() <<endl;

    Partition sc = Partition( vector<Process> {procA, procB});
    Partition be = Partition( vector<Process> {procC});

    Slice s = {.sc = sc, .be = be, .cpus = 1};
    Window w1 = {.length = 20, .slices = vector<Slice> {s} };
    Window w2 = {.length = 40, .slices = vector<Slice> {s, s} };
    MajorFrame frame = {.length = 60, .windows = vector<Window> {w1, w2} };

    //Process* proc_ptr = frame.windows[0].slices[0].sc.get_current_proc();
    //proc_ptr->exec();
   
    // TEST timers
    struct timespec start_time;
    if( clock_gettime(CLOCK_MONOTONIC, &start_time) == -1 )
        handle_error("clock_gettime");

    ev::default_loop loop;
    EvTimerfd timer1( start_time,
            (struct timespec) {.tv_sec = 1, .tv_nsec = 0}, 1);
    EvTimerfd timer2( start_time,
            (struct timespec) {.tv_sec = 0, .tv_nsec = 500000000}, 2);
    loop.run(0);

    return 0;
}

