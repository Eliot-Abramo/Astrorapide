#include <stdint.h>
#include <time.h>
#include "util.hpp"

uint64_t CalcTimeDiff(const struct timespec & time2, const struct timespec & time1)
{
  return time2.tv_sec == time1.tv_sec ?
    time2.tv_nsec - time1.tv_nsec :
    (time2.tv_sec - time1.tv_sec - 1) * 1e9 + (1e9 - time1.tv_nsec) + time2.tv_nsec;
}

