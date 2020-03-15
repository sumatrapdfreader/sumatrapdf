#ifndef _WDL_WIN32_PRINTF_H_
#define _WDL_WIN32_PRINTF_H_

#include <windows.h>
#ifdef printf
#undef printf
#endif
#define printf wdl_printf

// this file is designed to be temporarily included when printf() debugging on win32


static void wdl_printf(const char *format, ...)
{
  char tmp[3800];
  int rv;
  va_list va;
  va_start(va,format);
  tmp[0]=0;
  rv=_vsnprintf(tmp,sizeof(tmp),format,va); // returns -1  if over, and does not null terminate, ugh
  va_end(va);

  if (rv < 0 || rv>=(int)sizeof(tmp)-1) tmp[sizeof(tmp)-1]=0; 
  OutputDebugStringA(tmp);
}

#endif
