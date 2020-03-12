#include <iostream>
#include "evfd.hpp"
#include "err.h"
#include <sys/types.h>
#include <unistd.h>

using namespace std;

int main(){
    ev::default_loop loop;
    ev::evfd efd(loop);
    efd.set([&]() {
       cout << "read " << efd.get_value() << " from efd" << endl;
    });

    switch(fork()) {
    case -1:
        err(1,"fork");
    case 0:{
        sleep(1);
        uint64_t buf = 1;
        printf("Child writing %lu to efd\n", buf);

        ssize_t s = write(efd.get_fd(), &buf, sizeof(uint64_t));
        if (s != sizeof(uint64_t))
            err(1,"write");
        break;}
    default:{
        efd.start();
        loop.run();
        }
    }

    return 0;
}

