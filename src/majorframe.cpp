#include "majorframe.hpp"

MajorFrame::MajorFrame(ev::loop_ref loop, Windows &windows)
    : loop(loop)
    , windows( windows )
    , current( this->windows.begin() )
    , timer(loop)
    , sigint(loop)
{
    timer.set(std::bind(&MajorFrame::timeout_cb, this));
    sigint.set<MajorFrame, &MajorFrame::sigint_cb>(this);
    sigint.start(SIGINT);
}

void MajorFrame::kill_all()
{
    for(Window &w : windows){
        for(Slice &s : w.slices){
            s.stop(); // unregister timer
            for(Process &p : s.sc.processes){
                p.kill();
            }
            for(Process &p : s.be.processes){
                p.kill();
            }
        }
    }
}

MajorFrame::~MajorFrame()
{
#ifdef VERBOSE
    std::cerr<< __PRETTY_FUNCTION__ <<std::endl;
#endif
    kill_all();
    // wait for all process cgroups to be removed
    loop.run();

}

void MajorFrame::move_to_next_window()
{
    if( ++current == windows.end() )
        current = windows.begin();
}

Window &MajorFrame::get_current_window()
{
    return *current;
}

void MajorFrame::timeout_cb()
{
    // TODO do all window switching stuff

    std::cerr<< "window timeout" << std::endl;
}

void MajorFrame::sigint_cb(ev::sig &w, int revents)
{
    w.stop();
    //loop.break_loop();
    throw std::system_error(0, std::generic_category(), "demos-sched killed by signal");
}
