#include "measure-time.h"

void calc_diff(struct timespec *smaller, struct timespec *bigger, struct timespec *diff)
{
    if (smaller->tv_nsec > bigger->tv_nsec)
    {
        diff->tv_nsec = 1000000000 + bigger->tv_nsec - smaller->tv_nsec;
        diff->tv_sec = bigger->tv_sec - 1 - smaller->tv_sec;
    }
    else 
    {
        diff->tv_nsec = bigger->tv_nsec - smaller->tv_nsec;
        diff->tv_sec = bigger->tv_sec - smaller->tv_sec;
    }
}

