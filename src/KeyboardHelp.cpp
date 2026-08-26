/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "gui/Dpi.h"

#include "gui/UIModels.h"
#include "gui/PlatformFont.h"
#include "gui/Gfx.h"
#include "gui/GuiColors.h"
#include "gui/PlatformWindow.h"
#if OS_WIN
#include "base/Win.h"
#include "gui/Layout.h"
#include "gui/win/WinGui.h"
#include "gui/VirtCtrl.h"
#endif

#include "Commands.h"
#include "KeyboardHelp.h"

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

// fallback shortcuts for platforms without an accelerator table (the Windows
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
    {CmdRotateRight, "]"}, {CmdToggleFullscreen, "F"}, {CmdOpenFile, "Ctrl + O"}, {CmdSaveAs, "Ctrl + S"},
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

// next to the parent window on whichever side has more room, or docked to the
// right edge of the work area when the parent is fullscreen / maximized
static Rect PositionHelpWindow(NativeWnd parent, bool fullscreen, Size size) {
    Rect work = PlatformWindowWorkArea(parent);
    if (work.IsEmpty()) {
        work = {0, 0, std::max(size.dx, 1920), std::max(size.dy, 1080)};
    }
    // never taller or wider than the work area (issue #5999)
    size.dx = std::min(size.dx, work.dx);
    size.dy = std::min(size.dy, work.dy);
    Rect frame = PlatformWindowRect(parent);
    if (!parent || frame.IsEmpty()) {
        return {work.x + ((work.dx - size.dx) / 2), work.y + ((work.dy - size.dy) / 2), size.dx, size.dy};
    }
    if (fullscreen || PlatformWindowIsMaximized(parent)) {
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

#if OS_WIN

// The window's whole content is a layout tree: VirtText for the title, section
// headers and descriptions, VirtRichText key-caps for the shortcuts, a
// VirtCloseButton for the ✕ and a VirtLine under the title. There is no
// painting or positioning code here: WindowBase paints the tree and the
// containers (VBox / HBox / Table) place everything
struct KeyboardHelpWnd : WindowBase {
    HWND parentFrame = nullptr;
    ScrollBox* scroll = nullptr;
    VirtCloseButton* closeBtn = nullptr;

    ~KeyboardHelpWnd() override = default;
    bool Create(const KeyboardHelpArgs&);
    void OnDpiChanged(WindowBase::DpiChangedEvent* ev);
    void OnNcHitTest(WindowBase::NcHitTestEvent* ev);
};

static KeyboardHelpWnd* gKeyboardHelpWnd = nullptr;

static void ScheduleCloseKeyboardHelp() {
    if (gKeyboardHelpWnd) {
        gKeyboardHelpWnd->ScheduleDelete();
    }
}

static void OnHelpBeforeDelete(KeyboardHelpWnd* w) {
    gKeyboardHelpWnd = nullptr;
    PlatformWindowActivateIfForeground(w->parentFrame);
}

static void OnHelpClose(WindowBase::CloseEvent*) {
    ScheduleCloseKeyboardHelp();
}

// the frame owns this window, so closing the frame destroys it behind our back
static void OnHelpDestroy(WindowBase::DestroyEvent*) {
    ScheduleCloseKeyboardHelp();
}

static void OnHelpCloseClicked(VirtMouseEvent*) {
    ScheduleCloseKeyboardHelp();
}

// the window has no caption, so HTCAPTION on the client (except the close
// button) is what lets the user drag it. A caption double-click would
// otherwise maximize a popup that has no maximize box.
void KeyboardHelpWnd::OnNcHitTest(WindowBase::NcHitTestEvent* ev) {
    Point pt = HwndScreenToClient(hwnd, ev->screenPos);
    Rect client = HwndClientRect(hwnd);
    if (!client.Contains(pt)) {
        return;
    }
    if (closeBtn && closeBtn->lastBounds.Contains(pt)) {
        ev->result = HTCLIENT;
        ev->didHandle = true;
        return;
    }
    ev->result = HTCAPTION;
    ev->didHandle = true;
}

// '?' toggles the help, so it also closes it while it has the focus
static void OnHelpWndProc(WindowBase::WndProcEvent* ev) {
    if (ev->msg == WM_NCLBUTTONDBLCLK) {
        ev->result = 0;
        ev->didHandle = true;
        return;
    }
    if (ev->msg == WM_CHAR && ev->wparam == '?') {
        ev->result = 0;
        ev->didHandle = true;
        ScheduleCloseKeyboardHelp();
        return;
    }
    auto* help = (KeyboardHelpWnd*)ev->w;
    ScrollBox* scroll = help ? help->scroll : nullptr;
    if (!scroll) {
        return;
    }
    if (ev->msg == WM_VSCROLL && ev->lparam == 0) {
        scroll->OnVScroll(ev->wparam);
        ev->result = 0;
        ev->didHandle = true;
        return;
    }
    if (ev->msg == WM_MOUSEWHEEL) {
        VirtMouseEvent mev;
        mev.wheelDelta = GET_WHEEL_DELTA_WPARAM(ev->wparam);
        scroll->OnMouseWheel(&mev);
        ev->result = 0;
        ev->didHandle = true;
    }
}

static void OnHelpKeyDown(KeyEvent* ev) {
    ScrollBox* scroll = gKeyboardHelpWnd ? gKeyboardHelpWnd->scroll : nullptr;
    if (!scroll) {
        return;
    }
    switch (ev->vkey) {
        case VK_UP:
            scroll->ScrollBy(-scroll->lineDy);
            ev->didHandle = true;
            break;
        case VK_DOWN:
            scroll->ScrollBy(scroll->lineDy);
            ev->didHandle = true;
            break;
        case VK_PRIOR:
            scroll->ScrollPage(-1);
            ev->didHandle = true;
            break;
        case VK_NEXT:
            scroll->ScrollPage(1);
            ev->didHandle = true;
            break;
        case VK_HOME:
            scroll->ScrollTo(0);
            ev->didHandle = true;
            break;
        case VK_END:
            scroll->ScrollTo(scroll->MaxScrollY());
            ev->didHandle = true;
            break;
    }
}

static void ApplyKeyboardHelpCloseDpi(VirtCloseButton* closeBtn, int dpi) {
    if (!closeBtn || dpi <= 0) {
        return;
    }
    int btnDx = DpiScaleByDpi(dpi, 16);
    int btnPad = DpiScaleByDpi(dpi, 4);
    closeBtn->padding = Insets{btnPad, btnPad, btnPad, btnPad};
    closeBtn->idealSize = {btnDx + (2 * btnPad), btnDx + (2 * btnPad)};
}

static ILayout* BuildKeyboardHelpLayout(KeyboardHelpDataSource* ds, Str title, ScrollBox** scrollOut,
                                        VirtCloseButton** closeOut) {
    PlatformFont* fontRow = GetDefaultGuiFont();
    PlatformFont* fontHeader = GetBoldPlatformFont(fontRow);
    PlatformFont* fontTitle = GetScaledPlatformFont(fontHeader, 125);
    if (!fontRow || !fontHeader || !fontTitle) {
        return nullptr;
    }

    int columnGap = DpiScale(16);
    int keysDescriptionGap = DpiScale(12);
    int rowGap = DpiScale(8);
    int sectionGap = DpiScale(14);

    VBox* columns[2] = {new VBox(), new VBox()};
    for (VBox* column : columns) {
        column->gap = sectionGap;
    }

    for (const KbSectionDef& definition : kSections) {
        StrVec keys;
        StrVec descriptions;
        for (const int* cmdId = definition.commands; *cmdId; cmdId++) {
            TempStr k = ds->CommandShortcutTemp(*cmdId, 2);
            TempStr d = ds->CommandDescriptionTemp(*cmdId);
            // skip commands with no keyboard shortcut (e.g. un-bound by the user)
            if (len(k) == 0 || len(d) == 0) {
                continue;
            }
            keys.Append(k);
            descriptions.Append(d);
        }
        int nRows = len(keys);
        if (nRows == 0) {
            continue;
        }
        auto* section = new VBox();
        section->gap = rowGap;
        section->AddChild(new VirtText(ds->Translate(Str(definition.title)), fontHeader));

        auto* table = new Table();
        table->SetSize(nRows, 2);
        table->colGap = keysDescriptionGap;
        table->rowGap = rowGap;
        for (int i = 0; i < nRows; i++) {
            auto* caps = new VirtRichText();
            caps->font = fontRow;
            ParseTipInto(caps, fmt("(Kbd/%s)", keys.At(i)));
            TableCell& keysCell = table->SetCell(i, 0, caps);
            keysCell.alignH = CrossAxisAlign::CrossEnd;
            keysCell.alignV = CrossAxisAlign::CrossCenter;
            TableCell& descCell = table->SetCell(i, 1, new VirtText(descriptions.At(i), fontRow));
            descCell.alignV = CrossAxisAlign::CrossCenter;
        }
        section->AddChild(table);
        columns[definition.column]->AddChild(section);
    }

    auto* header = new HBox();
    header->alignCross = CrossAxisAlign::CrossCenter;
    header->AddChild(new VirtText(title, fontTitle), 1);
    auto* closeBtn = new VirtCloseButton();
    ApplyKeyboardHelpCloseDpi(closeBtn, DpiGet());
    closeBtn->onClick = MkFunc1Void<VirtMouseEvent*>(OnHelpCloseClicked);
    header->AddChild(closeBtn);
    if (closeOut) {
        *closeOut = closeBtn;
    }

    auto* content = new HBox();
    content->gap = columnGap;
    content->AddChild(columns[0]);
    content->AddChild(columns[1]);
    auto* scroll = new ScrollBox(content);
    scroll->lineDy = DpiScale(24);
    if (scrollOut) {
        *scrollOut = scroll;
    }

    auto* separator = new VirtLine();
    separator->thickness = DpiScale(1);

    auto* root = new VBox();
    root->alignCross = CrossAxisAlign::Stretch;
    root->AddChild(header);
    root->AddChild(new Spacer(0, DpiScale(6)));
    root->AddChild(separator);
    root->AddChild(new Spacer(0, DpiScale(10)));
    // flex so the columns shrink to the window and ScrollBox scrolls them
    root->AddChild(scroll, 1);

    int pad = DpiScale(20);
    return new Padding(root, Insets{pad, pad, pad, pad});
}

bool KeyboardHelpWnd::Create(const KeyboardHelpArgs& helpArgs) {
    parentFrame = (HWND)helpArgs.parent;
    KeyboardHelpDataSource* ds = helpArgs.dataSource ? helpArgs.dataSource : GetDefaultKeyboardHelpDataSource();
    // scale to the monitor the parent (and so the help) is on
    DpiScope dpiScope(parentFrame);
    Str title = ds->Translate(StrL("Keyboard Shortcuts"));

    layout = BuildKeyboardHelpLayout(ds, title, &scroll, &closeBtn);
    if (!layout) {
        return false;
    }
    Size size = layout->Layout(ExpandInf());
    Rect work = PlatformWindowWorkArea(parentFrame);
    DWORD style = WS_POPUP;
    if (!work.IsEmpty() && size.dy > work.dy) {
        size.dy = work.dy;
        size.dx += DpiGetSystemMetrics(SM_CXVSCROLL);
        style |= WS_VSCROLL;
    }

    CreateCustomArgs args;
    args.title = title;
    args.style = style;
    args.exStyle = WS_EX_TOOLWINDOW;
    args.visible = false;
    onDpiChanged = MkMethod1<KeyboardHelpWnd, WindowBase::DpiChangedEvent*, &KeyboardHelpWnd::OnDpiChanged>(this);
    CreateCustom(args);
    if (!hwnd) {
        return false;
    }
    // owned by the frame (not created as its child: CreateCustomHwnd would add
    // WS_CHILD) so the help stays above it and is destroyed with it
    if (parentFrame) {
        SetWindowLongPtrW(hwnd, GWLP_HWNDPARENT, (LONG_PTR)parentFrame);
    }
    int dpi = DpiGetForHwnd(hwnd);
    if (dpi <= 0) {
        dpi = DpiGet();
    }
    ApplyKeyboardHelpCloseDpi(closeBtn, dpi);

    Rect wr = PositionHelpWindow(parentFrame, helpArgs.parentFullscreen, size);
    SetWindowPos(hwnd, nullptr, wr.x, wr.y, wr.dx, wr.dy, SWP_NOZORDER | SWP_NOACTIVATE);
    DoLayout();
    UpdateTheme();
    SetIsVisible(true);
    HwndSetFocus(hwnd);
    return true;
}

void KeyboardHelpWnd::OnDpiChanged(WindowBase::DpiChangedEvent* ev) {
    int dpi = (int)ev->dpiX;
    ApplyKeyboardHelpCloseDpi(closeBtn, dpi);
    DoLayout();
    ev->didHandle = true;
}

void ToggleKeyboardHelp(const KeyboardHelpArgs& args) {
    if (gKeyboardHelpWnd) {
        ScheduleCloseKeyboardHelp();
        return;
    }
    auto* w = new KeyboardHelpWnd();
    w->closeOnEsc = true;
    w->onBeforeDelete = MkFunc0(OnHelpBeforeDelete, w);
    w->onClose = MkFunc1Void<WindowBase::CloseEvent*>(OnHelpClose);
    w->onDestroy = MkFunc1Void<WindowBase::DestroyEvent*>(OnHelpDestroy);
    w->onWndProc = MkFunc1Void<WindowBase::WndProcEvent*>(OnHelpWndProc);
    w->onNcHitTest = MkMethod1<KeyboardHelpWnd, WindowBase::NcHitTestEvent*, &KeyboardHelpWnd::OnNcHitTest>(w);
    w->onKeyDown = MkFunc1Void<KeyEvent*>(OnHelpKeyDown);
    if (!w->Create(args)) {
        delete w;
        return;
    }
    gKeyboardHelpWnd = w;
}

void CloseKeyboardHelp() {
    ScheduleCloseKeyboardHelp();
}

bool IsKeyboardHelpVisible() {
    return gKeyboardHelpWnd != nullptr;
}

#else // !OS_WIN: mac / gtk4 draw into a PlatformWindow (VirtCtrl is win-only)

struct KbRow {
    Str keys;
    Str description;
    StrVec tokens;
    Rect keysRect;
    Rect descriptionRect;
};

struct KbSection {
    Str title;
    Vec<KbRow*> rows;
    Rect titleRect;
    int column = 0;
};

struct KeyboardHelpWindow {
    PlatformWindow* window = nullptr;
    NativeWnd parent = nullptr;
    KeyboardHelpDataSource* dataSource = nullptr;
    bool parentFullscreen = false;
    bool closeHovered = false;
    bool closePressed = false;

    PlatformFont* fontTitle = nullptr;
    PlatformFont* fontHeader = nullptr;
    PlatformFont* fontRow = nullptr;
    Str title;
    Vec<KbSection*> sections;

    Rect titleRect;
    Rect closeRect;
    Rect separatorRect;
    int contentTop = 0;
    int capPadX = 0;
    int capGap = 0;
    int capRadius = 0;

    ~KeyboardHelpWindow();
    bool Create(const KeyboardHelpArgs&);
    void CollectContent();
    Size LayoutContent();
    void Paint(PlatformWindowPaintEvent*);
    void OnPointer(PlatformPointerEvent*);
    void OnKey(PlatformKeyEvent*);
};

static KeyboardHelpWindow* gKeyboardHelpWindow = nullptr;

static void FreeSection(KbSection* section) {
    if (!section) {
        return;
    }
    str::Free(section->title);
    for (KbRow* row : section->rows) {
        str::Free(row->keys);
        str::Free(row->description);
        delete row;
    }
    delete section;
}

KeyboardHelpWindow::~KeyboardHelpWindow() {
    delete window;
    window = nullptr;
    str::Free(title);
    for (KbSection* section : sections) {
        FreeSection(section);
    }
}

static void SafeDeleteKeyboardHelpWindow() {
    if (!gKeyboardHelpWindow) {
        return;
    }
    KeyboardHelpWindow* help = gKeyboardHelpWindow;
    NativeWnd parent = help->parent;
    gKeyboardHelpWindow = nullptr;
    delete help;
    PlatformWindowActivateIfForeground(parent);
}

void CloseKeyboardHelp() {
    SafeDeleteKeyboardHelpWindow();
}

static void ScheduleCloseKeyboardHelp() {
    if (gKeyboardHelpWindow) {
        PlatformPostTask(MkFunc0Void(SafeDeleteKeyboardHelpWindow));
    }
}

void KeyboardHelpWindow::CollectContent() {
    title = str::Dup(dataSource->Translate(StrL("Keyboard Shortcuts")));

    for (const KbSectionDef& definition : kSections) {
        auto* section = new KbSection();
        section->title = str::Dup(dataSource->Translate(Str(definition.title)));
        section->column = definition.column;
        for (const int* cmdId = definition.commands; *cmdId; cmdId++) {
            TempStr keys = dataSource->CommandShortcutTemp(*cmdId, 2);
            TempStr description = dataSource->CommandDescriptionTemp(*cmdId);
            // skip commands with no keyboard shortcut (e.g. un-bound by the user)
            if (len(keys) == 0 || len(description) == 0) {
                continue;
            }
            auto* row = new KbRow();
            row->keys = str::Dup(keys);
            row->description = str::Dup(description);
            Split(&row->tokens, row->keys, StrL(", "), false, 4);
            section->rows.Append(row);
        }
        if (len(section->rows) == 0) {
            FreeSection(section);
            continue;
        }
        sections.Append(section);
    }
}

Size KeyboardHelpWindow::LayoutContent() {
    int pad = DpiScale(20);
    int columnGap = DpiScale(16);
    int keysDescriptionGap = DpiScale(12);
    int rowGap = DpiScale(8);
    int sectionGap = DpiScale(14);
    int headerHeight = PlatformFontLineHeight(fontHeader) + DpiScale(8);
    int rowHeight = PlatformFontLineHeight(fontRow);
    capPadX = DpiScale(7);
    capGap = DpiScale(5);
    capRadius = DpiScale(5);

    int keyWidths[2] = {0, 0};
    int descriptionWidths[2] = {0, 0};
    int headerWidths[2] = {0, 0};
    for (KbSection* section : sections) {
        int column = section->column;
        headerWidths[column] = std::max(headerWidths[column], PlatformFontMeasureText(fontHeader, section->title).dx);
        for (KbRow* row : section->rows) {
            int keysDx = 0;
            for (int i = 0; i < row->tokens.size; i++) {
                keysDx += PlatformFontMeasureText(fontRow, row->tokens.At(i)).dx + (2 * capPadX);
            }
            keysDx += std::max(row->tokens.size - 1, 0) * capGap;
            keyWidths[column] = std::max(keyWidths[column], keysDx);
            descriptionWidths[column] =
                std::max(descriptionWidths[column], PlatformFontMeasureText(fontRow, row->description).dx);
        }
    }

    int columnWidths[2];
    for (int column = 0; column < 2; column++) {
        columnWidths[column] =
            std::max(headerWidths[column], keyWidths[column] + keysDescriptionGap + descriptionWidths[column]);
    }
    int width = (2 * pad) + columnWidths[0];
    if (columnWidths[1] > 0) {
        width += columnGap + columnWidths[1];
    }

    int titleHeight = std::max(PlatformFontLineHeight(fontTitle), DpiScale(24));
    titleRect = {pad, pad, width - (2 * pad) - titleHeight - DpiScale(8), titleHeight};
    closeRect = {width - pad - titleHeight, pad, titleHeight, titleHeight};
    separatorRect = {pad, pad + titleHeight + DpiScale(6), width - (2 * pad), DpiScale(1)};
    contentTop = separatorRect.Bottom() + DpiScale(10);

    int columnX[2] = {pad, pad + columnWidths[0] + columnGap};
    int columnY[2] = {contentTop, contentTop};
    bool firstSection[2] = {true, true};
    for (KbSection* section : sections) {
        int column = section->column;
        int& y = columnY[column];
        if (!firstSection[column]) {
            y += sectionGap;
        }
        firstSection[column] = false;
        section->titleRect = {columnX[column], y, columnWidths[column], headerHeight};
        y += headerHeight;
        for (KbRow* row : section->rows) {
            y += rowGap;
            row->keysRect = {columnX[column], y, keyWidths[column], rowHeight};
            row->descriptionRect = {columnX[column] + keyWidths[column] + keysDescriptionGap, y,
                                    descriptionWidths[column], rowHeight};
            y += rowHeight;
        }
    }

    int height = std::max(columnY[0], columnY[1]) + pad;
    return {width, height};
}

static void PaintCloseButton(KeyboardHelpWindow* help, Gfx* gfx) {
    Rect r = help->closeRect;
    Color glyph = gColsCloseBtn[help->closeHovered ? kColCloseXHover : kColCloseX];
    if (help->closeHovered) {
        gfx->FillEllipse(r, gColsCloseBtn[kColCloseCircleHover]);
    }
    int inset = std::max(r.dx / 3, 3);
    Point p1{r.x + inset, r.y + inset};
    Point p2{r.Right() - inset, r.Bottom() - inset};
    Point p3{r.Right() - inset, r.y + inset};
    Point p4{r.x + inset, r.Bottom() - inset};
    gfx->DrawLineAA(p1, p2, glyph, 1.5f);
    gfx->DrawLineAA(p3, p4, glyph, 1.5f);
}

void KeyboardHelpWindow::Paint(PlatformWindowPaintEvent* ev) {
    Gfx* gfx = ev->gfx;
    Color background = gColsFill[kColFillBg];
    Color text = gColsText[kColText];
    Color border = gColsLine[kColLineFg];
    gfx->FillRect(ev->clientRect, background);
    gfx->DrawText(title, titleRect, gfxTextVCenter | gfxTextSingleLine, fontTitle, text);
    gfx->FillRect(separatorRect, border);
    PaintCloseButton(this, gfx);

    Color capBackground = AccentColor(background, 16);
    for (KbSection* section : sections) {
        gfx->DrawText(section->title, section->titleRect, gfxTextVCenter | gfxTextSingleLine, fontHeader, text);
        for (KbRow* row : section->rows) {
            int capsDx = std::max(row->tokens.size - 1, 0) * capGap;
            for (int i = 0; i < row->tokens.size; i++) {
                capsDx += PlatformFontMeasureText(fontRow, row->tokens.At(i)).dx + (2 * capPadX);
            }
            int x = row->keysRect.Right() - capsDx;
            for (int i = 0; i < row->tokens.size; i++) {
                Str token = row->tokens.At(i);
                int dx = PlatformFontMeasureText(fontRow, token).dx + (2 * capPadX);
                Rect cap{x, row->keysRect.y, dx, row->keysRect.dy};
                gfx->FillRoundedRect(cap, capRadius, capBackground, border);
                gfx->DrawText(token, cap, gfxTextCenter | gfxTextVCenter | gfxTextEllipsis, fontRow, text);
                x += dx + capGap;
            }
            gfx->DrawText(row->description, row->descriptionRect, gfxTextVCenter | gfxTextSingleLine, fontRow, text);
        }
    }
}

void KeyboardHelpWindow::OnPointer(PlatformPointerEvent* ev) {
    if (ev->type == PlatformPointerEventType::Leave) {
        if (closeHovered) {
            closeHovered = false;
            window->Invalidate();
        }
        window->SetCursor(CursorId::Arrow);
        return;
    }
    bool overClose = closeRect.Contains(ev->pos);
    if (overClose != closeHovered) {
        closeHovered = overClose;
        window->Invalidate();
    }
    window->SetCursor(overClose ? CursorId::Hand : ev->pos.y < contentTop ? CursorId::Move : CursorId::Arrow);
    if (ev->button != 1) {
        return;
    }
    if (ev->type == PlatformPointerEventType::Down) {
        if (overClose) {
            closePressed = true;
            ev->didHandle = true;
        } else if (ev->pos.y < contentTop) {
            window->BeginMove(*ev);
            ev->didHandle = true;
        }
    } else if (ev->type == PlatformPointerEventType::Up && closePressed) {
        closePressed = false;
        ev->didHandle = true;
        if (overClose) {
            ScheduleCloseKeyboardHelp();
        }
    }
}

void KeyboardHelpWindow::OnKey(PlatformKeyEvent* ev) {
    if (ev->codepoint == '?') {
        ev->didHandle = true;
        ScheduleCloseKeyboardHelp();
    }
}

bool KeyboardHelpWindow::Create(const KeyboardHelpArgs& args) {
    parent = (NativeWnd)args.parent;
    parentFullscreen = args.parentFullscreen;
    dataSource = args.dataSource ? args.dataSource : GetDefaultKeyboardHelpDataSource();
    GuiColorsInitIfNeeded();
    fontRow = GetDefaultGuiFont();
    fontHeader = GetBoldPlatformFont(fontRow);
    fontTitle = GetScaledPlatformFont(fontHeader, 125);
    if (!fontRow || !fontHeader || !fontTitle) {
        return false;
    }
    CollectContent();
    Size size = LayoutContent();

    PlatformWindow::CreateArgs createArgs;
    createArgs.parent = parent;
    createArgs.title = title;
    createArgs.initialSize = size;
    createArgs.visible = false;
    createArgs.frameless = true;
    createArgs.userData = this;
    window = PlatformWindow::Create(createArgs);
    if (!window) {
        return false;
    }
    window->onPaint = MkMethod1<KeyboardHelpWindow, PlatformWindowPaintEvent*, &KeyboardHelpWindow::Paint>(this);
    window->onPointer = MkMethod1<KeyboardHelpWindow, PlatformPointerEvent*, &KeyboardHelpWindow::OnPointer>(this);
    window->onKey = MkMethod1<KeyboardHelpWindow, PlatformKeyEvent*, &KeyboardHelpWindow::OnKey>(this);
    window->onCloseRequest = MkFunc0Void(ScheduleCloseKeyboardHelp);
    window->SetBounds(PositionHelpWindow(parent, parentFullscreen, size));
    window->Show(true);
    window->Focus();
    return true;
}

void ToggleKeyboardHelp(const KeyboardHelpArgs& args) {
    if (gKeyboardHelpWindow) {
        ScheduleCloseKeyboardHelp();
        return;
    }
    auto* help = new KeyboardHelpWindow();
    gKeyboardHelpWindow = help;
    if (!help->Create(args)) {
        gKeyboardHelpWindow = nullptr;
        delete help;
        return;
    }
}

bool IsKeyboardHelpVisible() {
    return gKeyboardHelpWindow != nullptr;
}

#endif
