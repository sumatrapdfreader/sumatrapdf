/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "gui/Dpi.h"

#include "gui/UIModels.h"
#include "gui/PlatformFont.h"
#include "gui/Gfx.h"
#include "gui/GuiColors.h"
#include "gui/PlatformWindow.h"
#include "base/Win.h"
#include "gui/Layout.h"
#include "gui/win/WinGui.h"
#include "gui/VirtCtrl.h"
#include "Settings.h"
#include "AppSettings.h"
#include "DocController.h"
#include "MainWindow.h"
#include "Accelerators.h"
#include "Translations.h"

#include "Commands.h"
#include "KeyboardHelp.h"
#include "KeyboardHelpCommon.h"

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

    // Shortcuts from advanced settings that are not already a rebinding of a
    // command listed above (named entries, commands with args, unlisted cmds)
    if (gSettings && gSettings->shortcuts) {
        StrVec keys;
        StrVec descriptions;
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
            keys.Append(k);
            descriptions.Append(d);
        }
        int nRows = len(keys);
        if (nRows > 0) {
            auto* section = new VBox();
            section->gap = rowGap;
            section->AddChild(new VirtText(ds->Translate(StrL("Custom")), fontHeader));
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
            columns[0]->AddChild(section);
        }
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

static SumatraKeyboardHelpDataSource gSumatraKeyboardHelpDataSource;

void ToggleKeyboardHelp(MainWindow* win) {
    if (!win) {
        return;
    }
    KeyboardHelpArgs args;
    args.parent = win->hwndFrame;
    args.parentFullscreen = win->IsFullScreenOrPresentation();
    args.dataSource = &gSumatraKeyboardHelpDataSource;
    ToggleKeyboardHelp(args);
}
