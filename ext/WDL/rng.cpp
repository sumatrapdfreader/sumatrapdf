/*
    WDL - rng.cpp
    Copyright (C) 2005 and later, Cockos Incorporated
   
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

/*

  This file provides the implementation of a decent random number generator, 
  that internally uses a 256-bit state, and SHA-1 to iterate.

*/

#ifdef _WIN32
#include <windows.h>
#else
#include <stdlib.h>
#include <memory.h>
#include <string.h>
#endif


#include "rng.h"
#include "sha.h"

static unsigned char state[32];

void WDL_RNG_addentropy(void *buf, int buflen)
{
  WDL_SHA1 tmp;
  tmp.add(state,sizeof(state));
  tmp.result(state);
  tmp.reset();
  tmp.add((unsigned char *)buf,buflen);
  tmp.result(state+sizeof(state) - WDL_SHA1SIZE);
}

static void rngcycle()
{
  int i;
  for (i = 0; i < (int)sizeof(state) && state[i]++; i++);
}

int WDL_RNG_int32()
{
  WDL_SHA1 tmp;
  tmp.add(state,sizeof(state));
  rngcycle();
  union { 
    char buf[WDL_SHA1SIZE];
    int a;
  } b;
  tmp.result(b.buf);
  return b.a;

}


void WDL_RNG_bytes(void *buf, int buflen)
{
  char *b=(char *)buf;
  while (buflen > 0)
  {
    char tb[WDL_SHA1SIZE];
    WDL_SHA1 tmp;
    tmp.add(state,sizeof(state));
    rngcycle();

    tmp.result(tb);
    const int l=buflen < WDL_SHA1SIZE ? buflen : WDL_SHA1SIZE;
    memcpy(b,tb,l);
    buflen-=l;
    b+=l;
  }
}

