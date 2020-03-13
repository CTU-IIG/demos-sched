#include "demos-sch.h"
#include "demossched.hpp"
#include <stdlib.h>
#include <unistd.h>
#include <err.h>

using namespace std;

static int fd_completed;

int demos_init()
{
    char *fd1_str = getenv("COMPLETED_EFD");
    char *fd2_str = getenv("NEW_PERIOD_EFD");

    if(!fd1_str || !fd2_str)
        return -1;

    fd_completed = atoi(fd1_str);
    int fd_new_period = atoi(fd2_str);

    return fd_new_period;
}

int demos_completed()
{
    uint64_t buf = 1;
    if( write(fd_completed, &buf, sizeof(buf)) == -1 )
        err(1,"write");

    return 0;
}
