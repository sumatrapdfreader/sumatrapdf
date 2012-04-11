/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "Mui.h"
#include "BitManip.h"

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
    SetStyle(NULL);
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

void Control::AddChild(Control *c, int pos)
{
    CrashIf(NULL == c);
    if ((pos < 0) || (pos >= (int)children.Count()))
        children.Append(c);
    else
        children.InsertAt(pos, c);
    c->SetParent(this);
}

void Control::AddChild(Control *c1, Control *c2, Control *c3)
{
    AddChild(c1);
    AddChild(c2);
    if (c3) AddChild(c3);
}

void Control::Measure(const Size availableSize)
{
    if (layout) {
        layout->Measure(availableSize);
    } else {
        desiredSize = Size();
    }
}

Size Control::DesiredSize()
{
    return desiredSize;
}

void Control::MeasureChildren(Size availableSize) const
{
    for (size_t i = 0; i < GetChildCount(); i++) {
        GetChild(i)->Measure(availableSize);
    }
}

void Control::Arrange(const Rect finalRect)
{
    SetPosition(finalRect);
    if (layout) {
        // might over-write position if our layout knows about us
        layout->Arrange(finalRect);
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
    bool sizeChanged = (p.Width != pos.Width) || (p.Height != pos.Height);
    // when changing position we need to invalidate both
    // before and after position
    // TODO: not sure why I need this, but without it there
    // are drawing artifacts
    Rect p1(p); p1.Inflate(1,1);
    Rect p2(pos); p2.Inflate(1,1);
    RequestRepaint(this, &p1, &p2);
    pos = p;
    if (!sizeChanged)
        return;
    HwndWrapper *hwnd = GetRootHwndWnd(this);
    hwnd->evtMgr->NotifySizeChanged(this, p.Width, p.Height);
}

// convert position (x,y) int coordiantes of root window
// to position in this window's coordinates
void Control::MapRootToMyPos(int& x, int& y) const
{
    int offX = pos.X;
    int offY = pos.Y;
    const Control *c = this;
    while (c->parent) {
        c = c->parent;
        offX += c->pos.X;
        offY += c->pos.Y;
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

void Control::SetStyle(Style *style)
{
    cachedStyle = CacheStyle(style);
}

}
