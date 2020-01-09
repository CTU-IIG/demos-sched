#include <iostream>
#include <sched.h>
#include <vector>
#include <bitset>
#include <unistd.h>
#include <time.h>

// number of processors in system, need to know at compile time
#define NPROC 8

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

int main(int argc, char *argv[]) 
{
    // configure linux scheduler
    struct sched_param sp = {.sched_priority = 99};
    sched_setscheduler( 0, SCHED_FIFO, &sp );
    
    // init random seed
    srand(time(NULL));

    // parse yaml config

    // forks, pipes, exec, cgroups, ...

    // while(1)
    
    // TEST
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

    Process* proc_ptr = frame.windows[0].slices[0].sc.get_current_proc();
    proc_ptr->exec();


    return 0;
}

