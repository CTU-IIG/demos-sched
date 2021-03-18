#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    if (argc != 3) {
        printf("usage: ./inifinite_proc <period_usec> <print_msg>\n");
        exit(1);
    }

    int period = atoi(argv[1]);
    char *msg = argv[2];
    fprintf(stderr, "launched %s\n", argv[2]);

    while (true) {
        usleep(period);
        fprintf(stderr, "%s\n", msg);
    }
}
