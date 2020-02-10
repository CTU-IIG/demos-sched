#include <iostream>
#include "timerfd.hpp"

using namespace std;
using namespace ev;

void tim_cb(timer &t, int revents)
{
    cout << "ev::timer" << endl;
}

void io_cb(io &t, int revents)
{
    cout << "ev::io" << endl;
}

int main(int argc, char *argv[])
{
    default_loop loop;
    timerfd tfd(loop);
    timer tim(loop);
    io i(loop);

    i.set<io_cb>();
    i.start(0, ev::READ);

    tfd.set([](){cout << "timerfd expired" << endl;});
    tfd.start(chrono::steady_clock::now() + 1s);

    tim.set<tim_cb>();
    tim.start(0.1);

    loop.run(ev::ONCE);

    tfd.stop();
    i.stop();
    tim.start(2);

    loop.run();

    return 0;
}
