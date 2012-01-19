/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "mui.h"
#include "BaseUtil.h"
#include "Vec.h"

/*
MUI is a simple UI library for win32.
MUI stands for nothing, it's just ui and gui are overused.

MUI is intended to allow building UIs that have modern
capabilities not supported by the standard win32 HWND
architecture:
- overlapping, alpha-blended windows
- animations
- a saner layout

It's inspired by WPF, WDL (http://www.cockos.com/wdl/),
DirectUI (https://github.com/kjk/directui).

MUI is minimal - it only supports stuff needed for Sumatra.
I got burned trying to build the whole toolkit at once with DirectUI.
Less code there is, the easier it is to change or extend.

The basic architectures is that of a tree of "virtual" (not backed
by HWND) windows. Each window can have children (making it a container).
Children windows are positioned relative to its parent window and can
be positioned outside of parent's bounds.

There must be a parent window backed by HWND which handles windows
messages and paints child windows on WM_PAINT.
*/

#include "mui.h"

namespace mui {

VirtWnd::VirtWnd(VirtWnd *parent)
{
    SetParent(parent);
    isVisible = true;
    pos = RectF();
}

VirtWnd::~VirtWnd()
{

}

// traverse tree upwards to find HWND that is ultimately backing
// this window
HWND VirtWnd::GetHwndParent() const
{
    const VirtWnd *curr = this;
    while (curr) {
        if (curr->hwndParent)
            return curr->hwndParent;
        curr = curr->parent;
    }
    return NULL;
}

void VirtWnd::AddChild(VirtWnd *wnd, int pos)
{
    CrashAlwaysIf(NULL == wnd);
    if ((pos < 0) || (pos >= (int)children.Count()))
        children.Append(wnd);
    else
        children.InsertAt(pos, wnd);
    wnd->SetParent(this);
}

// Requests the window to draw itself on a Graphics canvas.
// offX and offY is a position of this window within
// Graphics canvas (pos is relative to that offset)
void VirtWnd::Draw(Graphics *gfx, float offX, float offY) const
{
    if (!isVisible)
        return;
}

// Should be called from WM_PAINT. Recursively paints a given window and
// all its children. VirtWnd must be the top-level window associated
// with HWND.
void VirtWndPainter::OnPaint(HWND hwnd, VirtWnd *hwndWnd)
{
    CrashAlwaysIf(hwnd != hwndWnd->hwndParent);

    PAINTSTRUCT ps;
    HDC dc = BeginPaint(hwnd, &ps);

    EndPaint(hwnd, &ps);
}

}

