/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef Mui_h
#error "this is only meant to be included by Mui.h inside mui namespace"
#endif
#ifdef MuiHwndWrapper_h
#error "dont include twice!"
#endif
#define MuiHwndWrapper_h

class Painter;
class EventMgr;

// Control that has to be the root of window tree and is
// backed by a HWND. It combines a painter and EventManager
// for this HWND. In your message loop you must call
// HwndWrapper::evtMgr->OnMessage()
class HwndWrapper : public Control
{
    bool    layoutRequested;
    bool    repaintRequested;
    bool    firstLayout;

public:
    Painter *           painter;
    EventMgr *          evtMgr;
    // size the window to fit the size of the content on first layout
    bool                sizeToFit;
    // center the content within the window. Incompatible with sizeToFit
    bool                centerContent;

    HwndWrapper(HWND hwnd = NULL);
    virtual ~HwndWrapper();

    void            SetMinSize(Size minSize);
    void            SetMaxSize(Size maxSize);

    void            RequestLayout();
    void            RequestRepaint() { repaintRequested = true; }
    void            LayoutIfRequested();
    void            SetHwnd(HWND hwnd);
    void            OnPaint(HWND hwnd);

    // ILayout
    virtual Size Measure(const Size availableSize);
    virtual void Arrange(const Rect finalRect);

    void           TopLevelLayout();
};
