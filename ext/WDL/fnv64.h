#ifndef _WDL_FNV64_H_
#define _WDL_FNV64_H_

#include "wdltypes.h"

#define WDL_FNV64_IV WDL_UINT64_CONST(0xCBF29CE484222325)

static WDL_UINT64 WDL_FNV64(WDL_UINT64 h, const unsigned char* data, int sz)
{
  int i;
  for (i=0; i < sz; ++i)
  {
    h *= WDL_UINT64_CONST(0x00000100000001B3);
    h ^= data[i];
  }
  return h;
}
#endif