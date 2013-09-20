/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "Mui.h"
#include "BitManip.h"

namespace mui {

static HWND gHwndControlTooltip = NULL;

static void CreateInfotipForLink(HWND hwndParent, const WCHAR *url, RECT pos)
{
    if (gHwndControlTooltip)
        return;

    HINSTANCE hinst =  GetModuleHandle(NULL);
    gHwndControlTooltip = CreateWindowEx(WS_EX_TOPMOST,
        TOOLTIPS_CLASS, NULL, WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        hwndParent, NULL, hinst, NULL);

    TOOLINFO ti = { 0 };
    ti.cbSize = sizeof(ti);
    ti.hwnd = hwndParent;
    ti.uFlags = TTF_SUBCLASS;
    ti.lpszText = (WCHAR *)url;
    ti.rect = pos;

    SendMessage(gHwndControlTooltip, TTM_ADDTOOL, 0, (LPARAM)&ti);
}

static void ClearInfotip(HWND hwndParent)
{
    if (!gHwndControlTooltip)
        return;

    TOOLINFO ti = { 0 };
    ti.cbSize = sizeof(ti);
    ti.hwnd = hwndParent;

    SendMessage(gHwndControlTooltip, TTM_DELTOOL, 0, (LPARAM)&ti);
    DestroyWindow(gHwndControlTooltip);
    gHwndControlTooltip = NULL;
}

// we use uint16 for those
STATIC_ASSERT(Control::WantedInputBitLast < 16, max16bitsForWantedIntputBits);
STATIC_ASSERT(Control::StateBitLast < 16, max16bitsForStateBits);

Control::Control(Control *newParent)
{
    wantedInputBits = 0;
    stateBits = 0;
    zOrder = 0;
    toolTip = NULL;
    parent = NULL;
    hwndParent = NULL;
    layout = NULL;
    hCursor = NULL;
    cachedStyle = NULL;
    namedEventClick = NULL;
    SetStyle(NULL);
    pos = Rect();
    if (newParent)
        SetParent(newParent);
}

void Control::SetToolTip(const WCHAR *toolTip)
{
    str::ReplacePtr(&this->toolTip, toolTip);
    if (NULL == toolTip)
        wantedInputBits &= WantsMouseOverBit;
    else
        wantedInputBits |= WantsMouseOverBit;
}

void Control::SetNamedEventClick(const char *s)
{
    str::ReplacePtr(&this->namedEventClick, s);
}

// note: all derived classes must call Control::NotifyMouseEnter()
// from their own NotifyMouseEnter().
void Control::NotifyMouseEnter()
{
    // show url as a tooltip
    HwndWrapper *hw = GetRootHwndWnd(this);
    HWND hwndParent = hw->hwndParent;
    int x = 0, y = 0;
    MapMyToRootPos(x, y);
    RECT pos = { x, y, 0, 0 };
    pos.right = x + this->pos.Width;
    pos.bottom = y + this->pos.Height;
    CreateInfotipForLink(hwndParent, toolTip, pos);
}

// note: all derived classes must call Control::NotifyMouseLeave()
// from their own NotifyMouseLeave().
void Control::NotifyMouseLeave()
{
    // hide url tooltip
    HwndWrapper *hw = GetRootHwndWnd(this);
    HWND hwndParent = hw->hwndParent;
    ClearInfotip(hwndParent);
}

Control::~Control()
{
    delete layout;
    DeleteVecMembers(children);
    free(toolTip);
    free((void*)namedEventClick);
}

void Control::SetParent(Control *newParent)
{
    parent = newParent;
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

Size Control::Measure(const Size availableSize)
{
    if (layout) {
        return layout->Measure(availableSize);
    }
    if (children.Count() == 1) {
        ILayout *l = children.At(0);
        return l->Measure(availableSize);
    }
    desiredSize = Size();
    return desiredSize;
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
    } else {
        if (children.Count() == 1) {
            ILayout *l = children.At(0);
            l->Arrange(finalRect);
        }
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

void Control::MapMyToRootPos(int& x, int& y) const
{
    // calculate the offset of window w within its root window
    x += pos.X;
    y += pos.Y;
    const Control *c = this;
    if (c->parent)
        c = c->parent;
    while (c && !c->hwndParent) {
        x += c->pos.X;
        y += c->pos.Y;
        c = c->parent;
    }
}

// convert position (x,y) in coordinates of root window
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
    CrashIf(!IsVisible());
}

// returns true if the style of control has changed
bool Control::SetStyle(Style *style)
{
    bool changed;
    CachedStyle *currStyle = cachedStyle;
    cachedStyle = CacheStyle(style, &changed);
    if (currStyle != cachedStyle)
        changed = true;
    if (changed)
        RequestRepaint(this);
    return changed;
}

}
