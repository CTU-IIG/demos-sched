#include <iostream>
#include <chrono>
#include <sched.h>
#include <vector>
#include <bitset>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/timerfd.h>
#include <ev++.h>
#include <err.h>

// TODO remove get_budgets
// TODO exception after fork

// maximum supported number of processors
#define MAX_NPROC 8

using namespace std;
using namespace std::chrono;
using namespace std::chrono_literals;

// std::chrono::nanoseconds and struct timespec conversions
// https://embeddedartistry.com/blog/2019/01/31/converting-between-timespec-stdchrono/
nanoseconds timespec2duration( struct timespec ts)
{
    auto duration = seconds{ts.tv_sec} + nanoseconds{ts.tv_nsec};
    return duration_cast<nanoseconds>(duration);
}

struct timespec duration2timespec( nanoseconds dur )
{
    auto secs = duration_cast<seconds>(dur);
    dur -= secs;
    return timespec{secs.count(), dur.count()};
}

// pid of all processes for easy cleanup
vector<pid_t> spawned_processes;

// clean up everything and exit
void kill_procs_and_exit(string msg)
{
    cerr << msg <<endl;
    for(pid_t pid : spawned_processes){
        if( kill(pid, SIGKILL) == -1){
            warn("need to kill process %d manually", pid);
            // TODO clean cgroups
        }
    }
    exit(1);
}

// cpu usage mask
typedef bitset<MAX_NPROC> Cpu;

class Process
{
    public:
        Process(vector<string> argv, nanoseconds budget, nanoseconds budget_jitter = 0ns)
            : argv(argv),
              budget(budget),
              budget_jitter(budget_jitter),
              actual_budget(budget)
        { }

        bool is_completed()
        {
            return completed;
        }

        void exec()
        {
            //TODO pipe
            //TODO cgroup, freeze

            pid = fork();
            if( pid == -1 )
                kill_procs_and_exit("fork");
                //err(1,"fork");

            if( pid == 0 ){ // launch new process
                // cast string to char*
                vector<char*> cstrings;
                cstrings.reserve( argv.size()+1 );
            
                for(size_t i = 0; i < argv.size(); ++i)
                    cstrings.push_back(const_cast<char*>( argv[i].c_str() ));
                cstrings.push_back( (char*)NULL );
            
                if( execv( cstrings[0], &cstrings[0] ) == -1)
                    kill_procs_and_exit("execv");
                    //err(1,"execv");
            } else {
                spawned_processes.push_back(pid);
            }
        }

        void recompute_budget()
        {
            nanoseconds rnd_val= budget_jitter * rand()/RAND_MAX;
            actual_budget = budget - budget_jitter/2 + rnd_val;
        }

        nanoseconds get_actual_budget()
        {
            return actual_budget;
        }
        
    private:
        vector<string> argv;
        nanoseconds budget;
        nanoseconds budget_jitter;
        nanoseconds actual_budget;
        bool completed = false;
        pid_t pid = -1;

};

class Partition
{
    public:
        Partition( vector<Process> processes )
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

        vector<nanoseconds> get_budgets()
        {
            vector<nanoseconds> ret;
            ret.reserve(processes.size());
            for(size_t i=0;i<processes.size();i++)
                ret.push_back(processes[i].get_actual_budget());
            return ret;
        }

    private:
        vector<Process> processes;
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
        nanoseconds length;
        vector<Slice> slices;
};

struct MajorFrame
{
        nanoseconds length;
        vector<Window> windows;

        vector<nanoseconds> get_budgets()
        {
            vector<nanoseconds> ret;
            ret.reserve(windows.size());
            for(size_t i=0;i<windows.size();i++)
                ret.push_back(windows[i].length);
            return ret;
        }
};

// example of ev++ usage:
// https://gist.github.com/koblas/3364414
//class ev::timerfd: public ev::io
class EvTimerfd: public ev::io
{
    private:
        int timer_fd;
        ev::io timerfd_watcher;
        vector<nanoseconds> budgets;
        size_t ring_buf_idx = 0;
        nanoseconds timeout;
        nanoseconds last_timeout;
        int timer_num;
    public:
        EvTimerfd( nanoseconds start_time,
                   vector<nanoseconds> budgets,
                   int timer_num)
        {
            this->timer_num = timer_num;

            this->budgets = budgets;
            this->last_timeout = start_time;
            this->timeout = start_time + budgets[0];

            // configure timer
            this->timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
            if( this->timer_fd < 0 )
                err(1,"timerfd_create");

            struct itimerspec timer_value;
            // first launch
            timer_value.it_value = duration2timespec(this->timeout);
            // no periodic timer
            timer_value.it_interval = duration2timespec(nanoseconds{0});

            // set timer
            if( timerfd_settime( this->timer_fd, TFD_TIMER_ABSTIME, &timer_value, NULL) == -1 )
               err(1,"timerfd_settime");

            // configure ev watcher
            timerfd_watcher.set<EvTimerfd, &EvTimerfd::timeout_cb>(this);
            timerfd_watcher.start(this->timer_fd, ev::READ);
        }

        void timeout_cb (ev::io &w, int revents)
        {
            if (EV_ERROR & revents)
                err(1,"ev cb: got invalid event");

            cout << "timer "<< timer_num <<" expired after "
                 << budgets[ring_buf_idx].count()<< " ns" << endl;
        
            // read to have empty fd
            uint64_t buf;
            int ret = read(timer_fd, &buf, 10);
            if(ret != sizeof(uint64_t) )
               err(1,"read timerfd");

            // move pointer to cyclic buffer of budgets
            ring_buf_idx++;
            if( ring_buf_idx >= budgets.size() )
                ring_buf_idx = 0;

            // reset timer
            last_timeout = timeout;
            timeout += budgets[ring_buf_idx];
            struct itimerspec timer_value;
            timer_value.it_value = duration2timespec(this->timeout);
            timer_value.it_interval = duration2timespec(nanoseconds{0});

            if( timerfd_settime( timer_fd, TFD_TIMER_ABSTIME, &timer_value, NULL) == -1 )
                err(1,"timerfd_settime");
        }
};


int main(int argc, char *argv[]) 
{
    // init random seed
    srand(time(NULL));

    // parse yaml config
    // consistency check

    // forks, pipes, exec, cgroups, ...
    
    // TEST data structures
    Process procA(vector<string> {"./infinite_proc","1000000","hello"}, 10ms,2ms);
    Process procB(vector<string> {"/bin/echo","foo"}, 5ms,1ms);
    Process procC(vector<string> {"/bin/echo","best effort"}, 5ms,1ms);
    //cout<< procA.exec() <<endl;

    Partition sc = Partition( vector<Process> {procA, procB});
    Partition be = Partition( vector<Process> {procC});

    Slice s = {.sc = sc, .be = be, .cpus = 1};
    Window w1 = {.length = 1s, .slices = vector<Slice> {s} };
    Window w2 = {.length = 500ms, .slices = vector<Slice> {s, s} };
    MajorFrame frame = {.length = 60ms, .windows = vector<Window> {w1, w2} };

    //cout<< frame.get_budgets()[0].count() <<endl;
    //cout<< frame.windows[0].slices[0].sc.get_budgets()[0].count() <<endl;
    Process* proc_ptr = frame.windows[0].slices[0].sc.get_current_proc();
    //proc_ptr->recompute_budget();
    //cout<<proc_ptr->get_actual_budget().count()<<endl;
    proc_ptr->exec();
   
    // TEST timers
    struct timespec start_time;
    if( clock_gettime(CLOCK_MONOTONIC, &start_time) == -1 )
        kill_procs_and_exit("clock_gettime");
        //err(1,"clock_gettime");
    nanoseconds start_ns = timespec2duration( start_time );

    ev::default_loop loop;
    EvTimerfd timer1( start_ns, frame.get_budgets(), 1);
    //EvTimerfd timer2( start_ns, 500ms, 2);
    
    // configure linux scheduler
    //struct sched_param sp = {.sched_priority = 99};
    //if (sched_setscheduler( 0, SCHED_FIFO, &sp ) == -1)
        //kill_procs_and_exit("sched_setscheduler");

    // how to use exception here?
    //try{
        //int ret = sched_setscheduler( 0, SCHED_FIFO, &sp );
        //cout<<ret<<endl;
    //} catch (int e){
        //kill_procs_and_exit("sched_setscheduler");
    //}

    loop.run(0);

    return 0;
}

