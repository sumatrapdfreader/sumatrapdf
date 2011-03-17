/* Copyright 2006-2011 the SumatraPDF project authors (see ../AUTHORS file).
   License: FreeBSD (see ./COPYING) */

#ifndef vstrlist_h
#define vstrlist_h

#include "Vec.h"
#include "TStrUtil.h"

class VStrList : public Vec<TCHAR *>
{
public:
    ~VStrList() {
        FreeVecMembers(*this);
    }

    TCHAR *Join(TCHAR *joint=NULL) {
        Vec<TCHAR> tmp(256, 1);
        size_t jointLen = joint ? Str::Len(joint) : 0;
        for (size_t i = 0; i < Count(); i++) {
            TCHAR *s = At(i);
            if (i > 0 && jointLen > 0)
                tmp.Append(joint, jointLen);
            tmp.Append(s, Str::Len(s));
        }
        return tmp.StealData();
    }

    int Find(TCHAR *string) const {
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
