#include "err.h"
#include "evfd.hpp"
#include <iostream>
#include <unistd.h>

using namespace std;

int main()
{
    ev::default_loop loop;
    ev::evfd efd(loop);
    efd.set([&](ev::evfd &w) {
        cout << "read from efd" << endl;
        w.stop();
    });

    switch (fork()) {
        case -1: {
            err(1, "fork");
        }
        case 0: {
            uint64_t buf = 1;
            cout << "Writing to efd" << endl;
            efd.write(buf);
            break;
        }
        default: {
            efd.start();
            loop.run();
        }
    }

    return 0;
}
