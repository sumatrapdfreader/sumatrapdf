/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef _VSTRLIST_H_
#define _VSTRLIST_H_

#include "Vec.h"
#include "base_util.h"
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
        size_t len = 0;
        size_t jointLen = joint ? tstr_len(joint) : 0;
        TCHAR *result, *tmp;

        size_t n = Count();
        for (size_t i = n; i > 0; i--)
            len += tstr_len(At(i - 1)) + jointLen;
        if (len <= jointLen)
            return SAZ(TCHAR);
        len -= jointLen;

        result = SAZA(TCHAR, len + 1);
        if (!result)
            return NULL;

        assert(Count() == n);
        tmp = result;
        TCHAR *end = result + len + 1;
        for (size_t i = 0; i < n; i++) {
            if (jointLen > 0 && i > 0) {
                tstr_copy(tmp, end - tmp, joint);
                tmp += jointLen;
            }
            tstr_copy(tmp, end - tmp, At(i));
            tmp += tstr_len(At(i));
        }

        return result;
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
