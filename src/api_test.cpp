#include <iostream>
#include "demos-sch.h"
#include <unistd.h>
#include <err.h>
#include "evfd.hpp"

using namespace std;

int main(){

    int fd_per = demos_init();
    if(fd_per == -1)
        err(1,"demos_init");

    ev::default_loop loop;
    ev::evfd efd(loop,fd_per);

    efd.set([&]() {
        cout<<"new_period"<<endl;
        sleep(1);
        demos_completed();
        cout<<"completed"<<endl;
        efd.start();
    });

    efd.start();
    loop.run();

//    while(true){
//        sleep(1);
//        demos_completed();
//    }

    return 0;
}
