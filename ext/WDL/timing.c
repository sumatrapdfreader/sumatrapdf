#include "timing.h"

#ifdef TIMING

#include <stdio.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

#include "wdlcstring.h"

#if defined(__APPLE__) && defined(__OBJC__)
#import <Foundation/Foundation.h>
#endif

static struct {
	WDL_INT64 st_time;
	WDL_INT64 cycles,mint,maxt;
	WDL_INT64 calls;
} __wdl_timingInfo[64];
static int __wdl_timingInfo_reg;


static void __wdl_timingInfo_gettime(WDL_INT64 *t)
{
#ifdef _WIN64
#define TIMING_UNITS "QPC_UNITS"
  LARGE_INTEGER now;
  QueryPerformanceCounter(&now);
  *t = now.QuadPart;
#elif !defined(_WIN32)
#define TIMING_UNITS "uSec"
  struct timeval tm={0,};
  gettimeofday(&tm,NULL);
  *t = ((WDL_INT64)tm.tv_sec)*1000000 + (WDL_INT64)tm.tv_usec;
#else
#define TIMING_UNITS "cycles"
	__asm 
	{
		mov esi, t
		_emit 0xf
		_emit 0x31
		mov [esi], eax
		mov [esi+4], edx
	}
#endif
}

void _timingPrint(void)
{
  int x,p=0;
  for (x = 0; x < (int) (sizeof(__wdl_timingInfo)/sizeof(__wdl_timingInfo[0])); x ++)
  {
    char buf[512];
    if (__wdl_timingInfo[x].calls)
    {
      p++;
      snprintf(buf,sizeof(buf),"wdl_timing: %d: %.0f calls, %.0f " TIMING_UNITS "/call (min=%.0f, max=%.0f). %.0f " TIMING_UNITS " of CPU time spent\n",
        x,(double)__wdl_timingInfo[x].calls,(__wdl_timingInfo[x].cycles/(double)__wdl_timingInfo[x].calls),
        (double)__wdl_timingInfo[x].mint,(double)__wdl_timingInfo[x].maxt,
        (double)__wdl_timingInfo[x].cycles 
      );
#ifdef _WIN32
      OutputDebugStringA(buf);
#elif defined(__APPLE__) && defined(__OBJC__)
      NSLog(@"%s",buf);
#else
      printf("%s",buf);
#endif
    }
  }
  if (!p)
#ifdef _WIN32
  OutputDebugString(
#elif defined(__APPLE__) && defined(__OBJC__)
  NSLog(@
#else
  printf(
#endif
  "wdl_timing: no calls to timing lib\n");
}
	
void _timingEnter(int which)
{
  if (!__wdl_timingInfo_reg)
  {
    __wdl_timingInfo_reg=1;
    atexit(_timingPrint);
  }

  if (which >= 0 && which < (int) (sizeof(__wdl_timingInfo)/sizeof(__wdl_timingInfo[0])))
    __wdl_timingInfo_gettime(&__wdl_timingInfo[which].st_time);
}

void _timingLeave(int which)
{
  if (which >= 0 && which < (int) (sizeof(__wdl_timingInfo)/sizeof(__wdl_timingInfo[0])))
  {
    WDL_INT64 t;
    __wdl_timingInfo_gettime(&t);
    t -= __wdl_timingInfo[which].st_time;
    if (!__wdl_timingInfo[which].mint || t < __wdl_timingInfo[which].mint) __wdl_timingInfo[which].mint=t?t:1;
    if (t > __wdl_timingInfo[which].maxt) __wdl_timingInfo[which].maxt=t;
    __wdl_timingInfo[which].cycles += t;
    __wdl_timingInfo[which].calls += 1;
  }
}

WDL_INT64 _timingQuery(int which, WDL_INT64* cycles)
{
  if (__wdl_timingInfo[which].calls && cycles)
  {
    cycles[0] = __wdl_timingInfo[which].cycles;
    cycles[1] = __wdl_timingInfo[which].mint;
    cycles[2] = __wdl_timingInfo[which].maxt;
  }
  return __wdl_timingInfo[which].calls;
}

#undef TIMING_UNITS

#endif
