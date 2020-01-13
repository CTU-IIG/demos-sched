#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    if( argc != 3){
        printf("usage: ./inifinite_proc <period_usec> <print_msg>\n");
        exit(1);
    }
    
    int period = atoi(argv[1]);
    char *msg = argv[2];
    printf("launched %s\n", argv[2]);
    
    while(1){
        usleep(period);
        printf("%s\n", msg);
    }
    return 0;
}
