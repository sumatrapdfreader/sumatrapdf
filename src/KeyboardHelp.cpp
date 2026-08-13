/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Win.h"
#include "base/Dpi.h"
#include "base/UITask.h"

#include "wingui/UIModels.h"
#include "wingui/Layout.h"
#include "wingui/WinGui.h"
#include "wingui/PlatformFont.h"
#include "wingui/Gfx.h"
#include "wingui/VirtCtrl.h"

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
//
// The sheet is a layout tree of VirtCtrls: an HBox of two columns, each
// column a Table of (key-caps, description) rows with the section headers as
// full-width spanning cells.

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

// a row while the sheet is being assembled; the strings live in the temp arena
// until the controls copy them
struct KbRowDef {
    Str keys; // shortcut(s), e.g. "↑, K"
    Str desc; // command name
};

struct KbSectionData {
    Str title;
    Vec<KbRowDef> rows;
    int height = 0; // estimated, only used to balance the two columns
    int col = 0;    // 0 (left) or 1 (right)
};

static Kind kindKbKeyCaps = "kbKeyCaps";

// the shortcut(s) of one row, drawn as rounded key-caps
struct KbKeyCaps : VirtCtrl {
    StrVec toks; // one per cap, at most 4
    PlatformFont* font = nullptr;
    int capPadX = 0;
    int capGap = 0;
    int capDy = 0;
    int radius = 0;

    KbKeyCaps();
    ~KbKeyCaps() override = default;

    Size GetIdealSize() override;
    void Paint(VirtPaintCtx&) override;
};

struct KeyboardHelpWnd : WindowBase {
    MainWindow* win = nullptr;

    PlatformFont* fontTitle = nullptr; // window title
    PlatformFont* fontHdr = nullptr;   // section headers
    PlatformFont* fontRow = nullptr;   // rows

    // the sheet's controls. `container` only owns them - this window positions
    // so the root never re-layouts them
    VBox* container = nullptr;
    VirtText* title = nullptr;
    VirtLink* closeBtn = nullptr;
    VirtLine* separator = nullptr;
    HBox* columns = nullptr;
    VirtText* footer = nullptr;
    // section headers and row descriptions, so SyncColors() can reach them
    Vec<VirtText*> texts;

    // metrics, in client pixels, computed in BuildContent
    int pad = 0;
    int contentTop = 0;
    int footerH = 0;

    ~KeyboardHelpWnd() override;
    bool Create(MainWindow* win);
    void BuildContent();
    void SyncColors();
    void PaintContent(HDC hdc, const Rect& client);
    void OnPaint(WindowBase::PaintEvent* ev);
    void OnSetCursor(WindowBase::SetCursorEvent* ev);
    void OnMouseEvent(WindowBase::MouseEvent* ev);
    void OnKeyDown(KeyEvent* ev);
};

static KeyboardHelpWnd* gKeyboardHelpWnd = nullptr;
static HWND gHwndToActivateOnClose = nullptr;

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

struct BoldFontCacheEntry {
    BoldFontCacheEntry* next;
    HFONT src;
    PlatformFont* bold;
};

// A PlatformFont adopts an HFONT and keeps it for the life of the process, so
// the bold variants can't be created and destroyed with the window. There are
// only ever a handful (one per app font per DPI), so they are cached here and
// live as long as the fonts they derive from.
static BoldFontCacheEntry* gBoldFonts = nullptr;

static PlatformFont* GetBoldFont(HFONT src) {
    for (BoldFontCacheEntry* e = gBoldFonts; e; e = e->next) {
        if (e->src == src) {
            return e->bold;
        }
    }
    HFONT bold = CreateBoldFontFrom(src);
    PlatformFont* font = GetPlatformFont(bold ? bold : src);
    auto* e = New<BoldFontCacheEntry>(GetPermArena());
    e->src = src;
    e->bold = font;
    ListInsertFront(&gBoldFonts, e);
    return font;
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

KeyboardHelpWnd::~KeyboardHelpWnd() {}

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

//--- the controls

KbKeyCaps::KbKeyCaps() {
    kind = kindKbKeyCaps;
    flags |= vwfNoHitTest;
}

Size KbKeyCaps::GetIdealSize() {
    int n = toks.size;
    if (n <= 0) {
        return {0, capDy};
    }
    int dx = 0;
    for (int i = 0; i < n; i++) {
        Size sz = PlatformFontMeasureText(font, toks.At(i));
        dx += sz.dx + (2 * capPadX);
    }
    dx += capGap * (n - 1);
    return {dx, capDy};
}

void KbKeyCaps::Paint(VirtPaintCtx& ctx) {
    int n = toks.size;
    if (n <= 0) {
        return;
    }
    COLORREF bg = ThemeWindowControlBackgroundColor();
    COLORREF capBg = AccentColor(bg, 16);
    COLORREF capBorder = ThemeEdgeColor();
    COLORREF txt = ThemeWindowTextColor();

    Rect r = ctx.bounds;
    int x = r.x;
    int y = r.y + ((r.dy - capDy) / 2);

    for (int i = 0; i < n; i++) {
        Str tok = toks.At(i);
        int dx = PlatformFontMeasureText(font, tok).dx + (2 * capPadX);
        Rect capRc{x, y, dx, capDy};
        ctx.gfx->FillRoundedRect(capRc, radius, capBg, capBorder);
        ctx.gfx->DrawText(tok, capRc, gfxTextCenter | gfxTextVCenter | gfxTextEllipsis, font, txt);
        x += dx + capGap;
    }
}

//--- building the sheet

static void OnCloseClicked(VirtMouseEvent*) {
    ScheduleCloseKeyboardHelp();
}

// collects the sections that have at least one bound shortcut and assigns each
// to a column, splitting where the two columns end up closest in height
static void CollectSections(Vec<KbSectionData>& out, int secTitleH, int rowH, int secGap) {
    for (auto& def : gKbSectionDefs) {
        KbSectionData s;
        s.title = trans::GetTranslation(Str(def.title));
        for (const int* c = def.cmds; *c; c++) {
            TempStr keys = ShortcutsForCmdTemp(*c, 2);
            if (keys.len == 0) {
                continue;
            }
            TempStr desc = CmdDescTemp(*c);
            if (desc.len == 0) {
                continue;
            }
            s.rows.Append({keys, desc});
        }
        if (len(s.rows) == 0) {
            continue;
        }
        s.height = secTitleH + (len(s.rows) * rowH);
        out.Append(s);
    }

    // the left column gets a prefix of the sections, so they stay in reading
    // order (Navigation top-left) while the columns end up about as tall
    int n = len(out);
    int total = 0;
    for (auto& s : out) {
        total += s.height + secGap;
    }
    int bestSplit = n;
    int bestBalance = total;
    int prefix = 0;
    for (int k = 1; k < n; k++) {
        prefix += out[k - 1].height + secGap;
        int bal = std::max(prefix, total - prefix);
        if (bal < bestBalance) {
            bestBalance = bal;
            bestSplit = k;
        }
    }
    for (int i = 0; i < n; i++) {
        out[i].col = (i < bestSplit) ? 0 : 1;
    }
}

void KeyboardHelpWnd::BuildContent() {
    HFONT hfontRow = GetAppFont(hwnd);
    fontRow = GetPlatformFont(hfontRow);
    fontHdr = GetBoldFont(hfontRow);
    fontTitle = GetBoldFont(GetAppBiggerFont(hwnd));

    Size szRow = PlatformFontMeasureText(fontRow, "Ag");
    Size szHdr = PlatformFontMeasureText(fontHdr, "Ag");
    Size szTitle = PlatformFontMeasureText(fontTitle, "Ag");

    int rem = DpiScale(hwnd, 16); // 1rem == 16px at 96 DPI
    pad = DpiScale(hwnd, 20);
    int titleGap = DpiScale(hwnd, 16);
    int rowGap = DpiScale(hwnd, 8);
    int secTitleH = szHdr.dy + DpiScale(hwnd, 8);
    int secGap = DpiScale(hwnd, 14);
    int colGap = rem; // gap between the two columns
    int capPadX = DpiScale(hwnd, 7);
    int capGap = DpiScale(hwnd, 5);
    int keysDescGap = DpiScale(hwnd, 12);
    contentTop = pad + szTitle.dy + titleGap;
    footerH = szRow.dy + DpiScale(hwnd, 6);

    Vec<KbSectionData> sections;
    CollectSections(sections, secTitleH, szRow.dy + rowGap, secGap);

    container = new VBox();

    title = new VirtText(trans::GetTranslation("Keyboard Shortcuts"), fontTitle);
    container->AddChild(title);

    // U+2715 MULTIPLICATION X
    closeBtn = new VirtLink("\xE2\x9C\x95", fontTitle);
    closeBtn->align = VirtTextAlign::Center;
    closeBtn->onClick = MkFunc1Void(OnCloseClicked);
    container->AddChild(closeBtn);

    separator = new VirtLine();
    separator->thickness = DpiScale(hwnd, 1);
    container->AddChild(separator);

    columns = new HBox();
    columns->alignCross = CrossAxisAlign::CrossStart;
    Table* tables[2] = {new Table(), new Table()};
    for (Table* t : tables) {
        t->colGap = keysDescGap;
        t->rowGap = rowGap;
    }
    columns->AddChild(tables[0]);
    columns->AddChild(new Spacer(colGap, 0));
    columns->AddChild(tables[1]);
    container->AddChild(columns);

    footer = new VirtText(trans::GetTranslation("Press ? to close"), fontRow);
    container->AddChild(footer);

    // one table per column: a section header spans both table columns, each row
    // is (key-caps, description)
    int nRows[2] = {0, 0};
    for (auto& s : sections) {
        nRows[s.col] += 1 + len(s.rows);
    }
    tables[0]->SetSize(nRows[0], 2);
    tables[1]->SetSize(nRows[1], 2);

    int capDy = szRow.dy + DpiScale(hwnd, 5);
    int radius = DpiScale(hwnd, 5);
    int rowAt[2] = {0, 0};
    for (auto& s : sections) {
        Table* t = tables[s.col];
        int& row = rowAt[s.col];

        // the padding is the gap that separates the header from the section
        // above: a column is a single table, and its rowGap is uniform
        auto* hdr = new VirtText(s.title, fontHdr);
        hdr->padding.top = (row == 0) ? 0 : secGap;
        texts.Append(hdr);
        TableCell& hdrCell = t->SetCell(row, 0, hdr, 1, 2);
        hdrCell.alignV = CrossAxisAlign::CrossEnd;
        row++;

        for (auto& r : s.rows) {
            auto* caps = new KbKeyCaps();
            caps->font = fontRow;
            caps->capPadX = capPadX;
            caps->capGap = capGap;
            caps->capDy = capDy;
            caps->radius = radius;
            // at most 4 caps per row, like the sheet has always shown
            Split(&caps->toks, r.keys, ", ", false, 4);
            // the caps sit against the description, like a gutter
            TableCell& capsCell = t->SetCell(row, 0, caps);
            capsCell.alignH = CrossAxisAlign::CrossEnd;
            capsCell.alignV = CrossAxisAlign::CrossCenter;

            auto* desc = NewVirtText({.s = r.desc, .font = fontRow, .ellipsis = true});
            texts.Append(desc);
            TableCell& descCell = t->SetCell(row, 1, desc);
            descCell.alignV = CrossAxisAlign::CrossCenter;
            row++;
        }
    }

    layout = container;

    Size colsSize = columns->Layout(ExpandInf());
    int rightPad = rem / 2; // 0.5rem right margin
    int clientDx = pad + colsSize.dx + rightPad;
    int clientDy = contentTop + colsSize.dy + DpiScale(hwnd, 12) + footerH + DpiScale(hwnd, 6);

    RECT wr = {0, 0, clientDx, clientDy};
    DWORD style = (DWORD)GetWindowLongPtrW(hwnd, GWL_STYLE);
    DWORD exStyle = (DWORD)GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    AdjustWindowRectEx(&wr, style, FALSE, exStyle);
    int winDx = wr.right - wr.left;
    int winDy = wr.bottom - wr.top;

    Rect r = PositionHelpWindow(win, winDx, winDy);
    SetWindowPos(hwnd, nullptr, r.x, r.y, r.dx, r.dy, SWP_NOZORDER);

    // this window places the pieces itself, so the tree is only collected, not
    // laid out
    RefreshVirtTops(hwnd, layout, Rect{0, 0, clientDx, clientDy}, &vroot);

    Size szClose = closeBtn->GetIdealSize();
    int closeGrow = DpiScale(hwnd, 6);
    title->SetBounds({pad, pad, clientDx - (2 * pad) - szClose.dx, szTitle.dy});
    closeBtn->SetBounds({clientDx - pad - szClose.dx - closeGrow, pad - closeGrow, szClose.dx + (2 * closeGrow),
                         szClose.dy + (2 * closeGrow)});
    int sepY = contentTop - DpiScale(hwnd, 10);
    separator->SetBounds({pad, sepY, clientDx - (2 * pad), separator->thickness});
    columns->SetBounds({pad, contentTop, colsSize.dx, colsSize.dy});
    footer->SetBounds({pad, clientDy - footerH - (pad / 2), clientDx - (2 * pad), footerH});
}

// colors are read from the theme on every paint, so a theme change shows
// through without rebuilding the tree
void KeyboardHelpWnd::SyncColors() {
    COLORREF txt = ThemeWindowTextColor();
    COLORREF dim = AccentColor(txt, 90);
    title->textColor = txt;
    closeBtn->textColor = dim;
    separator->color = ThemeEdgeColor();
    footer->textColor = dim;
    for (VirtText* t : texts) {
        t->textColor = txt;
    }
}

void KeyboardHelpWnd::PaintContent(HDC hdc, const Rect& client) {
    COLORREF bg = ThemeWindowControlBackgroundColor();
    SetBkColor(hdc, bg);
    HdcFillRectWithBkColor(hdc, client);
    SetBkMode(hdc, TRANSPARENT);

    SyncColors();
    GfxHdc gfx(hdc);
    if (vroot) {
        vroot->Paint(&gfx, client);
    }
}

void KeyboardHelpWnd::OnPaint(WindowBase::PaintEvent* ev) {
    Rect client = HwndClientRect(hwnd);
    // double-buffer: a lot of small draws would otherwise flicker
    HDC memDC = CreateCompatibleDC(ev->hdc);
    HBITMAP bmp = CreateCompatibleBitmap(ev->hdc, client.dx, client.dy);
    HGDIOBJ oldBmp = SelectObject(memDC, bmp);
    PaintContent(memDC, client);
    BitBlt(ev->hdc, 0, 0, client.dx, client.dy, memDC, 0, 0, SRCCOPY);
    SelectObject(memDC, oldBmp);
    DeleteObject(bmp);
    DeleteDC(memDC);
}

// title-band cursor. VirtTree for content is handled after this in WndProcDefault
void KeyboardHelpWnd::OnSetCursor(WindowBase::SetCursorEvent* ev) {
    Point pt = HwndGetCursorPos(hwnd);
    if (pt.y < contentTop) {
        SetCursor(LoadCursorW(nullptr, IDC_SIZEALL));
        ev->result = TRUE;
        ev->didHandle = true;
    }
}

// dragging the title band moves the window (it has no title bar of its own)
void KeyboardHelpWnd::OnMouseEvent(WindowBase::MouseEvent* ev) {
    if (ev->msg != WM_LBUTTONDOWN) {
        return;
    }
    int y = (short)HIWORD(ev->lparam);
    if (y < contentTop) {
        ReleaseCapture();
        SendMessageW(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
        ev->result = 0;
    }
}

void KeyboardHelpWnd::OnKeyDown(KeyEvent* ev) {
    // '?' (Shift + '/') toggles the sheet back off, matching how it opened. The
    // sheet otherwise stays up until '?' or the close button - it doesn't close
    // on focus loss, so it can sit alongside the document as a reference.
    if (ev->vkey == VK_OEM_2 && ev->isShift) {
        ScheduleCloseKeyboardHelp();
        ev->didHandle = true;
    }
}

bool KeyboardHelpWnd::Create(MainWindow* win) {
    this->win = win;
    onPaint = MkMethod1<KeyboardHelpWnd, WindowBase::PaintEvent*, &KeyboardHelpWnd::OnPaint>(this);
    onSetCursor = MkMethod1<KeyboardHelpWnd, WindowBase::SetCursorEvent*, &KeyboardHelpWnd::OnSetCursor>(this);
    onMouseEvent = MkMethod1<KeyboardHelpWnd, WindowBase::MouseEvent*, &KeyboardHelpWnd::OnMouseEvent>(this);
    onKeyDown = MkMethod1<KeyboardHelpWnd, KeyEvent*, &KeyboardHelpWnd::OnKeyDown>(this);
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
    BuildContent();
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
