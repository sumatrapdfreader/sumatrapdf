/* Copyright 2019 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

class SplitterWnd;

enum class SplitterType {
    Horiz,
    Vert,
};

// called when user drags the splitter ('done' is false) and when drag is finished ('done' is
// true). the owner can constrain splitter by using current cursor
// position and returning false if it's not allowed to go there
typedef std::function<bool(bool)> SplitterWndCb;

SplitterWnd* CreateSplitter(HWND parent, SplitterType type, const SplitterWndCb&);
HWND GetHwnd(SplitterWnd*);
void SetBgCol(SplitterWnd*, COLORREF);

// call at the end of program
void DeleteSplitterBrush();

// If a splitter is "live", the owner will re-layout windows in the callback,
// otherwise we'll draw an indicator of where the splitter will be
// at the end of resize and the owner only re-layouts when callback is called
// with 'done' set to true
void SetSplitterLive(SplitterWnd*, bool live);

struct SplitterMoveArgs {
    bool done = false;
    // user can set to false to forbid resizing here
    bool resizeAllowed = true;
};

typedef std::function<void(SplitterMoveArgs*)> OnSplitterMove;

struct SplitterCtrl : public WindowBase {
    SplitterType type = SplitterType::Horiz;
    bool isLive = false;
    OnSplitterMove onSplitterMove = nullptr;

    HBITMAP bmp = nullptr;
    HBRUSH brush = nullptr;

    PointI prevResizeLinePos{};
    // if a parent clips children, DrawXorBar() doesn't work, so for
    // non-live resize, we need to remove WS_CLIPCHILDREN style from
    // parent and restore it when we're done
    bool parentClipsChildren = false;

    SplitterCtrl(HWND parent);
    ~SplitterCtrl() override;

    bool Create();
};

