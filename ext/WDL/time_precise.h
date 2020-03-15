#ifndef _WDL_TIME_PRECISE_H_
#define _WDL_TIME_PRECISE_H_

#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include <sys/time.h>
#else
#include <time.h>
#endif

#include "wdltypes.h"


#ifdef WDL_TIME_PRECISE_DECL
WDL_TIME_PRECISE_DECL
#else
static WDL_STATICFUNC_UNUSED
#endif

double time_precise()
{
#ifdef _WIN32
  LARGE_INTEGER freq;
  QueryPerformanceFrequency(&freq);

  LARGE_INTEGER now;
  QueryPerformanceCounter(&now);
  return (double)now.QuadPart / (double)freq.QuadPart;
#elif defined(__APPLE__)
  struct timeval tm={0,};
  gettimeofday(&tm,NULL);
  return (double)tm.tv_sec + (double)tm.tv_usec/1000000;
#else
  struct timespec tm={0,};
  clock_gettime(CLOCK_MONOTONIC,&tm);
  return (double)tm.tv_sec + (double)tm.tv_nsec/1000000000;
#endif
}

#endif
