/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef mui_h
#define mui_h

#include "BaseUtil.h"
#include "Vec.h"

namespace mui {

using namespace Gdiplus;
#include "GdiPlusUtil.h"

class VirtWnd 
{
public:
    VirtWnd(VirtWnd *parent=NULL);
    virtual ~VirtWnd();

    void SetParent(VirtWnd *parent) {
        this->parent = parent;
    }

    size_t GetChildCount() const {
        return children.Count();
    }

    HWND GetHwndParent() const;

    VirtWnd *       parent;
    Vec<VirtWnd*>   children;

    // can be backed by a HWND
    HWND            hwndParent;

    // position and size (relative to parent, might be outside of parent's bounds)
    RectF           pos;

};

}

#endif
