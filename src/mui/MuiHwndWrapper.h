/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

class Painter;
class EventMgr;

// Control that has to be the root of window tree and is
// backed by a HWND. It combines a painter and EventManager
// for this HWND. In your message loop you must call
// HwndWrapper::evtMgr->OnMessage()
class HwndWrapper : public Control {
    bool layoutRequested = false;
    bool markedForRepaint = false;
    bool firstLayout = true;

  public:
    Painter* painter = nullptr;
    EventMgr* evtMgr = nullptr;
    // size the window to fit the size of the content on first layout
    bool sizeToFit = false;
    // center the content within the window. Incompatible with sizeToFit
    bool centerContent = false;

    FrameRateWnd* frameRateWnd = nullptr;

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
    Size Measure(const Size availableSize) override;
    void Arrange(const Rect finalRect) override;

    void TopLevelLayout();

    bool IsInSizeMove() const;
};
