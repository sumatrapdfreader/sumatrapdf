/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Win.h"
#include "base/Dpi.h"
#include "base/UITask.h"

#include "wingui/UIModels.h"
#include "wingui/Layout.h"
#include "wingui/WinGui.h"

#include "Settings.h"
#include "GlobalPrefs.h"
#include "DocController.h"
#include "MainWindow.h"
#include "Theme.h"
#include "AppSettings.h"
#include "Commands.h"
#include "Accelerators.h"
#include "Translations.h"
#include "KeyboardHelp.h"

// A compact keyboard cheat sheet, in the spirit of Vimium's `?` overlay: the
// most useful commands grouped into sections, each row showing the command's
// current shortcut (as key-caps) and its name. Both the shortcuts and the names
// come straight from the command table and the live accelerator list, so a user
// override or a rebind shows through here without any extra bookkeeping. A
// command with no binding is simply skipped, so the sheet self-adjusts.

// which commands go in which section; each list is 0-terminated. Kept in enum
// order within a section only for readability - display order follows this.
// clang-format off
static const int kSecNav[] = {
    CmdScrollUp, CmdScrollDown, CmdScrollLeft, CmdScrollRight,
    CmdScrollUpPage, CmdScrollDownPage,
    CmdGoToNextPage, CmdGoToPrevPage,
    CmdGoToFirstPage, CmdGoToLastPage, CmdGoToPage,
    CmdNavigateBack, CmdNavigateForward, 0,
};
static const int kSecView[] = {
    CmdZoomIn, CmdZoomOut,
    CmdZoomFitPage, CmdZoomFitWidth, CmdZoomActualSize, CmdToggleZoom,
    CmdSinglePageView, CmdFacingView, CmdBookView, CmdToggleContinuousView,
    CmdRotateLeft, CmdRotateRight,
    CmdToggleFullscreen, CmdTogglePresentationMode, CmdInvertColors, 0,
};
static const int kSecDoc[] = {
    CmdOpenFile, CmdSaveAs, CmdPrint, CmdReloadDocument, CmdClose, CmdNewWindow,
    CmdOpenNextFileInFolder, CmdOpenPrevFileInFolder, CmdRenameFile, CmdProperties, 0,
};
static const int kSecFind[] = {
    CmdFindFirst, CmdFindNext, CmdFindPrev,
    CmdSelectAll, CmdCopySelection,
    CmdSelectTextViaKeyboard, CmdToggleKeyboardLinkFollowing, 0,
};
static const int kSecTabs[] = {
    CmdNextTabSmart, CmdNextTab, CmdPrevTab,
    CmdMoveTabLeft, CmdMoveTabRight, CmdReopenLastClosedFile, 0,
};
static const int kSecAnnot[] = {
    CmdCreateAnnotHighlight, CmdCreateAnnotUnderline,
    CmdSaveAnnotations, CmdDeleteAnnotation, 0,
};
static const int kSecIface[] = {
    CmdToggleBookmarks, CmdToggleToolbar, CmdToggleMenuBar,
    CmdToggleCursorPosition, CmdTogglePageInfo, CmdCommandPalette,
    CmdFavoriteAdd, CmdFavoriteToggle, CmdHelpOpenManual, CmdToggleKeyboardHelp, 0,
};

struct KbSectionDef {
    const char* title;
    const int* cmds;
};

static KbSectionDef gKbSectionDefs[] = {
    {"Navigation", kSecNav},
    {"View & Zoom", kSecView},
    {"Document", kSecDoc},
    {"Find & Select", kSecFind},
    {"Tabs", kSecTabs},
    {"Annotations", kSecAnnot},
    {"Interface", kSecIface},
};
// clang-format on

struct KbRow {
    Str keys;       // owned, shortcut(s) e.g. "↑, K"
    Str desc;       // owned, command name
    int keysDx = 0; // measured width of the key-caps
    int descDx = 0; // measured width of the description text
};

struct KbSection {
    Str title; // owned, translated
    Vec<KbRow> rows;
    int col = 0;    // 0 (left) or 1 (right)
    int y = 0;      // top of the section, relative to content area
    int height = 0; // section title + rows
};

struct KeyboardHelpWnd : Wnd {
    MainWindow* win = nullptr;

    HFONT fontTitle = nullptr; // window title
    HFONT fontHdr = nullptr;   // section headers
    HFONT fontRow = nullptr;   // rows (app font, not owned)
    bool ownTitle = false;
    bool ownHdr = false;

    Vec<KbSection> sections;

    // metrics, in client pixels, computed in BuildContent
    int pad = 0;
    int contentTop = 0;
    int colGap = 0;
    int secGap = 0;
    int rowH = 0;
    int rowTextH = 0;
    int secTitleH = 0;
    int keysDescGap = 0;
    int capPadX = 0;
    int capGap = 0;
    int footerH = 0;
    // per-column geometry (each column is sized to just fit its own content, so
    // the inter-column gap and right margin stay exactly what we set them to)
    int colX[2] = {0, 0};
    int colKeysW[2] = {0, 0};
    int colDescW[2] = {0, 0};
    int colW[2] = {0, 0};
    Rect closeRc; // the 'x' hit rect, updated on paint

    ~KeyboardHelpWnd() override;
    bool Create(MainWindow* win);
    void BuildContent(HDC hdc);
    void PaintContent(HDC hdc, const Rect& client);
    LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) override;
    bool PreTranslateMessage(MSG& msg) override;
};

static KeyboardHelpWnd* gKeyboardHelpWnd = nullptr;
static HWND gHwndToActivateOnClose = nullptr;

// draw red marks over the left margin, inter-column gap and right margin, for
// eyeballing the spacing while tweaking the layout
static bool gShowGuideLines = false;

static void SafeDeleteKeyboardHelpWnd() {
    if (!gKeyboardHelpWnd) {
        return;
    }
    auto* tmp = gKeyboardHelpWnd;
    gKeyboardHelpWnd = nullptr;
    delete tmp;
    if (gHwndToActivateOnClose) {
        HWND fg = GetForegroundWindow();
        if (!fg || fg == gHwndToActivateOnClose) {
            SetActiveWindow(gHwndToActivateOnClose);
        }
        gHwndToActivateOnClose = nullptr;
    }
}

// close on the UI task queue: the trigger can arrive from inside the window's
// own message handling (WM_ACTIVATE / a key), where deleting it now would pull
// the ground out from under the current call.
static void ScheduleCloseKeyboardHelp() {
    if (!gKeyboardHelpWnd) {
        return;
    }
    auto fn = MkFunc0Void(SafeDeleteKeyboardHelpWnd);
    uitask::Post(fn, "SafeDeleteKeyboardHelpWnd");
}

static HFONT CreateBoldFontFrom(HFONT font) {
    if (!font) {
        return nullptr;
    }
    LOGFONTW lf{};
    if (GetObjectW(font, sizeof(lf), &lf) == 0) {
        return nullptr;
    }
    lf.lfWeight = FW_BOLD;
    return CreateFontIndirectW(&lf);
}

// id -> its (translated) menu description; empty if not found
static TempStr CmdDescTemp(int cmdId) {
    int off = 0;
    int id = (int)CmdFirst + 1;
    while (SeqStrAt(gCommandDescriptions, off).s) {
        if (id == cmdId) {
            Str s = SeqStrAt(gCommandDescriptions, off);
            return str::DupTemp(trans::GetTranslation(s));
        }
        if (!SeqStrAdvance(gCommandDescriptions, off, &id)) {
            break;
        }
    }
    return {};
}

KeyboardHelpWnd::~KeyboardHelpWnd() {
    for (auto& s : sections) {
        str::Free(s.title);
        for (auto& r : s.rows) {
            str::Free(r.keys);
            str::Free(r.desc);
        }
    }
    if (ownTitle && fontTitle) {
        DeleteObject(fontTitle);
    }
    if (ownHdr && fontHdr) {
        DeleteObject(fontHdr);
    }
}

// Park the window beside the main window on whichever side has more room, so it
// covers the document as little as possible, always kept fully on-screen. In
// fullscreen / presentation (the frame fills the monitor, so neither side has
// room) it's pinned to the right edge instead. The user can still drag it.
static Rect PositionHelpWindow(MainWindow* win, int winDx, int winDy) {
    HWND frame = win->hwndFrame;
    Rect fr = HwndWindowRect(frame);

    if (win->isFullScreen || win->InPresentation()) {
        // the frame is the whole screen; sit against its right edge, centered
        // vertically
        int x = std::max(fr.x, (fr.x + fr.dx) - winDx);
        int y = std::max(fr.y, fr.y + ((fr.dy - winDy) / 2));
        return Rect{x, y, winDx, winDy};
    }

    Rect work = GetWorkAreaRect(fr, frame);
    if (IsZoomed(frame)) {
        // maximized: the frame covers the whole work area, so there's no room
        // beside it - pin to the right edge and center vertically
        int x = std::max(work.x, (work.x + work.dx) - winDx);
        int maxY = std::max(work.y, (work.y + work.dy) - winDy);
        int y = limitValue(work.y + ((work.dy - winDy) / 2), work.y, maxY);
        return Rect{x, y, winDx, winDy};
    }

    int spaceRight = (work.x + work.dx) - (fr.x + fr.dx);
    int spaceLeft = fr.x - work.x;
    // hug the frame edge on the roomier side; the columns read left-to-right,
    // so on a tie prefer the right
    int x = (spaceRight >= spaceLeft) ? (fr.x + fr.dx) : (fr.x - winDx);
    int y = fr.y;
    // keep it fully visible on the monitor (may overlap the frame if that side
    // is narrower than the window, but it's never clipped off-screen)
    x = limitValue(x, work.x, work.x + work.dx - winDx);
    y = limitValue(y, work.y, work.y + work.dy - winDy);
    return Rect{x, y, winDx, winDy};
}

static int MeasureKeyCapsWidth(KeyboardHelpWnd* w, HDC hdc, Str keys);

void KeyboardHelpWnd::BuildContent(HDC hdc) {
    fontRow = GetAppFont(hwnd);
    HFONT baseTitle = GetAppBiggerFont(hwnd);
    HFONT b = CreateBoldFontFrom(baseTitle);
    if (b) {
        fontTitle = b;
        ownTitle = true;
    } else {
        fontTitle = baseTitle;
    }
    HFONT h = CreateBoldFontFrom(fontRow);
    if (h) {
        fontHdr = h;
        ownHdr = true;
    } else {
        fontHdr = fontRow;
    }

    Size szRow = HdcMeasureText(hdc, "Ag", fontRow);
    Size szHdr = HdcMeasureText(hdc, "Ag", fontHdr);
    Size szTitle = HdcMeasureText(hdc, "Ag", fontTitle);
    rowTextH = szRow.dy;

    int rem = DpiScale(hwnd, 16); // 1rem == 16px at 96 DPI
    pad = DpiScale(hwnd, 20);
    int titleGap = DpiScale(hwnd, 16);
    rowH = rowTextH + DpiScale(hwnd, 8);
    secTitleH = szHdr.dy + DpiScale(hwnd, 8);
    secGap = DpiScale(hwnd, 14);
    colGap = rem; // gap between the two columns
    capPadX = DpiScale(hwnd, 7);
    capGap = DpiScale(hwnd, 5);
    keysDescGap = DpiScale(hwnd, 12);
    contentTop = pad + szTitle.dy + titleGap;
    footerH = szRow.dy + DpiScale(hwnd, 6);

    for (auto& def : gKbSectionDefs) {
        KbSection s;
        s.title = str::Dup(trans::GetTranslation(Str(def.title)));
        for (const int* c = def.cmds; *c; c++) {
            TempStr keys = ShortcutsForCmdTemp(*c, 2);
            if (keys.len == 0) {
                continue;
            }
            TempStr desc = CmdDescTemp(*c);
            if (desc.len == 0) {
                continue;
            }
            KbRow r;
            r.keys = str::Dup(keys);
            r.desc = str::Dup(desc);
            r.keysDx = MeasureKeyCapsWidth(this, hdc, keys);
            r.descDx = HdcMeasureText(hdc, desc, fontRow).dx;
            s.rows.Append(r);
        }
        if (len(s.rows) == 0) {
            str::Free(s.title);
            continue;
        }
        s.height = secTitleH + (len(s.rows) * rowH);
        sections.Append(s);
    }

    // split the sections into two columns at the point that evens out their
    // heights best. The left column gets a prefix of the sections, so they stay
    // in reading order (Navigation top-left) while the two columns end up about
    // as tall as each other.
    int n = len(sections);
    int total = 0;
    for (auto& s : sections) {
        total += s.height + secGap;
    }
    int bestSplit = n; // sections [0, bestSplit) go left, rest go right
    int bestBalance = total;
    int prefix = 0;
    for (int k = 1; k < n; k++) {
        prefix += sections[k - 1].height + secGap;
        int leftH = prefix;
        int rightH = total - prefix;
        int bal = std::max(leftH, rightH);
        if (bal < bestBalance) {
            bestBalance = bal;
            bestSplit = k;
        }
    }
    int colY[2] = {0, 0};
    for (int i = 0; i < n; i++) {
        int col = (i < bestSplit) ? 0 : 1;
        sections[i].col = col;
        sections[i].y = colY[col];
        colY[col] += sections[i].height + secGap;
    }
    int maxColH = std::max(colY[0], colY[1]) - secGap;
    if (maxColH < 0) {
        maxColH = 0;
    }

    // size each column to just fit its own widest key-caps and description, so
    // there's no dead space padding out a fixed column width
    for (auto& s : sections) {
        int c = s.col;
        for (auto& r : s.rows) {
            colKeysW[c] = std::max(colKeysW[c], r.keysDx);
            colDescW[c] = std::max(colDescW[c], r.descDx);
        }
    }
    colW[0] = colKeysW[0] + keysDescGap + colDescW[0];
    colW[1] = colKeysW[1] + keysDescGap + colDescW[1];
    colX[0] = pad;
    colX[1] = pad + colW[0] + colGap;

    int rightPad = rem / 2; // 0.5rem right margin
    int clientDx = colX[1] + colW[1] + rightPad;
    int clientDy = contentTop + maxColH + DpiScale(hwnd, 12) + footerH + DpiScale(hwnd, 6);

    RECT wr = {0, 0, clientDx, clientDy};
    DWORD style = (DWORD)GetWindowLongPtrW(hwnd, GWL_STYLE);
    DWORD exStyle = (DWORD)GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    AdjustWindowRectEx(&wr, style, FALSE, exStyle);
    int winDx = wr.right - wr.left;
    int winDy = wr.bottom - wr.top;

    Rect r = PositionHelpWindow(win, winDx, winDy);
    SetWindowPos(hwnd, nullptr, r.x, r.y, r.dx, r.dy, SWP_NOZORDER);
}

// total width of the key-caps for `keys` (used to size the columns and to
// right-align the caps within the gutter)
static int MeasureKeyCapsWidth(KeyboardHelpWnd* w, HDC hdc, Str keys) {
    StrVec toks;
    Str sep = ", ";
    Split(&toks, keys, sep);
    int n = std::min(toks.size, 4);
    if (n <= 0) {
        return 0;
    }
    int totalW = 0;
    for (int i = 0; i < n; i++) {
        Size sz = HdcMeasureText(hdc, toks.At(i), w->fontRow);
        totalW += sz.dx + (2 * w->capPadX);
    }
    totalW += w->capGap * (n - 1);
    return totalW;
}

// draw the shortcut(s) as key-caps, right-aligned within the gutter
static void DrawKeyCaps(KeyboardHelpWnd* w, HDC hdc, const Rect& gutter, Str keys, COLORREF capBg, COLORREF capBorder,
                        COLORREF txt) {
    StrVec toks;
    Str sep = ", ";
    Split(&toks, keys, sep);
    int n = std::min(toks.size, 4);
    if (n <= 0) {
        return;
    }
    int capH = w->rowTextH + DpiScale(w->hwnd, 5);

    int widths[4];
    int totalW = 0;
    for (int i = 0; i < n; i++) {
        Size sz = HdcMeasureText(hdc, toks.At(i), w->fontRow);
        widths[i] = sz.dx + (2 * w->capPadX);
        totalW += widths[i];
    }
    totalW += w->capGap * (n - 1);

    int x = gutter.x + gutter.dx - totalW;
    int y = gutter.y + ((gutter.dy - capH) / 2);
    int rad = DpiScale(w->hwnd, 5);

    HPEN pen = CreatePen(PS_SOLID, 1, capBorder);
    HBRUSH br = CreateSolidBrush(capBg);
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    HGDIOBJ oldBr = SelectObject(hdc, br);
    for (int i = 0; i < n; i++) {
        RoundRect(hdc, x, y, x + widths[i], y + capH, rad, rad);
        Rect capRc{x, y, widths[i], capH};
        SetTextColor(hdc, txt);
        HdcDrawText(hdc, toks.At(i), capRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX, w->fontRow);
        x += widths[i] + w->capGap;
    }
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBr);
    DeleteObject(pen);
    DeleteObject(br);
}

void KeyboardHelpWnd::PaintContent(HDC hdc, const Rect& client) {
    COLORREF bg = ThemeWindowControlBackgroundColor();
    COLORREF txt = ThemeWindowTextColor();
    COLORREF dim = AccentColor(txt, 90);
    COLORREF capBg = AccentColor(bg, 16);
    COLORREF capBorder = ThemeEdgeColor();

    SetBkColor(hdc, bg);
    HdcFillRectWithBkColor(hdc, client);
    SetBkMode(hdc, TRANSPARENT);

    // title
    SetTextColor(hdc, txt);
    Rect titleRc{pad, pad, client.dx - (2 * pad), (rowTextH * 2)};
    HdcDrawText(hdc, trans::GetTranslation("Keyboard Shortcuts"), titleRc,
                DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX, fontTitle);

    // close 'x' at the top-right
    Str xStr = "\xE2\x9C\x95"; // U+2715 MULTIPLICATION X
    Size xsz = HdcMeasureText(hdc, xStr, fontTitle);
    closeRc = Rect{client.dx - pad - xsz.dx, pad, xsz.dx, xsz.dy};
    SetTextColor(hdc, dim);
    HdcDrawText(hdc, xStr, closeRc, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX, fontTitle);

    // separator under the title
    int sepY = contentTop - DpiScale(hwnd, 10);
    Rect sepRc{pad, sepY, client.dx - (2 * pad), DpiScale(hwnd, 1)};
    SetBkColor(hdc, capBorder);
    HdcFillRectWithBkColor(hdc, sepRc);
    SetBkColor(hdc, bg);

    for (auto& s : sections) {
        int c = s.col;
        int cx = colX[c];
        int y = contentTop + s.y;
        Rect hdrRc{cx, y, colW[c], secTitleH};
        SetTextColor(hdc, txt);
        HdcDrawText(hdc, s.title, hdrRc, DT_LEFT | DT_BOTTOM | DT_SINGLELINE | DT_NOPREFIX, fontHdr);
        y += secTitleH;
        for (auto& r : s.rows) {
            Rect gutter{cx, y, colKeysW[c], rowH};
            DrawKeyCaps(this, hdc, gutter, r.keys, capBg, capBorder, txt);
            Rect descRc{cx + colKeysW[c] + keysDescGap, y, colDescW[c], rowH};
            SetTextColor(hdc, txt);
            HdcDrawText(hdc, r.desc, descRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS,
                        fontRow);
            y += rowH;
        }
    }

    // spacing guides: thin horizontal marks showing the inter-column gap and the
    // left / right margins, so the spacing is easy to eyeball
    if (gShowGuideLines) {
        COLORREF guide = RGB(0xff, 0x30, 0x30);
        int gy = contentTop + ((client.dy - contentTop) / 2);
        int contentRight = colX[1] + colW[1];
        Rect guides[] = {
            {0, gy, pad, DpiScale(hwnd, 2)},                                 // left margin
            {colX[0] + colW[0], gy, colGap, DpiScale(hwnd, 2)},              // gap between columns
            {contentRight, gy, client.dx - contentRight, DpiScale(hwnd, 2)}, // right margin
        };
        SetBkColor(hdc, guide);
        for (Rect& g : guides) {
            HdcFillRectWithBkColor(hdc, g);
        }
        SetBkColor(hdc, bg);
    }

    // footer hint
    Rect footRc{pad, client.dy - footerH - (pad / 2), client.dx - (2 * pad), footerH};
    SetTextColor(hdc, dim);
    HdcDrawText(hdc, trans::GetTranslation("Press ? to close"), footRc,
                DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX, fontRow);
}

LRESULT KeyboardHelpWnd::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_ERASEBKGND:
            return 1; // we paint the whole client, so skip the erase flicker
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            Rect client = HwndClientRect(hwnd);
            // double-buffer: a lot of small draws would otherwise flicker
            HDC memDC = CreateCompatibleDC(hdc);
            HBITMAP bmp = CreateCompatibleBitmap(hdc, client.dx, client.dy);
            HGDIOBJ oldBmp = SelectObject(memDC, bmp);
            PaintContent(memDC, client);
            BitBlt(hdc, 0, 0, client.dx, client.dy, memDC, 0, 0, SRCCOPY);
            SelectObject(memDC, oldBmp);
            DeleteObject(bmp);
            DeleteDC(memDC);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_SETCURSOR: {
            POINT pt;
            GetCursorPos(&pt);
            ScreenToClient(hwnd, &pt);
            int grow = DpiScale(hwnd, 6);
            Rect closeHit{closeRc.x - grow, closeRc.y - grow, closeRc.dx + (2 * grow), closeRc.dy + (2 * grow)};
            if (pt.y < contentTop && !closeHit.Contains(Point(pt.x, pt.y))) {
                SetCursor(LoadCursorW(nullptr, IDC_SIZEALL));
                return TRUE;
            }
            break;
        }
        case WM_LBUTTONDOWN: {
            int x = (short)LOWORD(lp);
            int y = (short)HIWORD(lp);
            int grow = DpiScale(hwnd, 6);
            Rect hit{closeRc.x - grow, closeRc.y - grow, closeRc.dx + (2 * grow), closeRc.dy + (2 * grow)};
            if (hit.Contains(Point(x, y))) {
                ScheduleCloseKeyboardHelp();
                return 0;
            }
            // dragging the title band moves the window (it has no title bar of
            // its own); hand off to the standard move loop
            if (y < contentTop) {
                ReleaseCapture();
                SendMessageW(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
                return 0;
            }
            break;
        }
    }
    return WndProcDefault(hwnd, msg, wp, lp);
}

bool KeyboardHelpWnd::PreTranslateMessage(MSG& msg) {
    if (msg.message != WM_KEYDOWN) {
        return false;
    }
    // '?' (Shift + '/') toggles the sheet back off, matching how it opened. The
    // sheet otherwise stays up until '?' or the close button - it doesn't close
    // on focus loss, so it can sit alongside the document as a reference.
    if (msg.wParam == VK_OEM_2 && IsShiftPressed()) {
        ScheduleCloseKeyboardHelp();
        return true;
    }
    return false;
}

bool KeyboardHelpWnd::Create(MainWindow* win) {
    this->win = win;
    {
        CreateCustomArgs args;
        args.visible = false;
        args.style = WS_POPUPWINDOW;
        args.title = "Keyboard Shortcuts";
        CreateCustom(args);
    }
    if (!hwnd) {
        return false;
    }
    HDC hdc = GetDC(hwnd);
    BuildContent(hdc);
    ReleaseDC(hwnd, hdc);
    SetIsVisible(true);
    HwndSetFocus(hwnd);
    return true;
}

void ToggleKeyboardHelp(MainWindow* win) {
    if (gKeyboardHelpWnd) {
        ScheduleCloseKeyboardHelp();
        return;
    }
    if (!win) {
        return;
    }
    auto* wnd = new KeyboardHelpWnd();
    bool ok = wnd->Create(win);
    if (!ok) {
        delete wnd;
        return;
    }
    gKeyboardHelpWnd = wnd;
    gHwndToActivateOnClose = win->hwndFrame;
}
