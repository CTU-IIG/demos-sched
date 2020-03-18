#include <iostream>
#include "evfd.hpp"
#include "err.h"
#include <sys/types.h>
#include <unistd.h>

using namespace std;

int main(){
    ev::default_loop loop;
    ev::evfd efd(loop);
    efd.set([&](ev::evfd* w, uint64_t val) {
       cout << "read " << val << " from efd" << endl;
       w->stop();
    });

    switch(fork()) {
    case -1:
        err(1,"fork");
    case 0:{
        uint64_t buf = 1;
        printf("Child writing %lu to efd\n", buf);
        efd.write(buf);
        break;}
    default:{
        efd.start();
        loop.run();
        }
    }

    return 0;
}

