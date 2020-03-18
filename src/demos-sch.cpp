#include "demos-sch.h"
#include "demossched.hpp"
#include <stdlib.h>
#include <unistd.h>
#include <err.h>

static int fd_completed;

int demos_init()
{
    char *str = getenv("DEMOS_EFD");
    if( !str )
        return -1;

    int fd_new_period;
    if( sscanf( str, "%d,%d", &fd_completed, &fd_new_period) != 2 )
        return -1;

    return fd_new_period;
}

int demos_completed()
{
    uint64_t buf = 1;
    if( write(fd_completed, &buf, sizeof(buf)) == -1 )
        err(1,"write");

    return 0;
}
