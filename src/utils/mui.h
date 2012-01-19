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

    void AddChild(VirtWnd *wnd, int pos);

    size_t GetChildCount() const {
        return children.Count();
    }

    HWND GetHwndParent() const;

    virtual void Draw(Graphics *gfx, float offX, float offY) const ;

    VirtWnd *       parent;
    Vec<VirtWnd*>   children;

    // can be backed by a HWND
    HWND            hwndParent;

    // position and size (relative to parent, might be outside of parent's bounds)
    RectF           pos;

    bool            isVisible;
};

// Manages painting process of VirtWnd window and all its children.
// Automatically does double-buffering for less flicker.
// Create one object for each HWND-backed VirtWnd and keep it around.
class VirtWndPainter
{
    // bitmap for double-buffering
    Bitmap *cacheBmp;

public:
    VirtWndPainter() : cacheBmp(NULL)
    {
    }

    void OnPaint(HWND hwnd, VirtWnd *hwndWnd);
};

}

#endif
