/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef Mui_h
#error "this is only meant to be included by Mui.h inside mui namespace"
#endif
#ifdef MuiEventMgr_h
#error "dont include twice!"
#endif
#define MuiEventMgr_h

class HwndWrapper;
class Control;

class IClickHandler
{
public:
    IClickHandler() {}
    virtual ~IClickHandler() {}
    virtual void Clicked(Control *w, int x, int y) = 0;
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

