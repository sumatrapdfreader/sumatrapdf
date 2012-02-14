/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "Mui.h"

namespace mui {

// we use uint16 for those
STATIC_ASSERT(Control::WantedInputBitLast < 16, max16bitsForWantedIntputBits);
STATIC_ASSERT(Control::StateBitLast < 16, max16bitsForStateBits);

Control::Control(Control *newParent)
{
    wantedInputBits = 0;
    stateBits = 0;
    zOrder = 0;
    parent = NULL;
    hwndParent = NULL;
    layout = NULL;
    hCursor = NULL;
    cachedStyle = NULL;
    SetCurrentStyle(NULL, gStyleDefault);
    pos = Rect();
    if (newParent)
        SetParent(newParent);
}

Control::~Control()
{
    delete layout;
    DeleteVecMembers(children);
    if (!parent)
        return;
    HwndWrapper *root = GetRootHwndWnd(parent);
    CrashIf(!root);
    UnRegisterEventHandlers(root->evtMgr);
}

void Control::SetParent(Control *newParent)
{
    HwndWrapper *prevRoot = NULL;
    if (parent)
        prevRoot = GetRootHwndWnd(parent);
    HwndWrapper *newRoot = GetRootHwndWnd(newParent);
    CrashIf(!newRoot);

    parent = newParent;

    if (prevRoot)
        UnRegisterEventHandlers(prevRoot->evtMgr);

    RegisterEventHandlers(newRoot->evtMgr);
}

Control *Control::GetChild(size_t idx) const
{
    return children.At(idx);
}

size_t Control::GetChildCount() const
{
    return children.Count();
}

bool Control::WantsMouseClick() const
{
    return bit::IsSet(wantedInputBits, WantsMouseClickBit);
}

bool Control::WantsMouseMove() const
{
    return bit::IsSet(wantedInputBits, WantsMouseMoveBit);
}

bool Control::IsMouseOver() const
{
    return bit::IsSet(stateBits, MouseOverBit);
}

bool Control::IsVisible() const
{
    return !bit::IsSet(stateBits, IsHiddenBit);
}

void Control::SetIsMouseOver(bool isOver)
{
    if (isOver)
        bit::Set(stateBits, MouseOverBit);
    else
        bit::Clear(stateBits, MouseOverBit);
}

void Control::AddChild(Control *wnd, int pos)
{
    CrashAlwaysIf(NULL == wnd);
    if ((pos < 0) || (pos >= (int)children.Count()))
        children.Append(wnd);
    else
        children.InsertAt(pos, wnd);
    wnd->SetParent(this);
}

void Control::AddChild(Control *wnd1, Control *wnd2, Control *wnd3)
{
    AddChild(wnd1);
    AddChild(wnd2);
    if (wnd3) AddChild(wnd3);
}

void Control::Measure(const Size availableSize)
{
    if (layout) {
        layout->Measure(availableSize, this);
    } else {
        desiredSize = Size();
    }
}

void Control::MeasureChildren(Size availableSize) const
{
    for (size_t i = 0; i < GetChildCount(); i++) {
        GetChild(i)->Measure(availableSize);
    }
}

void Control::Arrange(const Rect finalRect)
{
    if (layout) {
        layout->Arrange(finalRect, this);
    } else {
        SetPosition(finalRect);
    }
}

void Control::Show()
{
    if (IsVisible())
        return; // perf: avoid unnecessary repaints
    bit::Clear(stateBits, IsHiddenBit);
    RequestRepaint(this);
}

void Control::Hide()
{
    if (!IsVisible())
        return;
    RequestRepaint(this); // request repaint before hiding, to trigger repaint
    bit::Set(stateBits, IsHiddenBit);
}

void Control::SetPosition(const Rect& p)
{
    if (p.Equals(pos))
        return;  // perf optimization
    // when changing position we need to invalidate both
    // before and after position
    // TODO: not sure why I need this, but without it there
    // are drawing artifacts
    Rect p1(p); p1.Inflate(1,1);
    Rect p2(pos); p2.Inflate(1,1);
    RequestRepaint(this, &p1, &p2);
    pos = p;
}

// convert position (x,y) int coordiantes of root window
// to position in this window's coordinates
void Control::MapRootToMyPos(int& x, int& y) const
{
    int offX = pos.X;
    int offY = pos.Y;
    const Control *w = this;
    while (w->parent) {
        w = w->parent;
        offX += w->pos.X;
        offY += w->pos.Y;
    }
    x -= offX;
    y -= offY;
}

// Requests the window to draw itself on a Graphics canvas.
// offX and offY is a position of this window within
// Graphics canvas (pos is relative to that offset)
void Control::Paint(Graphics *gfx, int offX, int offY)
{
    if (!IsVisible())
        return;
}

void Control::SetCurrentStyle(Style *style1, Style *style2)
{
    cachedStyle = CacheStyle(style1, style2);
}

}
