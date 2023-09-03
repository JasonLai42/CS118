#include "helperfunc.hpp"
#include "constants.h"
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>


unsigned short gen_seq() {
    struct timeval seed;
    gettimeofday(&seed, NULL);
    srand((double)seed.tv_sec + ((double)seed.tv_usec / 1000000) + getpid());

    return (unsigned short)(rand() % MAX_SEQNUM);
}

unsigned short wrap_seq_ack(unsigned short num) {
    unsigned short wrapped_num = (unsigned short)(num % MAX_SEQNUM);
    
    return wrapped_num;
}

double start_timer() {
    struct timeval start;
    gettimeofday(&start, NULL);
    double s = (double)start.tv_sec + ((double)start.tv_usec / 1000000);

    return s;
}

double get_elapsed(double s) {
    struct timeval end;
    gettimeofday(&end, NULL);
    double e = (double)end.tv_sec + ((double)end.tv_usec / 1000000);

    return e - s;
}