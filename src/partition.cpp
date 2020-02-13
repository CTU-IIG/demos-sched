#include "partition.hpp"
#include <algorithm>
#include <string.h>

using namespace std::placeholders;
using namespace std;

Partition::Partition(ev::loop_ref loop,
                     std::string freezer_path,
                     std::string cpuset_path,
                     std::string events_path,
                     std::string name)
    : cgf( freezer_path, name )
    , cgc( cpuset_path, name )
    , cge( events_path, name)
    , current(nullptr)
    , name(name)
{
#ifdef VERBOSE
    std::cerr<<__PRETTY_FUNCTION__<<" "<<name<<std::endl;
#endif
    freeze();
}

Process &Partition::get_current_proc()
{
    return *current;
}

void Partition::freeze()
{
    for( Process &p : processes)
        p.freeze();
}

void Partition::unfreeze()
{
    for( Process &p : processes)
        p.unfreeze();
}

void Partition::add_process(ev::loop_ref loop,
                            std::vector<char *> argv,
                            chrono::nanoseconds budget,
                            chrono::nanoseconds budget_jitter)
{
    //std::cerr<<__PRETTY_FUNCTION__<<" "<<name<<std::endl;

    // get process name, after the last / in argv[0]
//    char *token, *cmd_name, *saveptr;
//    for ( char* cmd = argv[0];; cmd = nullptr) {
//        token = strtok_r(cmd,"/",&saveptr);
//        if( token == nullptr )
//            break;
//        cmd_name = token;
//    }

    //cerr<< cmd_name <<endl;

    processes.emplace_back( loop, "todo", *this,
                            argv, budget, budget_jitter);
    current = processes.begin();
    current->exec();
}

// cyclic queue
void Partition::move_to_next_proc()
{
    if( ++current == processes.end() )
        current = processes.begin();
}

void Partition::proc_exit_cb(Process &proc)
{
#ifdef VERBOSE
    cerr<< __PRETTY_FUNCTION__ << " " << name << endl;
#endif

    // find proc in processes and clear it
    //auto it = find(processes.begin(), processes.end(), proc);

    for(Processes::iterator it = processes.begin(); it != processes.end(); it++){
        if( &proc == &(*it) ){
            processes.erase(it);
            return;
        }
    }

    throw std::system_error(1, std::generic_category(), std::string(__PRETTY_FUNCTION__) + ": cannot find process" );
}
