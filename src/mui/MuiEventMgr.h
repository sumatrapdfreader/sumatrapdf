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

class IClicked
{
public:
    virtual void Clicked(Control *c, int x, int y) = 0;
};

class ISizeChanged
{
public:
    virtual void SizeChanged(Control *c, int newDx, int newDy) = 0;
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

    struct EventHandler {
        enum Type {
            Clicked, SizeChanged
        };
        Type            type;
        Control *       ctrlSource;
        union {
            IClicked *      clicked;
            ISizeChanged *  sizeChanged;
            void *          handler;
        };
    };

    Vec<EventHandler> eventHandlers;

    LRESULT OnSetCursor(int x, int y, bool& wasHandled);
    LRESULT OnMouseMove(WPARAM keys, int x, int y, bool& wasHandled);
    LRESULT OnLButtonUp(WPARAM keys, int x, int y, bool& wasHandled);
    LRESULT OnGetMinMaxInfo(MINMAXINFO *info, bool& wasHandled);

    void UnRegisterEventHandler(EventHandler::Type type, void *handler);

public:
    EventMgr(HwndWrapper *wndRoot);
    ~EventMgr();

    LRESULT OnMessage(UINT msg, WPARAM wParam, LPARAM lParam, bool& handledOut);

    void           UnRegisterClicked(IClicked *handler);
    void           RegisterClicked(Control *ctrlSource, IClicked *handler);
    void           NotifyClicked(Control *c, int x, int y);

    void           UnRegisterSizeChanged(ISizeChanged *handler);
    void           RegisterSizeChanged(Control *ctrlSource, ISizeChanged *handler);
    void           NotifySizeChanged(Control *c, int dx, int dy);

    void SetMinSize(Size s);
    void SetMaxSize(Size s);
};

