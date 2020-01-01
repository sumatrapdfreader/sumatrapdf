/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

class Painter;
class EventMgr;

// Control that has to be the root of window tree and is
// backed by a HWND. It combines a painter and EventManager
// for this HWND. In your message loop you must call
// HwndWrapper::evtMgr->OnMessage()
class HwndWrapper : public Control {
    bool layoutRequested;
    bool markedForRepaint;
    bool firstLayout;

  public:
    Painter* painter;
    EventMgr* evtMgr;
    // size the window to fit the size of the content on first layout
    bool sizeToFit;
    // center the content within the window. Incompatible with sizeToFit
    bool centerContent;

    FrameRateWnd* frameRateWnd;

    HwndWrapper(HWND hwnd = nullptr);
    virtual ~HwndWrapper();

    void SetMinSize(Size minSize);
    void SetMaxSize(Size maxSize);

    void RequestLayout();
    void MarkForRepaint() {
        markedForRepaint = true;
    }
    void LayoutIfRequested();
    void SetHwnd(HWND hwnd);
    void OnPaint(HWND hwnd);

    // ILayout
    virtual Size Measure(const Size availableSize);
    virtual void Arrange(const Rect finalRect);

    void TopLevelLayout();

    bool IsInSizeMove() const;
};
