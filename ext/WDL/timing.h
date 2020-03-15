/*
  WDL - timing.h

  this is based on some public domain Pentium RDTSC timing code from usenet in 1996.

  To enable this, your app must #define TIMING, include timing.h, and call timingEnter(x)/timingLeave(x) a bunch
  of times (where x is 0..63).

*/

#ifndef _TIMING_H_
#define _TIMING_H_


//#define TIMING


#include "wdltypes.h"

#if defined(TIMING)
#ifdef __cplusplus
extern "C" {
#endif
void _timingEnter(int);
void _timingLeave(int);
WDL_INT64 _timingQuery(int, WDL_INT64*);
#ifdef __cplusplus
}
#endif
#define timingLeave(x) _timingLeave(x)
#define timingEnter(x) _timingEnter(x)
#define timingQuery(x,y) _timingQuery(x,y)
#else
#define timingLeave(x)
#define timingEnter(x)
#define timingQuery(x,y) (0)
#endif

#define timingPrint()
#define timingInit()

#endif
