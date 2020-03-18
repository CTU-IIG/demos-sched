#include <iostream>
#include "demos-sch.h"
#include <unistd.h>
#include <err.h>

using namespace std;

int main(){

    if( demos_init() == -1)
        err(1,"demos_init");

    while(true){
        cout << "new_period" << endl;
        sleep(1);
        cout << "completed" << endl;
        demos_completed();
    }

    return 0;
}
