/* Cockos SWELL (Simple/Small Win32 Emulation Layer for Linux/OSX)
   Copyright (C) 2006 and later, Cockos, Inc.

    This software is provided 'as-is', without any express or implied
    warranty.  In no event will the authors be held liable for any damages
    arising from the use of this software.

    Permission is granted to anyone to use this software for any purpose,
    including commercial applications, and to alter it and redistribute it
    freely, subject to the following restrictions:

    1. The origin of this software must not be misrepresented; you must not
       claim that you wrote the original software. If you use this software
       in a product, an acknowledgment in the product documentation would be
       appreciated but is not required.
    2. Altered source versions must be plainly marked as such, and must not be
       misrepresented as being the original software.
    3. This notice may not be removed or altered from any source distribution.
*/
  
#ifdef SWELL_PROVIDED_BY_APP

#import <Cocoa/Cocoa.h>
#import <objc/objc-runtime.h>
#define SWELL_API_DEFPARM(x)
#define SWELL_API_DEFINE(ret,func,parms) ret (*func) parms ;
#include "swell.h"

// only include this file in projects that are linked to swell.dylib

struct SWELL_DialogResourceIndex *SWELL_curmodule_dialogresource_head;
struct SWELL_MenuResourceIndex *SWELL_curmodule_menuresource_head;

// define the functions

static struct
{
  const char *name;
  void **func;
} api_tab[]={
  
#undef _WDL_SWELL_H_API_DEFINED_
#undef SWELL_API_DEFINE
#define SWELL_API_DEFINE(ret, func, parms) {#func, (void **)&func },

#include "swell-functions.h"
  
};

static int dummyFunc() { return 0; }

class SwellAPPInitializer
{
public:
  SwellAPPInitializer()
  {
    void *(*SWELLAPI_GetFunc)(const char *name)=NULL;
    void *(*send_msg)(id, SEL) = (void *(*)(id, SEL))objc_msgSend;
    
    id del = [NSApp delegate];
    if (del && [del respondsToSelector:@selector(swellGetAPPAPIFunc)])
      *(void **)&SWELLAPI_GetFunc = send_msg(del,@selector(swellGetAPPAPIFunc));

    if (!SWELLAPI_GetFunc) NSLog(@"SWELL API provider not found\n");
    else if (SWELLAPI_GetFunc(NULL)!=(void*)0x100)
    {
      NSLog(@"SWELL API provider returned incorrect version\n");
      SWELLAPI_GetFunc=0;
    }
      
    int x;
    for (x = 0; x < sizeof(api_tab)/sizeof(api_tab[0]); x ++)
    {
      *api_tab[x].func=SWELLAPI_GetFunc?SWELLAPI_GetFunc(api_tab[x].name):0;
      if (!*api_tab[x].func)
      {
        if (SWELLAPI_GetFunc) NSLog(@"SWELL API not found: %s\n",api_tab[x].name);
        *api_tab[x].func = (void*)&dummyFunc;
      }
    }
  }
  ~SwellAPPInitializer()
  {
  }
};

SwellAPPInitializer m_swell_appAPIinit;

extern "C" __attribute__ ((visibility ("default"))) int SWELL_dllMain(HINSTANCE hInst, DWORD callMode, LPVOID _GetFunc)
{
  // this returning 1 allows DllMain to be called, if available
  return 1;
}

#endif
