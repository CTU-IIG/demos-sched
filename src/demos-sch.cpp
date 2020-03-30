#include "demos-sch.h"
#include "demossched.hpp"
#include <stdlib.h>
#include <unistd.h>

static int fd_completed, fd_new_period;

int demos_init()
{
    char *str = getenv("DEMOS_FDS");
    if( !str )
        return -1;

    if( sscanf( str, "%d,%d", &fd_completed, &fd_new_period) != 2 )
        return -1;

    return 0;
}

int demos_completed()
{
    uint64_t buf = 1;
    // notify demos
    if( write(fd_completed, &buf, sizeof(buf)) == -1 )
        return -1;

    // block until become readable
    if( read(fd_new_period, &buf, sizeof(buf)) == -1 )
        return -1;

    return 0;
}
