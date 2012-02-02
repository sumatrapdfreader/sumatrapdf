/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef mui_h
#define mui_h

#define __STDC_LIMIT_MACROS
#include "BaseUtil.h"
#include "Vec.h"
#include "BitManip.h"
#include "GeomUtil.h"
#include "MuiBase.h"
#include "MuiCss.h"

namespace mui {

using namespace Gdiplus;
using namespace css;

class Control;
class HwndWrapper;
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

void        RequestRepaint(Control *w, const Rect *r1 = NULL, const Rect *r2 = NULL);
void        RequestLayout(Control *w);
Brush *     BrushFromProp(Prop *p, const Rect& r);

class IClickHandler
{
public:
    IClickHandler() {}
    virtual ~IClickHandler() {}
    virtual void Clicked(Control *w, int x, int y) = 0;
};

// Layout can be optionally set on Control. If set, it'll be
// used to layout this window. This effectively over-rides Measure()/Arrange()
// calls of Control. This allows to decouple layout logic from Control class
// and implement generic layout algorithms.
class Layout
{
public:
    Layout() {
    }

    virtual ~Layout() {
    }

    virtual void Measure(const Size availableSize, Control *wnd) = 0;
    virtual void Arrange(const Rect finalRect, Control *wnd) = 0;
};

// Manages painting process of Control window and all its children.
// Automatically does double-buffering for less flicker.
// Create one object for each HWND-backed Control and keep it around.
class Painter
{
    HwndWrapper *wnd;
    // bitmap for double-buffering
    Bitmap *    cacheBmp;
    Size        sizeDuringLastPaint;

    void PaintBackground(Graphics *g, Rect r);

public:
    Painter(HwndWrapper *wnd);

    void Paint(HWND hwnd);
};

// A single EventMgr is associated with a single HwndWrapper
// (which itself is associated with single HWND) and handles
// win32 messages for that HWND needed to make the whole system
// work.
class EventMgr
{
    HwndWrapper *   wndRoot;
    // current window over which the mouse is
    Control *       currOver;

    Size    minSize;
    Size    maxSize;

    struct ClickHandler {
        Control *        wndSource;
        IClickHandler *  clickHandler;
    };

    Vec<ClickHandler> clickHandlers;

    LRESULT OnSetCursor(int x, int y, bool& wasHandled);
    LRESULT OnMouseMove(WPARAM keys, int x, int y, bool& wasHandled);
    LRESULT OnLButtonUp(WPARAM keys, int x, int y, bool& wasHandled);
    LRESULT OnGetMinMaxInfo(MINMAXINFO *info, bool& wasHandled);

public:
    EventMgr(HwndWrapper *wndRoot);
    ~EventMgr();

    LRESULT OnMessage(UINT msg, WPARAM wParam, LPARAM lParam, bool& handledOut);

    void UnRegisterClickHandlers(IClickHandler *clickHandler);
    void RegisterClickHandler(Control *wndSource, IClickHandler *clickHandler);
    IClickHandler *GetClickHandlerFor(Control *wndSource);

    void SetMinSize(Size s);
    void SetMaxSize(Size s);
};

class Control
{
public:
    // allows a window to opt-out from being notified about
    // input events, stored in wantedInputBits
    enum WantedInputBits : int {
        WantsMouseOverBit   = 0,
        WantsMouseDownBit   = 1,
        WantsMouseUpBit     = 2,
        WantsMouseClickBit  = 3,
        WantsMouseMoveBit   = 4,
        WantedInputBitLast
    };

    // describes current state of a window, stored in stateBits
    enum ControlStateBits : int {
        MouseOverBit = 0,
        IsPressedBit = 1,
        // using IsHidden and not IsVisible so that 0 is default, visible state
        IsHiddenBit  = 2,
        StateBitLast
    };

    Control(Control *newParent=NULL);
    virtual ~Control();

    void        SetParent(Control *newParent);
    void        AddChild(Control *wnd, int pos = -1);
    Control *   GetChild(size_t idx) const;
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
    Control *       parent;

    // we cache properties for the current style during SetStyle() which
    // makes if fast to access them anywhere without repeating the work
    // of searching the style inheritance chain
    // properties are cached in a separate cache and not in each object
    // as it allows us to save per-object memory by sharing those properties
    // (we expect many objects of the same type to have the same style).
    // For now we cache every prop for each object type. We could limit
    // the set of cached props per-object (but right now it doesn't seem
    // to be enoug of a saving)
    Prop **         cachedProps;
    Prop **         GetCachedProps() const;
    Prop *          GetCachedProp(PropType propType) const;
    void            SetCurrentStyle(Style *style1, Style *style2);

    // only used by HwndWrapper but we need it here
    HWND            hwndParent;

    // cursor to show when mouse is over this window.
    // only works if the window sets WantsMouseOverBit.
    // Control doesn't own hCursor in order to enable easy 
    // sharing of cursor among many windows.
    HCURSOR         hCursor;

    // position and size (relative to parent, might be outside of parent's bounds)
    Rect            pos;

    // desired size calculated in Measure()
    Size            desiredSize;

private:
    Vec<Control*>   children;
};

// Control that has to be the root of window tree and is
// backed by a HWND. It combines a painter and EventManager
// for this HWND. In your message loop you must call
// HwndWrapper::evtMgr->OnMessage()
class HwndWrapper : public Control
{
    bool    layoutRequested;

public:
    Painter *    painter;
    EventMgr *          evtMgr;

    HwndWrapper(HWND hwnd = NULL);
    virtual ~HwndWrapper();

    void            SetMinSize(Size minSize);
    void            SetMaxSize(Size maxSize);

    void            RequestLayout();
    void            LayoutIfRequested();
    void            SetHwnd(HWND hwnd);
    void            OnPaint(HWND hwnd);

    virtual void    TopLevelLayout();
};

class Button : public Control
{

    // use SetStyles() to set
    Style *         styleDefault;    // gStyleButtonDefault if styleDefault is NULL
    Style *         styleMouseOver; // gStyleButtonMouseOver if NULL

public:
    Button(const TCHAR *s);

    virtual ~Button();

    void SetText(const TCHAR *s);

    void RecalculateSize(bool repaintIfSizeDidntChange);

    virtual void Measure(const Size availableSize);
    virtual void Paint(Graphics *gfx, int offX, int offY);

    virtual void NotifyMouseEnter();
    virtual void NotifyMouseLeave();

    Size    GetBorderAndPaddingSize() const;

    void    SetStyles(Style *def, Style *mouseOver);

    TCHAR *         text;
    size_t          textDx; // cached measured text width
};

}

#endif
