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
  
#include "swell.h"


#ifndef SWELL_PROVIDED_BY_APP

// only add this file to your project if you are an application that wishes to publish the SWELL API to its modules/plugins
// the modules should be compiled using SWELL_PROVIDED_BY_APP and include swell-modstub-generic.cpp

#undef _WDL_SWELL_H_API_DEFINED_
#undef SWELL_API_DEFINE
#define SWELL_API_DEFINE(ret, func, parms) {#func, (void *)func },
static struct api_ent
{
  const char *name;
  void *func;
}
api_table[]=
{
#include "swell.h"
};

static int compfunc(const void *a, const void *b)
{
  return strcmp(((struct api_ent*)a)->name,((struct api_ent*)b)->name);
}

extern "C" {

__attribute__ ((visibility ("default"))) void *SWELLAPI_GetFunc(const char *name)
{
  if (!name) return (void *)0x100; // version
  static int a; 
  if (!a)
  { 
    a=1;
    qsort(api_table,sizeof(api_table)/sizeof(api_table[0]),sizeof(api_table[0]),compfunc);
  }
  struct api_ent find={name,NULL};
  struct api_ent *res=(struct api_ent *)bsearch(&find,api_table,sizeof(api_table)/sizeof(api_table[0]),sizeof(api_table[0]),compfunc);
  if (res) return res->func;
  return NULL;
}

};

#ifdef SWELL_MAKING_DYLIB
static INT_PTR (*s_AppMain)(int msg, INT_PTR parm1, INT_PTR parm2);
INT_PTR SWELLAppMain(int msg, INT_PTR parm1, INT_PTR parm2)
{
  // remove this code in 2019 or later
  static char chk;
  if (!s_AppMain && !chk)
  {
    chk=1;
    *(void **)&s_AppMain = dlsym(NULL,"SWELLAppMain");
    printf("libSwell: used legacy SWELLAppMain get to get %p\n",s_AppMain);
  }
  // end temp code

  if (s_AppMain) return s_AppMain(msg,parm1,parm2);
  return 0;
}

extern "C" {
__attribute__ ((visibility ("default"))) void SWELL_set_app_main(INT_PTR (*AppMain)(int msg, INT_PTR parm1, INT_PTR parm2))
{
  s_AppMain = AppMain;
}
};

#endif

#endif
