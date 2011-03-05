/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef _VSTRLIST_H_
#define _VSTRLIST_H_

#include "Vec.h"
#include "tstr_util.h"

class VStrList : public Vec<TCHAR *>
{
public:
    ~VStrList() {
        for (size_t i = 0; i < Count(); i++)
            free(At(i));
    }

    TCHAR *Join(TCHAR *joint=NULL)
    {
        Vec<TCHAR> tmp(256, 1);
        size_t jointLen = joint ? tstr_len(joint) : 0;
        for (size_t i=0; i<Count(); i++)
        {
            TCHAR *s = At(i);
            if (i > 0 && jointLen > 0)
                tmp.Append(joint, jointLen);
            tmp.Append(s, tstr_len(s));
        }
        return tmp.StealData();
    }

    int Find(TCHAR *string)
    {
        size_t n = Count();
        for (size_t i = 0; i < n; i++) {
            TCHAR *item = At(i);
            if (tstr_eq(string, item))
                return (int)i;
        }
        return -1;
    }
};

#endif
