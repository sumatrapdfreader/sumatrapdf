/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef MuiHwndWrapper_h
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

#endif
