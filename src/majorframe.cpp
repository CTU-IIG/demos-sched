#include "majorframe.hpp"

using namespace std;

MajorFrame::MajorFrame(ev::loop_ref loop, std::chrono::steady_clock::time_point start_time, Windows &windows)
    : loop(loop)
    , windows( windows )
    , current( this->windows.begin() )
    , timer(loop)
    , sigint(loop)
    , timeout( start_time )
{
    timer.set(bind(&MajorFrame::timeout_cb, this));
//    sigint.set<MajorFrame, &MajorFrame::sigint_cb>(this);
//    sigint.start(SIGINT);
}

//void MajorFrame::kill_all()
//{
//    for(Window &w : windows){
//        for(Slice &s : w.slices){
//            s.stop(); // unregister timer
//            for(Process &p : s.sc.processes){
//                p.kill();
//            }
//            for(Process &p : s.be.processes){
//                p.kill();
//            }
//            s.sc.unfreeze();
//            s.be.unfreeze();
//        }
//    }
//}

//MajorFrame::~MajorFrame()
//{
//#ifdef VERBOSE
//    std::cerr<< __PRETTY_FUNCTION__ <<std::endl;
//#endif
//    timer.stop();
//    kill_all();
//    // wait for all process cgroups to be removed
//    loop.run();

//}

void MajorFrame::move_to_next_window()
{
    if( ++current == windows.end() )
        current = windows.begin();
}

Window &MajorFrame::get_current_window()
{
    return *current;
}

void MajorFrame::start()
{
    current->update_timeout(timeout);
    current->start();
    timeout += current->length;
    timer.start(timeout);

}

void MajorFrame::stop()
{
    current->stop();
}

void MajorFrame::timeout_cb()
{
    // TODO do all window switching stuff
#ifdef VERBOSE
    cerr << __PRETTY_FUNCTION__ << endl;
#endif

    current->stop();
    move_to_next_window();
    current->update_timeout(timeout);
    current->start();
    timeout += current->length;
    timer.start(timeout);
}

void MajorFrame::sigint_cb(ev::sig &w, int revents)
{
    w.stop();
    //loop.break_loop();
    throw std::system_error(0, std::generic_category(), "demos-sched killed by signal");
}
