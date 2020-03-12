#include <iostream>
#include "demos-sch.h"
#include <unistd.h>

using namespace std;

int main(){

    while(true){
        sleep(1);
        demos_completed();
    }

    return 0;
}
