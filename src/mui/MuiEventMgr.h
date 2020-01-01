/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

class HwndWrapper;
class Control;

class ControlEvents {
  public:
    std::function<void(Control*, int, int)> Clicked;
    std::function<void(Control*, int, int)> SizeChanged;
};

class NamedEvents {
  public:
    std::function<void(Control*, int, int)> Clicked;
};

// A single EventMgr is associated with a single HwndWrapper
// (which itself is associated with single HWND) and handles
// win32 messages for that HWND needed to make the whole system
// work.
class EventMgr {
    HwndWrapper* wndRoot;
    // current window over which the mouse is
    Control* currOver;

    Size minSize;
    Size maxSize;

    bool inSizeMove;

    struct EventHandler {
        Control* ctrlSource;
        ControlEvents* events;
    };
    Vec<EventHandler> eventHandlers;

    struct NamedEventHandler {
        const char* name;
        NamedEvents* namedEvents;
    };
    Vec<NamedEventHandler> namedEventHandlers;

    LRESULT OnSetCursor(int x, int y, bool& wasHandled);
    LRESULT OnMouseMove(WPARAM keys, int x, int y, bool& wasHandled);
    LRESULT OnLButtonUp(WPARAM keys, int x, int y, bool& wasHandled);
    LRESULT OnGetMinMaxInfo(MINMAXINFO* info, bool& wasHandled);

  public:
    EventMgr(HwndWrapper* wndRoot);
    ~EventMgr();

    LRESULT OnMessage(UINT msg, WPARAM wParam, LPARAM lParam, bool& handledOut);

    ControlEvents* EventsForControl(Control* c);
    NamedEvents* EventsForName(const char* name);

    void RemoveEventsForControl(Control* c);

    void NotifyClicked(Control* c, int x, int y);
    void NotifySizeChanged(Control* c, int dx, int dy);
    void NotifyNamedEventClicked(Control* c, int x, int y);

    bool IsInSizeMove() const {
        return inSizeMove;
    }

    void SetMinSize(Size s);
    void SetMaxSize(Size s);
};
