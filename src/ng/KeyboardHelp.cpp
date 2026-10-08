/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// The section tables, the data source and the two-column layout are orig's
// KeyboardHelp.cpp. ng: orig has two renderers, a win32 VirtCtrl tree and a
// PlatformWindow that paints key caps itself; this is the second one written as
// gpui elements - the title row with a close button, a divider and two columns
// of sections, each a bold header over [key caps] [description] rows.

#include "gui/GpuiBridge.h"

#include "base/UITask.h"
#if OS_WIN
#include "base/Win.h"
#endif
#include "gui/UIModels.h"
#include "VirtKeys.h"

#include "Settings.h"
#include "AppSettings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "MainWindow.h"
#include "ShortcutParse.h"
#include "Accelerators.h"
#include "Theme.h"
#include "SumatraConfig.h"
#include "SumatraPDF.h"
#include "Translations.h"
#include "gui/Dpi.h"
#include "gui/AppShell.h"
#include "gui/ToolWindow.h"
#include "Commands.h"
#include "KeyboardHelp.h"

#include "SumatraLog.h"

// A section is an ordered list of command ids (terminated by 0). The keyboard
// shortcut for each command is looked up from its actual binding, not hard-coded
// here, so re-binding or clearing a shortcut is reflected automatically.
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
    CmdZoomFitPage, CmdZoomFitWidth, CmdZoomActualSize,
    CmdToggleZoom, CmdSinglePageView, CmdFacingView,
    CmdBookView, CmdToggleContinuousView,
    CmdRotateLeft, CmdRotateRight, CmdToggleFullscreen, 0,
};
static const int kSecDoc[] = {
    CmdOpenFile, CmdSaveAs, CmdPrint, CmdReloadDocument,
    CmdClose, CmdNewWindow, CmdOpenNextFileInFolder,
    CmdOpenPrevFileInFolder, CmdRenameFile, CmdProperties, 0,
};
static const int kSecFind[] = {
    CmdFindFirst, CmdFindNext, CmdFindPrev,
    CmdSelectAll, CmdCopySelection, CmdSelectTextViaKeyboard,
    CmdToggleKeyboardLinkFollowing, 0,
};
static const int kSecTabs[] = {
    CmdNextTabSmart, CmdNextTab, CmdPrevTab,
    CmdMoveTabLeft, CmdMoveTabRight, CmdReopenLastClosedFile, 0,
};
static const int kSecAnnot[] = {
    CmdCreateAnnotHighlight, CmdCreateAnnotUnderline, CmdSaveAnnotations,
    CmdDeleteAnnotation, 0,
};
static const int kSecIface[] = {
    CmdCommandPalette, CmdToggleBookmarks, CmdToggleToolbar, CmdToggleMenuBar,
    CmdToggleCursorPosition, CmdTogglePageInfo,
    CmdFavoriteAdd, CmdFavoriteToggle, CmdHelpOpenManual,0,
};
// clang-format on

struct KbSectionDef {
    const char* title;
    const int* commands;
    int column; // which of the two columns this section is laid out in
};

// column 0 (left): Navigation, Interface, Find & Select
// column 1 (right): View & Zoom, Document, Tabs, Annotations
static const KbSectionDef kSections[] = {
    {"Navigation", kSecNav, 0},    {"View & Zoom", kSecView, 1},   {"Interface", kSecIface, 0},
    {"Document", kSecDoc, 1},      {"Find & Select", kSecFind, 0}, {"Tabs", kSecTabs, 1},
    {"Annotations", kSecAnnot, 1},
};

static bool IsHelpListedCmd(int cmdId) {
    for (const KbSectionDef& definition : kSections) {
        for (const int* id = definition.commands; *id; id++) {
            if (*id == cmdId) {
                return true;
            }
        }
    }
    return false;
}

// fallback shortcuts for platforms without an accelerator table (the SumatraPDF
// data source looks up the real bindings instead). {id, ""} means "no default".
// clang-format off
static const struct {
    int id;
    const char* shortcut;
} kFallbackShortcuts[] = {
    {CmdScrollUp, "Up, K"}, {CmdScrollDown, "Down, J"}, {CmdScrollLeft, "Left, H"}, {CmdScrollRight, "Right, L"},
    {CmdScrollUpPage, "Page Up"}, {CmdScrollDownPage, "Page Down"}, {CmdGoToNextPage, "N"}, {CmdGoToPrevPage, "P"},
    {CmdGoToFirstPage, "Home"}, {CmdGoToLastPage, "End"}, {CmdGoToPage, "Ctrl + G"}, {CmdNavigateBack, "Alt + Left"},
    {CmdNavigateForward, "Alt + Right"}, {CmdZoomIn, "Ctrl + +"}, {CmdZoomOut, "Ctrl + -"}, {CmdZoomFitPage, "Ctrl + 0"},
    {CmdZoomFitWidth, "Ctrl + 2"}, {CmdZoomActualSize, "Ctrl + 1"}, {CmdToggleZoom, "Z"}, {CmdSinglePageView, "Ctrl + 6"},
    {CmdFacingView, "Ctrl + 7"}, {CmdBookView, "Ctrl + 8"}, {CmdToggleContinuousView, "C"}, {CmdRotateLeft, "["},
    {CmdRotateRight, "]"}, {CmdToggleFullscreen, "F"}, {CmdToggleAutomaticallyScroll, "Ctrl + Shift + H"},
    {CmdToggleReadingBar, ""}, {CmdToggleReadingBarInvert, ""},
    {CmdOpenFile, "Ctrl + O"}, {CmdSaveAs, "Ctrl + S"},
    {CmdPrint, "Ctrl + P"}, {CmdReloadDocument, "R"}, {CmdClose, "Ctrl + W"}, {CmdNewWindow, "Ctrl + N"},
    {CmdOpenNextFileInFolder, "Ctrl + Shift + Right"}, {CmdOpenPrevFileInFolder, "Ctrl + Shift + Left"},
    {CmdRenameFile, "F2"}, {CmdProperties, "Ctrl + D"}, {CmdFindFirst, "Ctrl + F"}, {CmdFindNext, "F3"},
    {CmdFindPrev, "Shift + F3"}, {CmdSelectAll, "Ctrl + A"}, {CmdCopySelection, "Ctrl + C"},
    {CmdSelectTextViaKeyboard, "F7"}, {CmdToggleKeyboardLinkFollowing, "Shift + F"}, {CmdNextTabSmart, "Ctrl + Tab"},
    {CmdNextTab, "Ctrl + Page Down"}, {CmdPrevTab, "Ctrl + Page Up"}, {CmdMoveTabLeft, "Ctrl + Shift + Page Up"},
    {CmdMoveTabRight, "Ctrl + Shift + Page Down"}, {CmdReopenLastClosedFile, "Ctrl + Shift + T"},
    {CmdCreateAnnotHighlight, "A"}, {CmdCreateAnnotUnderline, "U"}, {CmdSaveAnnotations, "Ctrl + Shift + S"},
    {CmdDeleteAnnotation, "Ctrl + Delete"}, {CmdToggleBookmarks, "F12"}, {CmdToggleToolbar, "F8"},
    {CmdToggleMenuBar, "F9"}, {CmdToggleCursorPosition, "M"}, {CmdTogglePageInfo, "I"}, {CmdCommandPalette, "Ctrl + K"},
    {CmdFavoriteAdd, "Ctrl + B"}, {CmdHelpOpenManual, "F1"}, {CmdToggleKeyboardHelp, "?"},
};
// clang-format on

struct DefaultKeyboardHelpDataSource : KeyboardHelpDataSource {
    Str Translate(Str s) override { return s; }

    TempStr CommandDescriptionTemp(int cmdId) override { return str::DupTemp(GetCommandDescription(cmdId)); }

    TempStr CommandShortcutTemp(int cmdId, int) override {
        for (const auto& e : kFallbackShortcuts) {
            if (e.id == cmdId) {
                return str::DupTemp(Str(e.shortcut));
            }
        }
        return {};
    }
};

static DefaultKeyboardHelpDataSource gDefaultDataSource;

KeyboardHelpDataSource* GetDefaultKeyboardHelpDataSource() {
    return &gDefaultDataSource;
}

struct SumatraKeyboardHelpDataSource : KeyboardHelpDataSource {
    Str Translate(Str s) override { return trans::GetTranslation(s); }

    TempStr CommandDescriptionTemp(int cmdId) override {
        Str description = GetCommandDescription(cmdId);
        if (len(description) == 0) {
            return {};
        }
        return str::DupTemp(trans::GetTranslation(description));
    }

    TempStr CommandShortcutTemp(int cmdId, int maxCount) override { return ShortcutsForCmdTemp(cmdId, maxCount); }
};

static SumatraKeyboardHelpDataSource gSumatraDataSource;

struct KeyboardHelpDlg {
    MainWindow* win = nullptr;
    bool visible = false;
    KeyboardHelpDataSource* dataSource = nullptr;
    // the sections are taller than the dialog
    float scrollY = 0;
    // the scrolled view as laid out last frame, for PageUp / PageDown
    gpui::Bounds viewBounds{};
    // orig's window, where the platform can have one (gui/ToolWindow.h);
    // null: a dialog in the frame
    ToolWindow* tw = nullptr;
    bool parentFullscreen = false;
    // the close button as laid out last frame
    gpui::Bounds closeBounds{};
    // the two columns at their natural height (dips), as orig's fonts measure
    float columnsDy = 0;
};

static KeyboardHelpDlg gKbHelp;

struct KeyboardHelpView {
    static void OnClose(KeyboardHelpView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnScroll(KeyboardHelpView* self, gp::Ctx* cx, const gp::ScrollEvent* ev);
};

static gp::Entity<KeyboardHelpView> gKbHelpView;

// the dialog in the frame; a window of its own is not the frame's business
bool IsKeyboardHelpVisible() {
    return gKbHelp.visible && !gKbHelp.tw;
}

void CloseKeyboardHelp() {
    if (!gKbHelp.visible) {
        return;
    }
    gKbHelp.visible = false;
    if (gKbHelp.tw) {
        ToolWindowClose(gKbHelp.tw);
        gKbHelp.tw = nullptr;
    }
    AppShellInvalidate(gKbHelp.win);
}

static void KbHelpOpenToolWindow(MainWindow* win);

void ToggleKeyboardHelp(const KeyboardHelpArgs& args) {
    if (gKbHelp.visible) {
        CloseKeyboardHelp();
        return;
    }
    if (!IsMainWindowValidAndNotClosing(args.win)) {
        return;
    }
    gKbHelp.win = args.win;
    gKbHelp.dataSource = args.dataSource ? args.dataSource : GetDefaultKeyboardHelpDataSource();
    gKbHelp.visible = true;
    gKbHelp.scrollY = 0;
    gKbHelp.parentFullscreen = args.parentFullscreen;
    KbHelpOpenToolWindow(args.win);
    logf("KeyboardHelp: shown\n");
    AppShellInvalidate(args.win);
}

void ToggleKeyboardHelp(MainWindow* win) {
    if (!win) {
        return;
    }
    KeyboardHelpArgs args;
    args.win = win;
    args.parentFullscreen = win->isFullScreen || win->InPresentation();
    args.dataSource = &gSumatraDataSource;
    ToggleKeyboardHelp(args);
}

// orig's OnHelpKeyDown: the keys scroll the help. ng: gpui clamps the offset
// it is handed to the content
bool KeyboardHelpOnKeyDown(int vk) {
    if (!gKbHelp.visible) {
        return false;
    }
    constexpr float kLineDy = 24;
    float pageDy = gKbHelp.viewBounds.h > 0 ? gKbHelp.viewBounds.h : 400;
    float y = gKbHelp.scrollY;
    switch (vk) {
        case VK_UP:
            y -= kLineDy;
            break;
        case VK_DOWN:
            y += kLineDy;
            break;
        case VK_PRIOR:
            y -= pageDy;
            break;
        case VK_NEXT:
            y += pageDy;
            break;
        case VK_HOME:
            y = 0;
            break;
        case VK_END:
            y = 1e6f;
            break;
        default:
            return false;
    }
    gKbHelp.scrollY = std::max(0.f, y);
    return true;
}

void KeyboardHelpView::OnScroll(KeyboardHelpView*, gp::Ctx* cx, const gp::ScrollEvent* ev) {
    gKbHelp.scrollY = ev->offsetY;
    gp::Notify(cx);
    AppShellInvalidate(gKbHelp.win);
}

void KeyboardHelpView::OnClose(KeyboardHelpView*, gp::Ctx* cx, const gp::ClickEvent*) {
    CloseKeyboardHelp();
    gp::Notify(cx);
}

// the rows of the help: orig's BuildKeyboardHelpLayout, as data
struct KbSection {
    TempStr title;
    StrVec keys;
    StrVec descriptions;
    int column = 0;
};

constexpr int kMaxKbSections = (int)dimof(kSections) + 1;

static int CollectKbSections(KeyboardHelpDataSource* ds, KbSection* sections) {
    int n = 0;
    for (const KbSectionDef& definition : kSections) {
        KbSection& sec = sections[n];
        for (const int* cmdId = definition.commands; *cmdId; cmdId++) {
            TempStr k = ds->CommandShortcutTemp(*cmdId, 2);
            TempStr d = ds->CommandDescriptionTemp(*cmdId);
            // skip commands with no keyboard shortcut (e.g. un-bound by the user)
            if (len(k) == 0 || len(d) == 0) {
                continue;
            }
            sec.keys.Append(k);
            sec.descriptions.Append(d);
        }
        if (len(sec.keys) == 0) {
            continue;
        }
        sec.title = str::DupTemp(ds->Translate(Str(definition.title)));
        sec.column = definition.column;
        n++;
    }

    // Shortcuts from advanced settings that are not already a rebinding of a
    // command listed above (named entries, commands with args, unlisted cmds)
    if (!gSettings || !gSettings->shortcuts) {
        return n;
    }
    KbSection& sec = sections[n];
    for (Shortcut* sc : *gSettings->shortcuts) {
        if (!sc || str::IsEmptyOrWhiteSpace(sc->key) || sc->cmdId <= 0) {
            continue;
        }
        CustomCommand* cmd = FindCustomCommand(sc->cmdId);
        int orig = cmd ? cmd->origId : sc->cmdId;
        bool extra = cmd && cmd->firstArg;
        if (!extra && len(sc->name) == 0 && IsHelpListedCmd(orig)) {
            continue;
        }
        TempStr k = ShortcutsForCmdTemp(sc->cmdId, 2);
        if (len(k) == 0) {
            k = str::DupTemp(sc->key);
        }
        TempStr d;
        if (len(sc->name) > 0) {
            d = str::DupTemp(sc->name);
        } else {
            d = ds->CommandDescriptionTemp(orig);
            if (len(d) == 0) {
                d = str::DupTemp(sc->cmd);
            }
        }
        if (len(k) == 0 || len(d) == 0) {
            continue;
        }
        sec.keys.Append(k);
        sec.descriptions.Append(d);
    }
    if (len(sec.keys) > 0) {
        sec.title = str::DupTemp(ds->Translate(StrL("Custom")));
        sec.column = 0;
        n++;
    }
    return n;
}

// orig's sizes (BuildKeyboardHelpLayout, VirtRichText::LayoutText)
constexpr float kKbColumnGap = 16;
constexpr float kKbKeysDescGap = 12;
constexpr float kKbRowGap = 8;
constexpr float kKbSectionGap = 14;
constexpr float kKbCapPadX = 7;
constexpr float kKbCapPadY = 1;
constexpr float kKbCapRadius = 5;

// one rounded cap per token, as orig's VirtRichText draws them. capGap: the
// font's space; capDy: 0 for the text's own height
static gp::El* KeyCaps(gp::Ctx* cx, Str keys, float fontPx, float capGap, float capDy) {
    const gp::Theme& th = gp::ThemeNow(cx->app);
    gp::El* row = gp::Div(cx->a)->FlexRow()->ItemsCenter()->JustifyEnd()->Gap(capGap);
    StrVec tokens;
    Split(&tokens, keys, StrL(", "), false, 4);
    for (Str token : tokens) {
        gp::El* cap = gp::Div(cx->a)
                          ->FlexRow()
                          ->ItemsCenter()
                          ->PadX(kKbCapPadX)
                          ->Radius(kKbCapRadius)
                          ->Border(1, th.border)
                          ->Bg(th.muted)
                          ->Shrink0()
                          ->Child(gp::TextEl(cx->a, GpuiDup(cx->a, token))->Font(fontPx)->Fg(th.foreground));
        if (capDy > 0) {
            cap->H(capDy);
        }
        row->Child(cap);
    }
    return row;
}

// --- a window of its own (Windows) ------------------------------------------

// orig's layout: 20 around, the title row, 6, a line, 10, the columns
constexpr float kKbHelpPad = 20;
constexpr float kKbHelpSepGapTop = 6;
constexpr float kKbHelpSepGapBottom = 10;
// orig's close button: a 16 glyph with 4 around
constexpr float kKbHelpCloseDx = 24;

static Str KbHelpToolTitle() {
    KeyboardHelpDataSource* ds = gKbHelp.dataSource ? gKbHelp.dataSource : GetDefaultKeyboardHelpDataSource();
    return ds->Translate(StrL("Keyboard Shortcuts"));
}

// orig's closeOnEsc, OnHelpWndProc ('?' closes it) and OnHelpKeyDown
static bool KbHelpToolOnKey(MainWindow*, gp::Ctx*, const gp::KeyEvent* ev) {
    bool questionMark = ev->vk == VK_OEM_2 && ev->shift && !ev->ctrl && !ev->alt;
    if (ev->vk == VK_ESCAPE || questionMark) {
        CloseKeyboardHelp();
        return true;
    }
    if (!KeyboardHelpOnKeyDown(ev->vk)) {
        return false;
    }
    AppShellInvalidate(gKbHelp.win);
    return true;
}

static void KbHelpToolOnClosed(MainWindow*) {
    gKbHelp.tw = nullptr;
    CloseKeyboardHelp();
}

// orig's OnNcHitTest: everything but the close button moves the window. ng:
// and but the scrollbar of the columns
static bool KbHelpIsClientPoint(MainWindow*, Point pt) {
    gp::Point p{(float)pt.x, (float)pt.y};
    if (gKbHelp.closeBounds.Contains(p)) {
        return true;
    }
    const gp::Bounds& v = gKbHelp.viewBounds;
    bool scrolls = gKbHelp.columnsDy > v.h + 1;
    constexpr float kScrollbarDx = 16;
    return scrolls && v.Contains(p) && p.x >= v.x + v.w - kScrollbarDx;
}

// ng: gpui has no table whose columns take the width of their widest cell,
// and the window has to have its size before it is made, so the texts are
// measured here the way orig measures them (DirectWrite, rounded up) in
// orig's fonts: the default UI font (9 pt), bold for the headers, 125% for
// the title
constexpr float kKbRowFontPx = 12;
constexpr float kKbTitleFontPx = 15;
// Segoe UI's line over its size
constexpr float kKbFontLineRatio = 1.33f;
// a section header is a plain text, which orig measures with GDI: a pixel less
constexpr float kKbHeaderTextDy = 15;

struct KbSectionDims {
    float keysDx = 0;
    float dx = 0;
    float dy = 0;
};

struct KbHelpDims {
    float spaceDx = 0;
    float textDy = 0;
    float capDy = 0;
    float headerDy = 0;
    float columnDx[2]{};
    float columnsDy = 0;
    KbSectionDims sections[kMaxKbSections];
};

// fontWord: gpui's font bits (gp::kFontWeightBold)
static float KbTextDx(MainWindow* win, Str s, float fontPx, int fontWord = 0) {
    gp::PaintCtx* paint = win->gpuiWin ? &win->gpuiWin->paint : nullptr;
    return ceilf(gp::MeasureText(paint, ToGpui(s), fontPx, 0, false, fontWord).w);
}

static void MeasureKbHelp(MainWindow* win, const KbSection* sections, int nSections, KbHelpDims& dims) {
    float textDy = ceilf(kKbRowFontPx * kKbFontLineRatio);
    float spaceDx = KbTextDx(win, StrL("a a"), kKbRowFontPx) - KbTextDx(win, StrL("aa"), kKbRowFontPx);
    if (spaceDx <= 0) {
        spaceDx = 4;
    }
    float capDy = textDy + 2 * kKbCapPadY;
    float rowDy = std::max(capDy, textDy);

    float columnDy[2]{};
    for (int i = 0; i < nSections; i++) {
        const KbSection& sec = sections[i];
        float keysDx = 0;
        float descDx = 0;
        int nRows = len(sec.keys);
        for (int row = 0; row < nRows; row++) {
            StrVec tokens;
            Split(&tokens, sec.keys.At(row), StrL(", "), false, 4);
            float dx = 0;
            for (Str token : tokens) {
                dx += (dx > 0 ? spaceDx : 0) + KbTextDx(win, token, kKbRowFontPx) + 2 * kKbCapPadX;
            }
            keysDx = std::max(keysDx, dx);
            descDx = std::max(descDx, KbTextDx(win, sec.descriptions.At(row), kKbRowFontPx));
        }
        float headerDx = KbTextDx(win, sec.title, kKbRowFontPx, gp::kFontWeightBold);
        float dx = std::max(keysDx + kKbKeysDescGap + descDx, headerDx);
        float dy = kKbHeaderTextDy + kKbRowGap + (float)nRows * rowDy + (float)(nRows - 1) * kKbRowGap;
        dims.sections[i] = {keysDx, dx, dy};
        int col = sec.column;
        dims.columnDx[col] = std::max(dims.columnDx[col], dx);
        columnDy[col] += (columnDy[col] > 0 ? kKbSectionGap : 0) + dy;
    }
    dims.spaceDx = spaceDx;
    dims.textDy = textDy;
    dims.capDy = capDy;
    dims.headerDy = std::max(ceilf(kKbTitleFontPx * kKbFontLineRatio), kKbHelpCloseDx);
    dims.columnsDy = std::max(columnDy[0], columnDy[1]);
}

static gp::El* KbHelpToolBuild(MainWindow* win, gp::Ctx* cx);

static ToolWindowDesc KbHelpToolDesc() {
    // orig: WS_POPUP, WS_EX_TOOLWINDOW, owned by the frame
    ToolWindowDesc desc;
    desc.name = "kbhelp";
    desc.title = KbHelpToolTitle;
    desc.frame = ToolWinFrame::None;
    desc.owner = ToolWinOwner::Owned;
    desc.style = ToolWinStyle::Tool;
    desc.build = KbHelpToolBuild;
    desc.onKey = KbHelpToolOnKey;
    desc.onClosed = KbHelpToolOnClosed;
    desc.isClientPoint = KbHelpIsClientPoint;
    return desc;
}

// orig's KeyboardHelpWnd::Create + PositionHelpWindow: the size of the
// content (with a scrollbar's width when it is taller than the work area),
// next to the frame on whichever side has more room, or at the right edge of
// the work area when the frame is fullscreen / maximized
static Rect KbHelpToolRect(MainWindow* win, const KbHelpDims& dims) {
    float dx = 2 * kKbHelpPad + dims.columnDx[0] + kKbColumnGap + dims.columnDx[1];
    float dy = 2 * kKbHelpPad + dims.headerDy + kKbHelpSepGapTop + 1 + kKbHelpSepGapBottom + dims.columnsDy;
    Size size = ToolWindowOuterSize(KbHelpToolDesc(), win, Size((int)(dx + 0.5f), (int)(dy + 0.5f)));
    Rect frame = AppShellWindowScreenRect(win);
    Rect work = AppShellWorkArea(win);
    if (size.dy > work.dy) {
        size.dx += MulDiv(16, std::max(AppShellWindowDpi(win), 96), 96);
    }
    size.dx = std::min(size.dx, work.dx);
    size.dy = std::min(size.dy, work.dy);
    bool maximized = win->isMaximized || (win->gpuiWin && win->gpuiWin->maximized);
#if OS_WIN
    maximized = maximized || IsZoomed(AppShellNativeHwnd(win));
#endif
    if (gKbHelp.parentFullscreen || maximized) {
        int x = std::max(work.x, work.Right() - size.dx);
        int y = limitValue(work.y + ((work.dy - size.dy) / 2), work.y, std::max(work.y, work.Bottom() - size.dy));
        return {x, y, size.dx, size.dy};
    }
    int rightSpace = work.Right() - frame.Right();
    int leftSpace = frame.x - work.x;
    int x = rightSpace >= leftSpace ? frame.Right() : frame.x - size.dx;
    x = limitValue(x, work.x, std::max(work.x, work.Right() - size.dx));
    int y = limitValue(frame.y, work.y, std::max(work.y, work.Bottom() - size.dy));
    return {x, y, size.dx, size.dy};
}

static void KbHelpOpenToolWindow(MainWindow* win) {
    if (gKbHelp.tw || !ToolWindowsAvailable()) {
        return;
    }
    KbSection sections[kMaxKbSections];
    int nSections = CollectKbSections(gKbHelp.dataSource, sections);
    KbHelpDims dims;
    MeasureKbHelp(win, sections, nSections, dims);
    gKbHelp.closeBounds = {};
    gKbHelp.columnsDy = dims.columnsDy;
    gKbHelp.tw = ToolWindowOpen(KbHelpToolDesc(), win, KbHelpToolRect(win, dims));
}

static gp::El* KbHelpToolBuild(MainWindow* win, gp::Ctx* cx) {
    if (!gKbHelp.visible || !gKbHelp.tw) {
        return nullptr;
    }
    if (!gKbHelpView.IsValid()) {
        gKbHelpView = gp::EntityNewState<KeyboardHelpView>(cx->app);
    }
    KeyboardHelpDataSource* ds = gKbHelp.dataSource;
    const gp::Theme& th = gp::ThemeNow(cx->app);
    KbSection sections[kMaxKbSections];
    int nSections = CollectKbSections(ds, sections);
    KbHelpDims dims;
    MeasureKbHelp(win, sections, nSections, dims);
    gKbHelp.columnsDy = dims.columnsDy;
    float rowDy = std::max(dims.capDy, dims.textDy);

    gp::El* columns[2];
    for (int col = 0; col < 2; col++) {
        columns[col] = gp::Div(cx->a)->FlexCol()->W(dims.columnDx[col])->Shrink0()->Gap(kKbSectionGap);
    }
    for (int i = 0; i < nSections; i++) {
        const KbSection& sec = sections[i];
        const KbSectionDims& sd = dims.sections[i];
        gp::El* section = gp::Div(cx->a)->FlexCol()->W(gp::kFill)->H(sd.dy)->Shrink0()->Gap(kKbRowGap);
        section->Child(gp::Div(cx->a)
                           ->FlexRow()
                           ->ItemsCenter()
                           ->H(kKbHeaderTextDy)
                           ->Shrink0()
                           ->Child(gp::TextEl(cx->a, GpuiDup(cx->a, sec.title))
                                       ->Font(kKbRowFontPx)
                                       ->Bold()
                                       ->Fg(th.foreground)
                                       ->Shrink0()));
        for (int row = 0; row < len(sec.keys); row++) {
            gp::El* rowEl = gp::Div(cx->a)->FlexRow()->ItemsCenter()->H(rowDy)->Shrink0()->Gap(kKbKeysDescGap);
            rowEl->Child(gp::Div(cx->a)->W(sd.keysDx)->Shrink0()->Child(
                KeyCaps(cx, sec.keys.At(row), kKbRowFontPx, dims.spaceDx, dims.capDy)));
            rowEl->Child(gp::TextEl(cx->a, GpuiDup(cx->a, sec.descriptions.At(row)))
                             ->Font(kKbRowFontPx)
                             ->Fg(th.foreground)
                             ->Shrink0());
            section->Child(rowEl);
        }
        columns[sec.column]->Child(section);
    }

    gp::El* content = gp::Div(cx->a)
                          ->Id(GStrL("kbhelp-content"))
                          ->FlexRow()
                          ->ItemsStart()
                          ->W(gp::kFill)
                          ->Flex1()
                          ->MinH(0)
                          ->Gap(kKbColumnGap)
                          ->ScrollY(gKbHelp.scrollY)
                          ->BoundsOut(&gKbHelp.viewBounds)
                          ->ScrollFromPath()
                          ->OnScroll(gp::ListenTo(gKbHelpView, &KeyboardHelpView::OnScroll))
                          ->Child(columns[0])
                          ->Child(columns[1]);

    gp::El* header = gp::Div(cx->a)->FlexRow()->ItemsCenter()->W(gp::kFill)->H(dims.headerDy)->Shrink0();
    header->Child(gp::TextEl(cx->a, GpuiDup(cx->a, ds->Translate(StrL("Keyboard Shortcuts"))))
                      ->Font(kKbTitleFontPx)
                      ->Bold()
                      ->Fg(th.foreground)
                      ->Flex1()
                      ->MinW(0)
                      ->Truncate());
    header->Child(gp::Div(cx->a)
                      ->FlexRow()
                      ->ItemsCenter()
                      ->JustifyCenter()
                      ->W(kKbHelpCloseDx)
                      ->H(kKbHelpCloseDx)
                      ->Shrink0()
                      ->BoundsOut(&gKbHelp.closeBounds)
                      ->Child(gpc::Button::New(cx, GStrL("kbhelp-close"))
                                  ->Icon(gp::IconName::Close)
                                  ->WithSize(gp::UiSize::XSmall)
                                  ->Compact()
                                  ->Ghost()
                                  ->OnClick(gp::ListenTo(gKbHelpView, &KeyboardHelpView::OnClose))
                                  ->IntoEl()));
    return gp::Div(cx->a)
        ->FlexCol()
        ->W(gp::kFill)
        ->Flex1()
        ->MinH(0)
        ->Pad(kKbHelpPad)
        ->Child(header)
        ->Child(gp::Div(cx->a)->W(gp::kFill)->H(kKbHelpSepGapTop)->Shrink0())
        ->Child(gp::Div(cx->a)->W(gp::kFill)->H(1)->Shrink0()->Bg(th.border))
        ->Child(gp::Div(cx->a)->W(gp::kFill)->H(kKbHelpSepGapBottom)->Shrink0())
        ->Child(content);
}

// the dialog in the frame: fixed columns
gp::El* KeyboardHelpBuild(MainWindow* win, gp::Ctx* cx) {
    if (!gKbHelp.visible || gKbHelp.tw || gKbHelp.win != win) {
        return nullptr;
    }
    if (!gKbHelpView.IsValid()) {
        gKbHelpView = gp::EntityNewState<KeyboardHelpView>(cx->app);
    }
    KeyboardHelpDataSource* ds = gKbHelp.dataSource;
    const gp::Theme& th = gp::ThemeNow(cx->app);
    constexpr float kFontPx = 13;
    constexpr float kCapFontPx = 12;
    constexpr float kCapGap = 5;
    constexpr float kKeysDx = 150;

    gp::El* columns[2] = {gp::Div(cx->a)->FlexCol()->Flex1()->MinW(0)->Gap(kKbSectionGap),
                          gp::Div(cx->a)->FlexCol()->Flex1()->MinW(0)->Gap(kKbSectionGap)};
    KbSection sections[kMaxKbSections];
    int nSections = CollectKbSections(ds, sections);
    for (int i = 0; i < nSections; i++) {
        const KbSection& sec = sections[i];
        gp::El* section = gp::Div(cx->a)->FlexCol()->W(gp::kFill)->Gap(kKbRowGap);
        section->Child(gp::TextEl(cx->a, GpuiDup(cx->a, sec.title))->Font(kFontPx)->Bold()->Fg(th.foreground));
        for (int row = 0; row < len(sec.keys); row++) {
            gp::El* rowEl = gp::Div(cx->a)->FlexRow()->W(gp::kFill)->ItemsCenter()->Gap(kKbKeysDescGap);
            rowEl->Child(
                gp::Div(cx->a)->W(kKeysDx)->Shrink0()->Child(KeyCaps(cx, sec.keys.At(row), kCapFontPx, kCapGap, 0)));
            rowEl->Child(gp::TextEl(cx->a, GpuiDup(cx->a, sec.descriptions.At(row)))
                             ->Font(kFontPx)
                             ->Fg(th.foreground)
                             ->Flex1()
                             ->MinW(0)
                             ->Truncate());
            section->Child(rowEl);
        }
        columns[sec.column]->Child(section);
    }

    gp::El* content = gp::Div(cx->a)
                          ->Id(GStrL("kbhelp-content"))
                          ->FlexRow()
                          ->W(gp::kFill)
                          ->Gap(kKbColumnGap)
                          ->ScrollY(gKbHelp.scrollY)
                          ->BoundsOut(&gKbHelp.viewBounds)
                          ->ScrollFromPath()
                          ->OnScroll(gp::ListenTo(gKbHelpView, &KeyboardHelpView::OnScroll));
    content->MaxH(560);
    content->Child(columns[0]);
    content->Child(columns[1]);

    return gpc::Dialog::New(cx)
        ->Open(true)
        ->Title(GpuiDup(cx->a, ds->Translate(StrL("Keyboard Shortcuts"))))
        ->Body(content)
        ->W(880)
        ->CloseButton(true)
        ->Footer(gp::Div(cx->a))
        ->OnClose(gp::ListenTo(gKbHelpView, &KeyboardHelpView::OnClose))
        ->OnCancel(gp::ListenTo(gKbHelpView, &KeyboardHelpView::OnClose))
        ->IntoEl(gp::WindowSize(cx->win));
}
