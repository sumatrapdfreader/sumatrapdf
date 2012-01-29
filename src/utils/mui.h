/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef mui_h
#define mui_h

#define __STDC_LIMIT_MACROS
#include "BaseUtil.h"
#include "Vec.h"
#include "BitManip.h"
#include "GeomUtil.h"
#include "MuiCss.h"

namespace mui {

using namespace Gdiplus;
using namespace css;

class VirtWnd;
class VirtWndHwnd;
class IClickHandler;

#define SizeInfinite ((INT)-1)

void        Initialize();
void        Destroy();

// Graphics objects cannot be used across threads. This class
// allows an easy allocation of small Graphics objects that
// can be used for measuring text
class GraphicsForMeasureText
{
    enum {
        bmpDx = 32,
        bmpDy = 4,
        stride = bmpDx * 4,
    };

    Graphics *  gfx;
    Bitmap *    bmp;
    BYTE        data[bmpDx * bmpDy * 4];
public:
    GraphicsForMeasureText();
    ~GraphicsForMeasureText();
    bool Create();
    Graphics *Get();
};

GraphicsForMeasureText *AllocGraphicsForMeasureText();

Graphics *UIThreadGraphicsForMeasureText();

void        RequestRepaint(VirtWnd *w, const Rect *r1 = NULL, const Rect *r2 = NULL);
void        RequestLayout(VirtWnd *w);
Brush *     BrushFromProp(Prop *p, const Rect& r);

class IClickHandler
{
public:
    IClickHandler() {}
    virtual ~IClickHandler() {}
    virtual void Clicked(VirtWnd *w, int x, int y) = 0;
};

// Layout can be optionally set on VirtWnd. If set, it'll be
// used to layout this window. This effectively over-rides Measure()/Arrange()
// calls of VirtWnd. This allows to decouple layout logic from VirtWnd class
// and implement generic layout algorithms.
class Layout
{
public:
    Layout() {
    }

    virtual ~Layout() {
    }

    virtual void Measure(const Size availableSize, VirtWnd *wnd) = 0;
    virtual void Arrange(const Rect finalRect, VirtWnd *wnd) = 0;
};

// Manages painting process of VirtWnd window and all its children.
// Automatically does double-buffering for less flicker.
// Create one object for each HWND-backed VirtWnd and keep it around.
class VirtWndPainter
{
    VirtWndHwnd *wnd;
    // bitmap for double-buffering
    Bitmap *    cacheBmp;
    Size        sizeDuringLastPaint;

    void PaintBackground(Graphics *g, Rect r);

public:
    VirtWndPainter(VirtWndHwnd *wnd);

    void Paint(HWND hwnd);
};

// A single EventMgr is associated with a single VirtWndHwnd
// (which itself is associated with single HWND) and handles
// win32 messages for that HWND needed to make the whole system
// work.
class EventMgr
{
    VirtWndHwnd *   wndRoot;
    // current window over which the mouse is
    VirtWnd *       currOver;

    Size    minSize;
    Size    maxSize;

    struct ClickHandler {
        VirtWnd *        wndSource;
        IClickHandler *  clickHandler;
    };

    Vec<ClickHandler> clickHandlers;

    LRESULT OnSetCursor(int x, int y, bool& wasHandled);
    LRESULT OnMouseMove(WPARAM keys, int x, int y, bool& wasHandled);
    LRESULT OnLButtonUp(WPARAM keys, int x, int y, bool& wasHandled);
    LRESULT OnGetMinMaxInfo(MINMAXINFO *info, bool& wasHandled);

public:
    EventMgr(VirtWndHwnd *wndRoot);
    ~EventMgr();

    LRESULT OnMessage(UINT msg, WPARAM wParam, LPARAM lParam, bool& handledOut);

    void UnRegisterClickHandlers(IClickHandler *clickHandler);
    void RegisterClickHandler(VirtWnd *wndSource, IClickHandler *clickHandler);
    IClickHandler *GetClickHandlerFor(VirtWnd *wndSource);

    void SetMinSize(Size s);
    void SetMaxSize(Size s);
};

class VirtWnd
{
public:
    // allows a window to opt-out from being notified about
    // input events, stored in wantedInputBits
    enum WndWantedInputBits : int {
        WantsMouseOverBit   = 0,
        WantsMouseDownBit   = 1,
        WantsMouseUpBit     = 2,
        WantsMouseClickBit  = 3,
        WantsMouseMoveBit   = 4,
        WndWantedInputBitLast
    };

    // describes current state of a window, stored in stateBits
    enum WndStateBits : int {
        MouseOverBit = 0,
        IsPressedBit = 1,
        // using IsHidden and not IsVisible so that 0 is default, visible state
        IsHiddenBit  = 2,
        WndStateBitLast
    };

    VirtWnd(VirtWnd *newParent=NULL);
    virtual ~VirtWnd();

    void        SetParent(VirtWnd *newParent);
    void        AddChild(VirtWnd *wnd, int pos = -1);
    VirtWnd *   GetChild(size_t idx) const;
    size_t      GetChildCount() const;

    void        SetPosition(const Rect& p);

    virtual void Paint(Graphics *gfx, int offX, int offY);

    // WPF-like layout system. Measure() should update desiredSize
    // Then the parent uses it to calculate the size of its children
    // and uses Arrange() to set it.

    // availableSize can have SizeInfinite as dx or dy to allow
    // using as much space as the window wants
    virtual void Measure(const Size availableSize);
    virtual void Arrange(const Rect finalRect);

    // mouse enter/leave are used e.g. by a button to change the look when mouse
    // is over them. The intention is that in response to those a window should
    // only do minimal processing that affects the window itself, not the rest
    // of the system
    virtual void NotifyMouseEnter() {}
    virtual void NotifyMouseLeave() {}

    virtual void NotifyMouseMove(int x, int y) {}

    virtual void RegisterEventHandlers(EventMgr *evtMgr) {}
    virtual void UnRegisterEventHandlers(EventMgr *evtMgr) {}

    bool WantsMouseClick() const;
    bool WantsMouseMove() const;
    bool IsMouseOver() const;
    void SetIsMouseOver(bool isOver);

    bool IsVisible() const;
    void Hide();
    void Show();

    void MeasureChildren(Size availableSize) const;
    void MapRootToMyPos(int& x, int& y) const;

    uint16          wantedInputBits; // WndWantedInputBits
    uint16          stateBits;       // WndStateBits
    // windows with bigger z-order are painted on top, 0 is default
    int16           zOrder;

    Layout *        layout;
    VirtWnd *       parent;

    // VirtWnd doesn't own this object to allow sharing the same
    // instance among multiple windows. Whoever allocates them
    // must delete them
    Style *         styleDefault;

    // only used by VirtWndHwnd but we need it here
    HWND            hwndParent;

    // cursor to show when mouse is over this window.
    // only works if the window sets WantsMouseOverBit.
    // VirtWnd doesn't own hCursor in order to enable easy 
    // sharing of cursor among many windows.
    HCURSOR         hCursor;

    // position and size (relative to parent, might be outside of parent's bounds)
    Rect            pos;

    // desired size calculated in Measure()
    Size            desiredSize;

private:
    Vec<VirtWnd*>   children;
};

// VirtWnd that has to be the root of window tree and is
// backed by a HWND. It combines a painter and EventManager
// for this HWND. In your message loop you must call
// VirtWndHwnd::evtMgr->OnMessage()
class VirtWndHwnd : public VirtWnd
{
    bool    layoutRequested;

public:
    VirtWndPainter *    painter;
    EventMgr *          evtMgr;

    VirtWndHwnd(HWND hwnd = NULL);
    virtual ~VirtWndHwnd();

    void            SetMinSize(Size minSize);
    void            SetMaxSize(Size maxSize);

    void            RequestLayout();
    void            LayoutIfRequested();
    void            SetHwnd(HWND hwnd);
    void            OnPaint(HWND hwnd);

    virtual void    TopLevelLayout();
};

class VirtWndButton : public VirtWnd
{
public:
    VirtWndButton(const TCHAR *s);

    virtual ~VirtWndButton() {
        free(text);
    }

    void SetText(const TCHAR *s);

    void RecalculateSize(bool repaintIfSizeDidntChange);

    virtual void Measure(const Size availableSize);
    virtual void Paint(Graphics *gfx, int offX, int offY);

    virtual void NotifyMouseEnter();
    virtual void NotifyMouseLeave();

    Size    GetBorderAndPaddingSize() const;

    void    GetStyleForState(Style **first, Style **second) const;
    Prop *  GetPropForState(PropType type) const;
    void    GetPropsForState(PropToGet *props, size_t propsCount) const;

    Font *  GetFontForState() const;

    void    SetStyles(Style *def, Style *mouseOver);

    TCHAR *         text;
    size_t          textDx; // cached measured text width

    // gStyleButtonDefault if styleDefault is NULL
    Style *         styleMouseOver; // gStyleButtonMouseOver if NULL
};

}

#endif
