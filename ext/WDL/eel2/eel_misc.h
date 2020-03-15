#ifndef _EEL_MISC_H_
#define _EEL_MISC_H_


#ifndef _WIN32
#include <sys/time.h>
#endif
#include <time.h>
// some generic EEL functions for things like time

#ifndef EEL_MISC_NO_SLEEP
static EEL_F NSEEL_CGEN_CALL _eel_sleep(void *opaque, EEL_F *amt)
{
  if (*amt >= 0.0) 
  {
  #ifdef _WIN32
    if (*amt > 30000000.0) Sleep(30000000);
    else Sleep((DWORD)(*amt+0.5));
  #else
    if (*amt > 30000000.0) usleep(((useconds_t)30000000)*1000);
    else usleep((useconds_t)(*amt*1000.0+0.5));
  #endif
  }
  return 0.0;
}
#endif

static EEL_F * NSEEL_CGEN_CALL _eel_time(void *opaque, EEL_F *v)
{
  *v = (EEL_F) time(NULL);
  return v;
}

static EEL_F * NSEEL_CGEN_CALL _eel_time_precise(void *opaque, EEL_F *v)
{
#ifdef _WIN32
  LARGE_INTEGER freq,now;
  QueryPerformanceFrequency(&freq);
  QueryPerformanceCounter(&now);
  *v = (double)now.QuadPart / (double)freq.QuadPart;
  // *v = (EEL_F)timeGetTime() * 0.001;
#else
  struct timeval tm={0,};
  gettimeofday(&tm,NULL);
  *v = tm.tv_sec + tm.tv_usec*0.000001;
#endif
  return v;
}


void EEL_misc_register()
{
#ifndef EEL_MISC_NO_SLEEP
  NSEEL_addfunc_retval("sleep",1,NSEEL_PProc_THIS,&_eel_sleep);
#endif
  NSEEL_addfunc_retptr("time",1,NSEEL_PProc_THIS,&_eel_time);
  NSEEL_addfunc_retptr("time_precise",1,NSEEL_PProc_THIS,&_eel_time_precise);
}

#ifdef EEL_WANT_DOCUMENTATION
static const char *eel_misc_function_reference =
#ifndef EEL_MISC_NO_SLEEP
  "sleep\tms\tYields the CPU for the millisecond count specified, calling Sleep() on Windows or usleep() on other platforms.\0"
#endif
  "time\t[&val]\tSets the parameter (or a temporary buffer if omitted) to the number of seconds since January 1, 1970, and returns a reference to that value. "
  "The granularity of the value returned is 1 second.\0"
  "time_precise\t[&val]\tSets the parameter (or a temporary buffer if omitted) to a system-local timestamp in seconds, and returns a reference to that value. "
  "The granularity of the value returned is system defined (but generally significantly smaller than one second).\0"
;
#endif


#endif
