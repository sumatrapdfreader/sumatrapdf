/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef SumatraPDF_Layout_win_h
#define SumatraPDF_Layout_win_h

class DeferWinPosHelper;

// MoveWindow of an HWND that is not itself an ILayout (frame canvas, lazy webview).
// lastBounds is always recorded, so a null hwnd still works as a measured slot.
struct HwndSlot : ILayout {
    HWND hwnd = nullptr;
    // if set, SetBounds batches the move through this helper (not owned)
    DeferWinPosHelper* winPos = nullptr;
    int dx = 0;
    int dy = 0;
    // SetWindowPos x as offset from the physical left when the parent is RTL
    // (tabs / menu rebar). Off for toc / canvas / favorites, which already
    // live in the frame's client coordinates.
    bool mapRtlX = false;

    HwndSlot(HWND hwnd = nullptr, int dx = 0, int dy = 0);
    ~HwndSlot() override;

    Size Layout(Constraints bc) override;
    int MinIntrinsicHeight(int width) override;
    int MinIntrinsicWidth(int height) override;
    void SetBounds(Rect) override;
};

void LayoutAndSizeToContent(ILayout* layout, int minDx, int minDy, HWND hwnd);

#if defined(DEBUG)
void LayoutWin_UnitTests();
#endif

#endif
