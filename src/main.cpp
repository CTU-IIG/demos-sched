#include <iostream>
#include <sched.h>
#include <vector>
#include <unistd.h>
#include <time.h>

using namespace std;

// what should be used as time_type?
typedef double time_type;

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

        time_type get_budget()
        {
            return budget - budget_jitter/2 + budget_jitter*((time_type) rand()/RAND_MAX);
        }
        
    private:
        vector<string> argv;
        time_type budget;
        time_type budget_jitter;
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

        Process* current_proc()
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

class Slice
{
    public:
        Partition sc;
        Partition be;

};

//typedef bitset... Cpu;

//class Window
//class MajorFrame

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
    
    // tests
    vector<string> proc_argv = {"/bin/echo","hello","world"};
    Process procA(proc_argv,10,2);
    cout<< procA.get_budget() <<endl;

    cout<< procA.exec() <<endl;


    return 0;
}

