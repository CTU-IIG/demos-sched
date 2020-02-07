#include "majorframe.hpp"

MajorFrame::MajorFrame(ev::loop_ref loop, Windows &windows)
    : loop(loop)
    , windows( windows )
    , current( this->windows.begin() )
    , timer(loop)
{
    timer.set(std::bind(&MajorFrame::timeout_cb, this));
}

MajorFrame::~MajorFrame()
{
    std::cerr<< __PRETTY_FUNCTION__ <<std::endl;
    for(Window &w : windows){
        for(Slice &s : w.slices){
            for(Process &p : s.sc.processes){
                p.kill();
            }
            for(Process &p : s.be.processes){
                p.kill();
            }
        }
    }
    // wait for all cgroups to be removed
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
