/*
    WDL - filename.h
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

#ifndef _WDL_FILENAME_H_
#define _WDL_FILENAME_H_

#include "wdltypes.h"

static WDL_STATICFUNC_UNUSED char WDL_filename_filterchar(char p, char repl='_', bool filterSlashes=true)
{
  if (p == '?'  || 
      p == '*'  ||
      p == ':'  ||
      p == '\"' ||
      p == '|'  ||
      p == '<'  || 
      p == '>') 
  {
    return repl;
  }

  if (filterSlashes && (p == '/' || p == '\\' ))
  {
    return repl;
  }

  return p;
}

static WDL_STATICFUNC_UNUSED void WDL_filename_filterstr(char *rd, char repl='_', int path_filter_mode=1)
{
  char *wr, lc = WDL_DIRCHAR;
  // path_filter_mode:
  //   0 = remove leading slashes, consolidate duplicate slashes (use when filtering the filepart but allowing subdirectories)
  //  >0 = filter slashes (use when filtering the filepart, disallowing subdirectories)
  //  <0 = allow absolute paths, consolidate duplicate slashes (filtering a full path)
  if (path_filter_mode<0)
  {
    #ifdef _WIN32
      if (rd[0] && rd[1] == ':' && WDL_IS_DIRCHAR(rd[2])) rd += 3;
      else if (WDL_IS_DIRCHAR(rd[0]) && WDL_IS_DIRCHAR(rd[1])) rd += 2;
    #else
      if (WDL_IS_DIRCHAR(*rd)) rd++;
    #endif
  }
  wr = rd;
  while (*rd)
  {
    char r=WDL_filename_filterchar(*rd++,repl,path_filter_mode>0);
    if (!r || (WDL_IS_DIRCHAR(r) && WDL_IS_DIRCHAR(lc))) continue; // filter multiple slashes in a row, or leading slash
    *wr++ = lc = r;
  }
  *wr=0;
}



#endif // _WDL_FILENAME_H_
