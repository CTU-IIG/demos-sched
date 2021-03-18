#include "tests/acutest.h"

#include "timerfd.hpp"
#include <iostream>

using namespace std;
using namespace ev;

static void test()
{
    default_loop loop;
    timerfd tfd(loop);

    bool expired = false;
    tfd.set([&]() {
        cout << "timerfd expired" << endl;
        expired = true;
    });
    tfd.start(chrono::steady_clock::now() + 0s);

    loop.run();

    TEST_CHECK(expired);
}

TEST_LIST = { { "timerfd", test }, { 0 } };
