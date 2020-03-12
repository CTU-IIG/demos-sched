#include "demos-sch.h"
#include "demossched.hpp"
#include <stdlib.h>
#include <unistd.h>
#include <err.h>

using namespace std;

int demos_init()
{
    return 0;
}

int demos_completed()
{
    char *fd_str = getenv("COMPLETED_EFD");
    if(!fd_str)
        err(1,"getenv");
    int fd = atoi(fd_str);
    uint64_t buf = 1;
    if( write(fd, &buf, sizeof(buf)) == -1 )
        err(1,"write");

    return 0;
}
