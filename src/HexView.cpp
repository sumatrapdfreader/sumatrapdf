/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/File.h"
#include "base/Win.h"
#include "gui/Dpi.h"

#include "gui/UIModels.h"
#include "gui/Layout.h"
#include "gui/win/WinGui.h"
#include "gui/PlatformFont.h"
#include "gui/Gfx.h"
#include "gui/VirtCtrl.h"

#include "Settings.h"
#include "DocController.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "AIChatTypes.h"
#include "WindowTab.h"
#include "Commands.h"
#include "AppSettings.h"
#include "Translations.h"
#include "Tabs.h"
#include "Theme.h"
#include "Notifications.h"
#include "Canvas.h"
#include "HexViewCommon.h"
#include "HexView.h"

// hex view of a tab's file, drawn on the canvas: offset, hex bytes and text
// columns. Uses the whole canvas, with as many bytes per row as fit
//
//          00 01 02 03 04 05 06 07  Text
//   0000   25 50 44 46 2D 31 2E 37  %PDF-1.7

constexpr int kHexFontPx = 13;
constexpr int kHexPadX = 8;
constexpr int kHexPadY = 4;
constexpr int kHexGap = 16;
// scrollbar positions are ints; a huge file maps several rows to one unit
constexpr u64 kHexScrollMax = 1u << 30;

static PlatformFont* HexFont() {
    return GetUserGuiFont(StrL("Consolas"), DpiScale(kHexFontPx));
}

static HexView* HexOfWindow(MainWindow* win) {
    WindowTab* tab = win ? win->CurrentTab() : nullptr;
    return tab && tab->IsHexTab() ? tab->hex : nullptr;
}

struct HexMetrics {
    int charDx = 0;
    int rowDy = 0;
    int gap = 0;
    int padX = 0;
    int padY = 0;
};

static HexMetrics GetHexMetrics() {
    PlatformFont* font = HexFont();
    Str sample = StrL("0123456789ABCDEF");
    HexMetrics m;
    m.charDx = std::max(PlatformFontMeasureText(font, sample).dx / len(sample), 1);
    m.rowDy = PlatformFontLineHeight(font) + DpiScale(2);
    m.gap = DpiScale(kHexGap);
    m.padX = DpiScale(kHexPadX);
    m.padY = DpiScale(kHexPadY);
    return m;
}

// fits rows and bytes per row to the canvas; the first row is the column header
static void HexLayout(MainWindow* win, HexView* hex, const HexMetrics& m) {
    Rect rc = HwndClientRect(win->hwndCanvas);
    int digits = HexOffsetDigits(hex->size);
    hex->perRow = HexPerRowFor((float)(rc.dx - 2 * m.padX), (float)m.charDx, (float)m.gap, digits);
    int fit = (rc.dy - 2 * m.padY) / m.rowDy - 1;
    hex->rows = std::max(fit, 1);
    HexSetTop(hex, hex->top);
}

static u64 HexRowsPerUnit(HexView* hex) {
    u64 total = HexLastTop(hex) / (u64)hex->perRow + (u64)hex->rows;
    return total / kHexScrollMax + 1;
}

void HexViewUpdateScrollbars(MainWindow* win) {
    HexView* hex = HexOfWindow(win);
    if (!hex || ScrollbarsAreHidden()) {
        return;
    }
    HexLayout(win, hex, GetHexMetrics());
    u64 unit = HexRowsPerUnit(hex);
    u64 total = HexLastTop(hex) / (u64)hex->perRow + (u64)hex->rows;
    SCROLLINFO si{};
    si.cbSize = sizeof(si);
    si.fMask = SIF_ALL | SIF_DISABLENOSCROLL;
    si.nMin = 0;
    si.nMax = (int)(total / unit) - 1;
    si.nPage = (UINT)std::max((u64)hex->rows / unit, (u64)1);
    si.nPos = (int)(hex->top / (u64)hex->perRow / unit);
    if (ScrollbarsUseOverlay()) {
        if (!win->overlayScrollV) {
            win->overlayScrollV =
                OverlayScrollbarCreate(win->hwndCanvas, OverlayScrollbar::Type::Vert, ScrollbarsOverlayMode());
        }
        bool show = HexLastTop(hex) > 0;
        OverlayScrollbarShow(win->overlayScrollV, show);
        if (show) {
            OverlayScrollbarSetInfo(win->overlayScrollV, &si, true);
        }
        return;
    }
    // always shown: hiding it would widen the rows and could flip-flop
    SetScrollInfo(win->hwndCanvas, SB_VERT, &si, TRUE);
    ShowWinScrollBar(win->hwndCanvas, SB_VERT, TRUE);
}

static void HexScrolled(MainWindow* win) {
    HexViewUpdateScrollbars(win);
    HwndInvalidate(win->hwndCanvas);
}

// draws text with the part [from, to) highlighted
static void DrawMarked(Gfx* gfx, Str s, Point pt, int from, int to, const HexMetrics& m, Color col) {
    PlatformFont* font = HexFont();
    if (from < 0) {
        gfx->DrawTextAt(s, pt, 0, font, col);
        return;
    }
    Rect rc(pt.x + from * m.charDx, pt.y, (to - from) * m.charDx, m.rowDy);
    gfx->FillRect(rc, ThemeNotificationsHighlightColor());
    gfx->DrawTextAt(s, pt, 0, font, col);
}

static void PaintHex(MainWindow* win, HexView* hex, Gfx* gfx) {
    HexMetrics m = GetHexMetrics();
    HexLayout(win, hex, m);
    PlatformFont* font = HexFont();
    Rect rc = HwndClientRect(win->hwndCanvas);
    gfx->FillRect(rc, ThemeMainWindowBackgroundColor());

    const u8* bytes = nullptr;
    int got = HexBytesShown(hex, &bytes);
    int perRow = hex->perRow;
    int digits = HexOffsetDigits(hex->size);
    int xHex = m.padX + digits * m.charDx + m.gap;
    int xText = xHex + (perRow * 3 - 1) * m.charDx + m.gap;
    Color colOffset = ThemeWindowLinkColor();
    Color colText = ThemeWindowTextColor();
    Color colChars = ThemeWindowTextDisabledColor();

    int y = m.padY;
    gfx->DrawTextAt(HexColumnsHeaderTemp(perRow), Point(xHex, y), 0, font, colOffset);
    gfx->DrawTextAt(Tr("Text"), Point(xText, y), 0, font, colOffset);
    y += m.rowDy;

    for (int r = 0; r < hex->rows; r++) {
        int at = r * perRow;
        if (at >= got) {
            break;
        }
        int n = std::min(got - at, perRow);
        u64 offset = hex->top + (u64)at;
        // the part of this row that is the byte gone to
        int from = -1;
        int to = -1;
        if (hex->hasMark && hex->mark >= offset && hex->mark < offset + (u64)n) {
            from = (int)(hex->mark - offset);
            to = from + 1;
        }
        gfx->DrawTextAt(HexOffsetTemp(offset, hex->size), Point(m.padX, y), 0, font, colOffset);
        int hexFrom = from < 0 ? -1 : from * 3;
        DrawMarked(gfx, HexBytesTemp(bytes + at, n, perRow), Point(xHex, y), hexFrom, hexFrom + 2, m, colText);
        DrawMarked(gfx, HexCharsTemp(bytes + at, n), Point(xText, y), from, to, m, colChars);
        y += m.rowDy;
    }
    if (got == 0 && hex->size > 0) {
        gfx->DrawTextAt(Tr("The file could not be read"), Point(m.padX, y), 0, font, colText);
    }
}

static void OnPaintHex(MainWindow* win, HexView* hex) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(win->hwndCanvas, &ps);
    SetLayout(hdc, 0);
    if (win->buffer) {
        Gfx* gfx = GfxCreate(win->buffer->GetDC());
        PaintHex(win, hex, gfx);
        delete gfx;
        win->buffer->Flush(hdc);
    }
    EndPaint(win->hwndCanvas, &ps);
}

static bool HexOnKeyDown(MainWindow* win, HexView* hex, WPARAM key) {
    switch (key) {
        case VK_UP:
            HexScrollRows(hex, -1);
            break;
        case VK_DOWN:
            HexScrollRows(hex, 1);
            break;
        case VK_PRIOR:
            HexScrollPages(hex, -1);
            break;
        case VK_NEXT:
            HexScrollPages(hex, 1);
            break;
        case VK_HOME:
            HexSetTop(hex, 0);
            break;
        case VK_END:
            HexSetTop(hex, hex->size);
            break;
        default:
            return false;
    }
    HexScrolled(win);
    return true;
}

static void HexOnVScroll(MainWindow* win, HexView* hex, WPARAM wp) {
    switch (LOWORD(wp)) {
        case SB_LINEUP:
            HexScrollRows(hex, -1);
            break;
        case SB_LINEDOWN:
            HexScrollRows(hex, 1);
            break;
        case SB_PAGEUP:
            HexScrollPages(hex, -1);
            break;
        case SB_PAGEDOWN:
            HexScrollPages(hex, 1);
            break;
        case SB_TOP:
            HexSetTop(hex, 0);
            break;
        case SB_BOTTOM:
            HexSetTop(hex, hex->size);
            break;
        case SB_THUMBTRACK:
        case SB_THUMBPOSITION: {
            int pos = 0;
            if (win->overlayScrollV && ScrollbarsUseOverlay()) {
                pos = win->overlayScrollV->nTrackPos;
            } else {
                // HIWORD is only 16 bits
                SCROLLINFO si{};
                si.cbSize = sizeof(si);
                si.fMask = SIF_TRACKPOS;
                GetScrollInfo(win->hwndCanvas, SB_VERT, &si);
                pos = si.nTrackPos;
            }
            u64 row = (u64)std::max(pos, 0) * HexRowsPerUnit(hex);
            HexSetTop(hex, row * (u64)hex->perRow);
            break;
        }
        default:
            return;
    }
    HexScrolled(win);
}

static void HexOnWheel(MainWindow* win, HexView* hex, WPARAM wp) {
    // 3 rows per notch; touchpads send fractions of a notch
    constexpr float kRowsPerNotch = 3.f;
    hex->wheelRem += (float)GET_WHEEL_DELTA_WPARAM(wp) * kRowsPerNotch / WHEEL_DELTA;
    int rows = (int)hex->wheelRem;
    if (rows == 0) {
        return;
    }
    hex->wheelRem -= (float)rows;
    HexScrollRows(hex, -rows);
    HexScrolled(win);
}

LRESULT WndProcCanvasHex(MainWindow* win, HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    HexView* hex = HexOfWindow(win);
    if (!hex) {
        return DefWindowProc(hwnd, msg, wp, lp);
    }
    switch (msg) {
        case WM_PAINT:
            OnPaintHex(win, hex);
            return 0;
        case WM_KEYDOWN:
            if (HexOnKeyDown(win, hex, wp)) {
                return 0;
            }
            break;
        case WM_VSCROLL:
            HexOnVScroll(win, hex, wp);
            return 0;
        case WM_MOUSEWHEEL:
            HexOnWheel(win, hex, wp);
            return 0;
        case WM_MOUSEHWHEEL:
            // the frame forwards wheel messages here: DefWindowProc would bounce it back
            return 0;
        case WM_SETCURSOR:
            win->DeleteToolTip();
            SetCursorCached(IDC_ARROW);
            return TRUE;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

// shows the tab's file as hex: in a new tab, or in the tab itself if its
// document failed to load
void ViewTabAsHex(WindowTab* tab) {
    if (!tab || len(tab->filePath) == 0 || tab->IsHexTab()) {
        return;
    }
    MainWindow* win = tab->win;
    TempStr path = str::DupTemp(tab->filePath);
    HexView* hex = HexViewOpen(path);
    if (!hex) {
        TempStr msg = fmt(Tr("Couldn't open '%s'").s, path);
        ShowPlainWarningNotification(win, msg, kNotif5SecsTimeOut);
        return;
    }

    bool inPlace = !tab->ctrl && tab->loadState == WindowTab::LoadState::Error;
    if (!inPlace) {
        SaveCurrentWindowTab(win);
        tab = new WindowTab(win);
        tab->SetFilePath(path);
    }
    // after SetFilePath(), which makes it a document tab
    tab->type = WindowTab::Type::HexView;
    tab->hex = hex;
    if (inPlace) {
        tab->loadState = WindowTab::LoadState::None;
        str::FreePtr(&tab->loadErrorReason);
        UpdateTabIsError(tab);
        TabsOnChangedDoc(win);
    } else {
        AddTabToWindow(win, tab);
    }
    LoadModelIntoTab(tab);
}
