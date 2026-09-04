/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/ScopedWin.h"
#include "base/UITask.h"
#include "base/Win.h"
#include "gui/Dpi.h"

#include "gui/UIModels.h"

#include "gui/Layout.h"
#include "gui/PlatformFont.h"
#include "gui/Gfx.h"
#include "gui/GuiColors.h"
#include "gui/VirtCtrl.h"
#include "gui/win/WinGui.h"

// HwndBase is the win32 plumbing WindowBase and ControlBase share: one window
// procedure, one subclassing scheme and one HWND -> object list. The two stay
// separate types on top of it: a control mustn't carry a top-level window's
// close / taskbar / drop-files machinery, and a window isn't positioned by a
// parent's layout, so only ControlBase is an ILayout.

static Kind kindWindow = "wnd";
static Kind kindControl = "control";

static const WStr kDefaultClassName = L"SumatraWgDefaultWinClass";
// its own class name, so a WindowBase custom window and a ControlBase custom
// window can be told apart in a debugger / spy tool
static const WStr kControlClassName = L"SumatraWgControlClass";

TempStr WinMsgNameTemp(UINT msg) {
    return fmt("0x%x", (int)msg);
}

//--- the single HWND -> object list, shared by windows and controls

struct HwndToWnd {
    HWND hwnd = nullptr;
    HwndBase* wnd = nullptr;
};

static Vec<HwndToWnd> gHwndToWnd;

HwndBase* HwndBaseFromHwnd(HWND hwnd) {
    for (auto& e : gHwndToWnd) {
        if (e.hwnd == hwnd) {
            return e.wnd;
        }
    }
    return nullptr;
}

WindowBase* WindowBaseFromHwnd(HWND hwnd) {
    HwndBase* w = HwndBaseFromHwnd(hwnd);
    return w ? w->AsWindowBase() : nullptr;
}

ControlBase* ControlFromHwnd(HWND hwnd) {
    HwndBase* w = HwndBaseFromHwnd(hwnd);
    return w ? w->AsControlBase() : nullptr;
}

static bool HwndListRemove(HwndBase* w) {
    bool removed = false;
    for (int i = 0; i < len(gHwndToWnd);) {
        if (gHwndToWnd[i].wnd == w) {
            VecRemoveAtFast(gHwndToWnd, i);
            removed = true;
        } else {
            i++;
        }
    }
    return removed;
}

static void HwndListAdd(HwndBase* w) {
    bool report = HwndListRemove(w);
    ReportIfFast(report);
    HwndToWnd e{w->hwnd, w};
    VecAppend(gHwndToWnd, e);
}

//- Taskbar.cpp

constexpr DWORD kWmTaskbarCallback = WM_APP + 0x15;
const DWORD kWmTaskbarCreated = ::RegisterWindowMessage(L"TaskbarCreated");
const DWORD kWmTaskbarButtonCreated = ::RegisterWindowMessage(L"TaskbarButtonCreated");

//--- HwndBase

static LRESULT CALLBACK WndBaseWindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    // seen crashes in TabCtrl::WndProc() which might be caused by handling drag&drop messages
    // after parent window was destroyed. maybe this will fix it
    if (!IsWindow(hwnd)) {
        return 0;
    }

    HwndBase* w = HwndBaseFromHwnd(hwnd);

    if (msg == WM_NCCREATE) {
        CREATESTRUCT* cs = (CREATESTRUCT*)lparam;
        ReportIf(w);
        // CreateCustomHwnd / CreateControl pass the HwndBase subobject
        w = (HwndBase*)(cs->lpCreateParams);
        w->hwnd = hwnd;
        HwndListAdd(w);
    }

    if (w) {
        return w->OnMessage(hwnd, msg, wparam, lparam);
    }
    return ::DefWindowProc(hwnd, msg, wparam, lparam);
}

static LRESULT CALLBACK WndBaseSubclassedWindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR /*subclassId*/,
                                                    DWORD_PTR /*data*/) {
    return WndBaseWindowProc(hwnd, msg, wp, lp);
}

static void RegisterWndClass(WStr className) {
    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(wc);
    wc.hInstance = GetInstance();
    wc.style = CS_DBLCLKS;
    wc.lpszClassName = className.s;
    wc.lpfnWndProc = WndBaseWindowProc;
    wc.hCursor = GetCachedCursor(IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(::GetStockObject(WHITE_BRUSH));
    ::RegisterClassExW(&wc);
}

WindowBase* HwndBase::AsWindowBase() {
    return nullptr;
}

ControlBase* HwndBase::AsControlBase() {
    return nullptr;
}

HwndBase::~HwndBase() {
    Destroy();
    GfxDestroyDoubleBuffer(this);
    // the tree first: a virtual control tells its root it's going away
    delete layout;
    delete vroot;
    DeleteBrushSafe(&bgBrush);
}

void HwndBase::Destroy() {
    // the order is important
    // stop dispatching messages to this object
    HwndListRemove(this);
    // unsubclass while hwnd is still valid
    UnSubclass();
    // finally destroy hwnd
    HwndDestroyWindowSafe(&hwnd);
}

void HwndBase::Subclass() {
    ReportIf(!IsWindow(hwnd));
    ReportIf(subclassId); // don't subclass multiple times
    if (subclassId) {
        return;
    }
    HwndListAdd(this);

    subclassId = NextSubclassId();
    BOOL ok = SetWindowSubclass(hwnd, WndBaseSubclassedWindowProc, subclassId, (DWORD_PTR)this);
    if (!ok) {
        // can fail under low memory / desktop heap exhaustion (it allocates and
        // attaches a window property), so don't assert. Reset subclassId so that
        // `subclassId != 0` keeps meaning "is subclassed".
        logf("HwndBase::Subclass: SetWindowSubclass() failed, err: %d\n", (int)GetLastError());
        subclassId = 0;
    }
}

void HwndBase::UnSubclass() {
    if (!subclassId) {
        return;
    }
    RemoveWindowSubclass(hwnd, WndBaseSubclassedWindowProc, subclassId);
    subclassId = 0;
}

HWND HwndBase::Detach() {
    UnSubclass();

    HWND wnd = hwnd;
    HwndListRemove(this);
    hwnd = nullptr;
    return wnd;
}

void HwndBase::SetText(Str s) {
    if (len(s) == 0) {
        s = StrL("");
    }
    HwndSetText(hwnd, s);
    HwndRepaintNow(hwnd); // TODO: move inside HwndSetText()?
}

TempStr HwndBase::GetTextTemp() {
    return HwndGetTextTemp(hwnd);
}

void HwndBase::SetPos(Rect* r) {
    HwndMoveWindow(hwnd, r);
}

void HwndBase::SetColors(Color textCol, Color bgCol) {
    if (textCol != kColorNoChange) {
        this->textColor = textCol;
    }
    if (bgCol == kColorNoChange) {
        return;
    }
    this->bgColor = bgCol;
    DeleteBrushSafe(&bgBrush); // will be re-created in BackgroundBrush()
    HwndScheduleRepaint(hwnd);
}

HBRUSH HwndBase::BackgroundBrush() {
    return BackgroundBrush(bgColor);
}

// the brush for `col`, cached in bgBrush; rebuilt when the color changed (a
// theme change moves a WindowBase's resolved color without SetColors())
HBRUSH HwndBase::BackgroundBrush(Color col) {
    if (col == kColorUnset) {
        return nullptr;
    }
    if (bgBrush && bgBrushColor == col) {
        return bgBrush;
    }
    DeleteBrushSafe(&bgBrush);
    bgBrush = CreateSolidBrush(col);
    bgBrushColor = col;
    return bgBrush;
}

PlatformFont* HwndBase::GetFont() {
    return font;
}

HFONT HwndBase::GetHFont() const {
    return font ? font->GetHFont() : nullptr;
}

// HwndSetFont() sends WM_SETFONT, which our wndproc records in `font` and (for
// subclassed controls) forwards to the control itself, so this both remembers
// and applies the font. Without it SetFont() was a no-op on screen.
void HwndBase::SetFont(PlatformFont* fontIn) {
    font = fontIn;
    if (!hwnd) {
        return;
    }
    HwndSetFont(hwnd, GetHFont());
}

void HwndBase::SetIsEnabled(bool isEnabled) const {
    ReportIf(!hwnd);
    BOOL enabled = isEnabled ? TRUE : FALSE;
    ::EnableWindow(hwnd, enabled);
}

bool HwndBase::IsEnabled() const {
    BOOL enabled = ::IsWindowEnabled(hwnd);
    return tobool(enabled);
}

void HwndBase::SuspendRedraw() const {
    SendMessageW(hwnd, WM_SETREDRAW, FALSE, 0);
}

void HwndBase::ResumeRedraw() const {
    SendMessageW(hwnd, WM_SETREDRAW, TRUE, 0);
}

void HwndBase::DoLayout(Size size) {
    LayoutTreeToSize(hwnd, layout, size, &vroot);
}

void HwndBase::DoLayout() {
    Rect rc = HwndClientRect(hwnd);
    DoLayout(rc.Size());
}

// A function used internally to call OnMessageReflect. Don't call or override this function.
LRESULT HwndBase::MessageReflect(UINT msg, WPARAM wparam, LPARAM lparam) {
    HWND wnd = nullptr;
    switch (msg) {
        case WM_COMMAND:
        case WM_CTLCOLORBTN:
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORDLG:
        case WM_CTLCOLORLISTBOX:
        case WM_CTLCOLORSCROLLBAR:
        case WM_CTLCOLORSTATIC:
        case WM_CHARTOITEM:
        case WM_VKEYTOITEM:
        case WM_HSCROLL:
        case WM_VSCROLL:
            wnd = reinterpret_cast<HWND>(lparam);
            break;

        case WM_DRAWITEM: {
            // Get HWND directly from the struct since control ID may be 0
            DRAWITEMSTRUCT* dis = (DRAWITEMSTRUCT*)lparam;
            wnd = dis->hwndItem;
            break;
        }
        case WM_MEASUREITEM: {
            // MEASUREITEMSTRUCT doesn't have hwnd, try GetDlgItem first
            wnd = GetDlgItem(hwnd, static_cast<int>(wparam));
            if (!wnd && wparam == 0) {
                // Control ID is 0, find owner-draw listbox child
                wnd = FindWindowExW(hwnd, nullptr, L"LISTBOX", nullptr);
            }
            break;
        }
        case WM_DELETEITEM:
        case WM_COMPAREITEM:
            wnd = GetDlgItem(hwnd, static_cast<int>(wparam));
            break;

        case WM_PARENTNOTIFY:
            switch (LOWORD(wparam)) {
                case WM_CREATE:
                case WM_DESTROY:
                    wnd = reinterpret_cast<HWND>(lparam);
                    break;
            }
    }
    if (!wnd) {
        return 0;
    }

    ControlBase* pWnd = ControlFromHwnd(wnd);
    if (!pWnd) {
        // ComboBox's inner EDIT is not a ControlBase; color via the combo
        pWnd = ControlFromHwnd(GetParent(wnd));
    }
    if (pWnd != nullptr) {
        return pWnd->DispatchMessageReflect(msg, wparam, lparam);
    }

    return 0;
}

LRESULT HwndBase::FinalWindowProc(UINT msg, WPARAM wparam, LPARAM lparam) {
    if (subclassId) {
        return ::DefSubclassProc(hwnd, msg, wparam, lparam);
    }
    // TODO: also DefSubclassProc?
    return ::DefWindowProc(hwnd, msg, wparam, lparam);
}

HWND HwndBase::CreateCustomHwnd(const CreateCustomArgs& args, WStr defaultClassName) {
    font = args.font;

    WStr className = args.className ? args.className : defaultClassName;
    // TODO: validate className is not win32 control class
    RegisterWndClass(className);
    ReportIf(args.parent && args.owner);
    HWND parentOrOwner = args.parent ? args.parent : args.owner;

    DWORD style = args.style;
    if (style == 0) {
        style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
    }
    if (args.parent) {
        style |= WS_CHILD;
    } else {
        style &= ~WS_CHILD;
        style |= WS_CLIPCHILDREN;
    }
    if (args.visible) {
        style |= WS_VISIBLE;
    } else {
        style &= ~WS_VISIBLE;
    }

    int x = args.pos.x;
    int y = args.pos.y;
    int dx = args.pos.dx;
    int dy = args.pos.dy;
    if (!args.parent && args.pos.IsEmpty()) {
        x = CW_USEDEFAULT;
        y = CW_USEDEFAULT;
        dx = CW_USEDEFAULT;
        dy = CW_USEDEFAULT;
    }

    DWORD exStyle = args.exStyle;
    if (args.isRtl) {
        exStyle |= WS_EX_LAYOUTRTL | WS_EX_NOINHERITLAYOUT;
    }
    ReportIf(args.menu && args.cmdId);
    HMENU m = args.menu;
    if (m == nullptr) {
        m = (HMENU)(INT_PTR)args.cmdId;
    }
    HINSTANCE inst = GetInstance();
    void* createParams = this;
    WCHAR* titleW = CWStrTemp(args.title);

    HWND hwndTmp =
        ::CreateWindowExW(exStyle, className.s, titleW, style, x, y, dx, dy, parentOrOwner, m, inst, createParams);

    ReportIf(!hwndTmp);
    // hwnd should be assigned in WM_NCCREATE
    ReportIf(hwndTmp != hwnd);
    ReportIf(this != HwndBaseFromHwnd(hwndTmp));
    if (!hwnd) {
        return nullptr;
    }

    DpiSetFromHwnd(hwnd);
    // trigger creating a backgroundBrush
    SetColors(kColorNoChange, args.bgColor);
    if (args.icon) {
        HwndSetIcon(hwnd, args.icon);
    }
    if (style & WS_VISIBLE) {
        if (style & WS_MAXIMIZE)
            ::ShowWindow(hwnd, SW_MAXIMIZE);
        else if (style & WS_MINIMIZE)
            ::ShowWindow(hwnd, SW_MINIMIZE);
        else
            ::ShowWindow(hwnd, SW_SHOWNORMAL);
    }
    return hwnd;
}

//--- WindowBase

WindowBase::WindowBase() {
    kind = kindWindow;
    GuiColorsInitIfNeeded();
}

WindowBase* WindowBase::AsWindowBase() {
    return this;
}

// What a window does when the system theme or colors changed: take the colors
// the OS draws its own UI in and repaint. An app with a theme of its own has
// already replaced the gui/ defaults with its palette (SumatraPDF does that in
// SumatraUpdateTheme()), and gGuiColorsFromSystem says so, so we leave those
// alone and only repaint.
// kColWin* resolved like VirtCtrl::GetColor(): the window's own textColor /
// bgColor when set, the gColsWin defaults otherwise, so a window follows the
// palette the app pushed into GuiColors without being told its colors
Color WindowBase::GetColor(int idx) const {
    const Color overrides[kColWinCount] = {textColor, bgColor};
    return GetCol(gColsWin, overrides, idx);
}

void (*gWindowBaseApplyDarkMode)(HWND) = nullptr;

void WindowBase::ApplyDarkMode() {
    if (gWindowBaseApplyDarkMode) {
        gWindowBaseApplyDarkMode(hwnd);
    }
}

struct RecolorChildrenCtx {
    Color txt = kColorUnset;
    Color bg = kColorUnset;
};

static BOOL CALLBACK RecolorChildProc(HWND hwnd, LPARAM lp) {
    auto* ctx = (RecolorChildrenCtx*)lp;
    ControlBase* c = ControlFromHwnd(hwnd);
    if (c) {
        c->SetColors(ctx->txt, ctx->bg);
    }
    return TRUE;
}

// The app pushed a new palette into GuiColors: take the gColsWin defaults over
// whatever explicit colors the last theme left behind. They are set explicitly,
// not just resolved on demand, because WM_ERASEBKGND only erases under a
// darkmode-subclassed checkbox when there is an explicit background (#5947)
void WindowBase::UpdateTheme() {
    if (!hwnd) {
        return;
    }
    Color colTxt = gColsWin[kColWinText];
    Color colBg = gColsWin[kColWinBg];
    SetColors(colTxt, colBg);
    // the native child controls carry explicit colors; the virtual ones paint
    // straight from the GuiColors defaults and need nothing
    RecolorChildrenCtx ctx{colTxt, colBg};
    EnumChildWindows(hwnd, RecolorChildProc, (LPARAM)&ctx);
    ApplyDarkMode();
    RedrawWindow(hwnd, nullptr, nullptr, RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN);
}

void WindowBase::OnThemeChange() {
    if (gGuiColorsFromSystem) {
        GuiSetDefaultColorsFromSystem();
    }
    if (!hwnd) {
        return;
    }
    uint flags = RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN;
    RedrawWindow(hwnd, nullptr, nullptr, flags);
}

LRESULT WindowBase::OnMessage(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    DpiScope dpi(hwnd);
    if (onWndProc.IsValid()) {
        WndProcEvent ev;
        ev.w = this;
        ev.hwnd = hwnd;
        ev.msg = msg;
        ev.wparam = wparam;
        ev.lparam = lparam;
        onWndProc.Call(&ev);
        if (ev.didHandle) {
            return ev.result;
        }
    }
    return WndProcDefault(hwnd, msg, wparam, lparam);
}

HWND WindowBase::CreateCustom(const CreateCustomArgs& args) {
    return CreateCustomHwnd(args, kDefaultClassName);
}

void WindowBase::SetVisibility(Visibility newVisibility) {
    ReportIf(!hwnd);
    visibility = newVisibility;
    bool isVisible = IsVisible();
    // ask the style, not GetParent(): for an owned popup (WS_POPUP plus
    // GWLP_HWNDPARENT, like the keyboard help) GetParent() returns the owner,
    // and just flipping WS_VISIBLE on a top-level window doesn't show it
    if (!HwndIsWindowStyleSet(hwnd, WS_CHILD)) {
        ::ShowWindow(hwnd, isVisible ? SW_SHOW : SW_HIDE);
    } else {
        BOOL bIsVisible = toBOOL(isVisible);
        HwndSetWindowStyle(hwnd, WS_VISIBLE, bIsVisible);
    }
}

Visibility WindowBase::GetVisibility() {
    return visibility;
}

void WindowBase::SetIsVisible(bool isVisible) {
    SetVisibility(isVisible ? Visibility::Visible : Visibility::Collapse);
}

bool WindowBase::IsVisible() const {
    return visibility == Visibility::Visible;
}

// default paint when onPaint is not set: virtual tree, or solid background
static void WindowBaseDefaultPaint(WindowBase* w, HDC hdc, PAINTSTRUCT* ps) {
    Color bg = w->GetColor(kColWinBg);
    if (w->vroot) {
        Rect rc = HwndClientRect(w->hwnd);
        Gfx* gfx = GfxCreateWithDoubleBuffer(w, hdc);
        gfx->FillRect(rc, bg);
        w->vroot->Paint(gfx, ToRect(ps->rcPaint));
        delete gfx;
        return;
    }
    auto* br = w->BackgroundBrush(bg);
    if (br != nullptr) {
        HdcFillRect(hdc, ToRect(ps->rcPaint), br);
    }
}

void WindowBase::SetFocusTo(ControlBase* c) {
    if (!c || !c->hwnd) {
        return;
    }
    // the win32 focus moving away from us clears the virtual focus (WM_KILLFOCUS)
    HwndSetFocusForce(c->hwnd);
}

void WindowBase::SetFocusTo(VirtCtrl* w) {
    if (!vroot || !w) {
        return;
    }
    // a virtual control has no HWND: we hold the focus on its behalf and route
    // the keys to it
    HwndSetFocusForce(hwnd);
    vroot->SetFocus(w);
}

bool WindowBase::TabNavigate(bool backwards) {
    if (!layout) {
        return false;
    }
    Vec<TabStop> stops;
    CollectTabStops(layout, stops);
    int n = len(stops);
    if (n == 0) {
        return false;
    }
    // where we are now: a virtual control if one has the focus, otherwise the
    // control that owns the win32 focus
    int idx = -1;
    VirtCtrl* focusedVirt = vroot ? vroot->focused : nullptr;
    HWND focusedHwnd = HwndThreadFocus();
    for (int i = 0; i < n && idx < 0; i++) {
        TabStop& ts = stops[i];
        if (focusedVirt && ts.vwnd == focusedVirt) {
            idx = i;
            break;
        }
        if (focusedVirt || !ts.ctrl) {
            continue;
        }
        // the focus can be on a child of the control rather than the control
        // itself - an editable ComboBox puts it on its inner Edit - so walk up
        // to this window. Without this every Tab restarts at the first stop
        for (HWND h = focusedHwnd; h && h != hwnd; h = ::GetParent(h)) {
            if (ts.ctrl->hwnd == h) {
                idx = i;
                break;
            }
        }
    }
    if (idx < 0) {
        idx = backwards ? n - 1 : 0;
    } else {
        idx = (idx + (backwards ? -1 : 1) + n) % n;
    }
    TabStop& ts = stops[idx];
    if (ts.vwnd) {
        SetFocusTo(ts.vwnd);
    } else {
        if (vroot) {
            vroot->SetFocus(nullptr);
        }
        SetFocusTo(ts.ctrl);
    }
    return true;
}

void WindowBase::Close() {
    ReportIf(!::IsWindow(hwnd));
    PostMessageW(hwnd, WM_CLOSE, 0, 0);
}

static void DoScheduledDelete(WindowBase* w) {
    if (w->onBeforeDelete.IsValid()) {
        w->onBeforeDelete.Call();
    }
    delete w;
}

// Deletes this window on the next message-loop turn: a window can't `delete
// this` while one of its own messages (WM_CLOSE / WM_DESTROY / a click
// handler) is still on the stack. Repeated calls post once, so wiring it to
// both onClose and onDestroy is safe. onBeforeDelete runs right before the
// delete, so the owner can clear the pointer it keeps to this window. Once
// scheduled, the window must not be deleted any other way.
void WindowBase::ScheduleDelete() {
    if (deleteScheduled) {
        return;
    }
    deleteScheduled = true;
    auto fn = MkFunc0(DoScheduledDelete, this);
    uitask::Post(fn, "WindowBase::ScheduleDelete");
}

static bool HwndOrAncestorComboIsDropped(HWND focus, HWND top) {
    for (HWND h = focus; h; h = ::GetParent(h)) {
        TempStr cls = HwndGetClassName(h);
        if (str::EqI(cls, StrL("ComboBox"))) {
            return SendMessageW(h, CB_GETDROPPEDSTATE, 0, 0) != 0;
        }
        if (h == top) {
            break;
        }
    }
    return false;
}

static bool HwndEditWantsReturn(HWND focus) {
    if (!focus) {
        return false;
    }
    TempStr cls = HwndGetClassName(focus);
    if (!str::EqI(cls, StrL("Edit"))) {
        return false;
    }
    DWORD style = (DWORD)GetWindowLongW(focus, GWL_STYLE);
    return (style & ES_WANTRETURN) != 0;
}

static bool IsHwndPushButton(HWND h) {
    if (!h) {
        return false;
    }
    TempStr cls = HwndGetClassName(h);
    if (!str::EqI(cls, StrL("Button"))) {
        return false;
    }
    DWORD type = (DWORD)GetWindowLongW(h, GWL_STYLE) & BS_TYPEMASK;
    return type == BS_PUSHBUTTON || type == BS_DEFPUSHBUTTON;
}

static VirtButton* FindDefaultVirtButton(ILayout* root) {
    if (!root) {
        return nullptr;
    }
    Vec<TabStop> stops;
    CollectTabStops(root, stops);
    for (TabStop& ts : stops) {
        VirtButton* b = AsVirtButton(ts.vwnd);
        if (b && b->IsDefault()) {
            return b;
        }
    }
    return nullptr;
}

// Enter clicks the focused button, or the default VirtButton when focus is
// on an edit / list / anything else. A dropped combo and a multiline edit
// with ES_WANTRETURN keep the key, like a native dialog.
bool WindowBase::ActivateOnEnter() {
    HWND focus = ::GetFocus();
    if (HwndOrAncestorComboIsDropped(focus, hwnd)) {
        return false;
    }
    if (HwndEditWantsReturn(focus)) {
        return false;
    }

    VirtButton* focusedBtn = nullptr;
    if (vroot && vroot->focused) {
        focusedBtn = AsVirtButton(vroot->focused);
    }
    if (focusedBtn && focusedBtn->Click()) {
        return true;
    }
    if (IsHwndPushButton(focus)) {
        SendMessageW(focus, BM_CLICK, 0, 0);
        return true;
    }
    VirtButton* def = FindDefaultVirtButton(layout);
    if (def && def != focusedBtn && def->Click()) {
        return true;
    }
    return false;
}

//--- mnemonic (&X in a label) navigation

// what a mnemonic can land on, in layout order: focusable controls (targeted
// directly when the mnemonic is their own) and non-focusable labels (their
// mnemonic focuses the next focusable stop, like statics in resource dialogs)
struct MnemonicStop {
    ControlBase* ctrl = nullptr;
    VirtCtrl* vwnd = nullptr;
    char mnemonic = 0;
    bool focusable = false;
};

static bool IsCtrlHwndFocusable(HWND h) {
    if (!h || !::IsWindowVisible(h) || !::IsWindowEnabled(h)) {
        return false;
    }
    DWORD style = (DWORD)GetWindowLongW(h, GWL_STYLE);
    return (style & WS_TABSTOP) != 0;
}

static void CollectMnemonicStopsVirt(VirtCtrl* w, Vec<MnemonicStop>& out) {
    if (!w || !w->IsHitTestable()) {
        return;
    }
    MnemonicStop ms;
    ms.vwnd = w;
    ms.focusable = w->HasFlag(vwfFocusable) && !w->HasFlag(vwfSkipTabStop);
    // parsed once when the control's text was set
    ms.mnemonic = w->mnemonic;
    if (ms.focusable || ms.mnemonic) {
        VecAppend(out, ms);
    }
    for (VirtCtrl* c : w->children) {
        CollectMnemonicStopsVirt(c, out);
    }
}

static void CollectMnemonicStops(ILayout* root, Vec<MnemonicStop>& out) {
    if (!root || IsCollapsed(root)) {
        return;
    }
    ControlBase* c = root->AsControl();
    if (c) {
        MnemonicStop ms;
        ms.ctrl = c;
        ms.focusable = IsCtrlHwndFocusable(c->hwnd);
        if (c->hwnd && ::IsWindowVisible(c->hwnd) && ::IsWindowEnabled(c->hwnd)) {
            ms.mnemonic = MnemonicCharInStr(HwndGetTextTemp(c->hwnd));
        }
        if (ms.focusable || ms.mnemonic) {
            VecAppend(out, ms);
        }
        return;
    }
    VirtCtrl* w = root->AsVirtCtrl();
    if (w) {
        CollectMnemonicStopsVirt(w, out);
        return;
    }
    int n = root->LayoutChildCount();
    for (int i = 0; i < n; i++) {
        CollectMnemonicStops(root->LayoutChildAt(i), out);
    }
}

// focus the control whose label contains &<c>; a focusable match is targeted
// directly (buttons / checkboxes are also clicked, like in a dialog), a match
// on a non-focusable label focuses the next focusable stop after it
bool WindowBase::MnemonicNavigate(char c) {
    if (!layout) {
        return false;
    }
    Vec<MnemonicStop> stops;
    CollectMnemonicStops(layout, stops);
    int n = len(stops);
    c = (char)toupper((u8)c);
    for (int i = 0; i < n; i++) {
        char m = stops[i].mnemonic;
        if (!m || (char)toupper((u8)m) != c) {
            continue;
        }
        // the match itself if focusable, else the next focusable stop after it
        for (int j = 0; j < n; j++) {
            MnemonicStop& target = stops[(i + j) % n];
            if (!target.focusable) {
                continue;
            }
            if (target.vwnd) {
                SetFocusTo(target.vwnd);
                VirtButton* b = AsVirtButton(target.vwnd);
                if (j == 0 && b) {
                    b->Click();
                }
                return true;
            }
            if (vroot) {
                vroot->SetFocus(nullptr);
            }
            SetFocusTo(target.ctrl);
            TempStr cls = HwndGetClassName(target.ctrl->hwnd);
            if (j == 0 && str::EqI(cls, StrL("Button"))) {
                SendMessageW(target.ctrl->hwnd, BM_CLICK, 0, 0);
            }
            return true;
        }
        return false;
    }
    return false;
}

// for interop with windows not wrapped in WindowBase, run this at the beginning of message loop
LRESULT TryReflectMessages(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    // hwnd is a parent of control sending WM_NOTIFY message
    switch (msg) {
        case WM_COMMAND: {
            // Reflect this message if it's from a control.
            ControlBase* pWnd = ControlFromHwnd(reinterpret_cast<HWND>(lparam));
            bool didHandle = false;
            if (pWnd != nullptr) {
                didHandle = pWnd->DispatchCommand(wparam, lparam);
            }
            if (didHandle) {
                return 1;
            }
        } break; // Note: Some MDI commands require default processing.
        case WM_NOTIFY: {
            // Do notification reflection if message came from a child window.
            // Restricting OnNotifyReflect to child windows avoids double handling.
            NMHDR* hdr = reinterpret_cast<LPNMHDR>(lparam);
            HWND from = hdr->hwndFrom;
            ControlBase* wndFrom = ControlFromHwnd(from);
            if (!wndFrom) {
                return 0;
            }
            if (hwnd == GetParent(wndFrom->hwnd)) {
                return wndFrom->DispatchNotifyReflect(wparam, lparam);
            }
        }
        // A set of messages to be reflected back to the control that generated them.
        case WM_CTLCOLORBTN:
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORDLG:
        case WM_CTLCOLORLISTBOX:
        case WM_CTLCOLORSCROLLBAR:
        case WM_CTLCOLORSTATIC:
        case WM_CHARTOITEM:
        case WM_VKEYTOITEM:
        case WM_HSCROLL:
        case WM_VSCROLL:
        case WM_PARENTNOTIFY: {
            ControlBase* pWnd = ControlFromHwnd(reinterpret_cast<HWND>(lparam));
            LRESULT result = 0;
            if (pWnd != nullptr) {
                result = pWnd->MessageReflect(msg, wparam, lparam);
            }
            if (result != 0) {
                return result;
            }
        } break;
        // owner-draw messages: lparam is a struct pointer, not an HWND
        case WM_DRAWITEM: {
            DRAWITEMSTRUCT* dis = (DRAWITEMSTRUCT*)lparam;
            ControlBase* pWnd = ControlFromHwnd(dis->hwndItem);
            if (pWnd != nullptr) {
                LRESULT result = pWnd->DispatchMessageReflect(msg, wparam, lparam);
                if (result != 0) {
                    return result;
                }
            }
        } break;
        case WM_MEASUREITEM: {
            HWND wnd = GetDlgItem(hwnd, static_cast<int>(wparam));
            if (!wnd && wparam == 0) {
                wnd = FindWindowExW(hwnd, nullptr, L"LISTBOX", nullptr);
            }
            ControlBase* pWnd = wnd ? ControlFromHwnd(wnd) : nullptr;
            if (pWnd != nullptr) {
                LRESULT result = pWnd->DispatchMessageReflect(msg, wparam, lparam);
                if (result != 0) {
                    return result;
                }
            }
        } break;
        case WM_DELETEITEM:
        case WM_COMPAREITEM: {
            HWND wnd = GetDlgItem(hwnd, static_cast<int>(wparam));
            ControlBase* pWnd = wnd ? ControlFromHwnd(wnd) : nullptr;
            if (pWnd != nullptr) {
                LRESULT result = pWnd->MessageReflect(msg, wparam, lparam);
                if (result != 0) {
                    return result;
                }
            }
        } break;
    }
    return 0;
}

LRESULT WindowBase::WndProcDefault(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    LRESULT result = 0;

    // WM_SETCURSOR: onSetCursor first so tooltips can update even when a virt
    // control later claims the cursor (FindBar / FindWindow button tips).
    if (msg == WM_SETCURSOR) {
        if (onSetCursor.IsValid()) {
            SetCursorEvent sev;
            sev.w = this;
            sev.hwndCursor = reinterpret_cast<HWND>(wparam);
            sev.hitTest = LOWORD(lparam);
            sev.mouseMsg = HIWORD(lparam);
            onSetCursor.Call(&sev);
            if (sev.didHandle) {
                return sev.result;
            }
        }
        if (vroot) {
            LRESULT res = 0;
            if (VirtTreeOnMessage(hwnd, vroot, msg, wparam, lparam, res)) {
                return res;
            }
        }
    } else if (vroot) {
        LRESULT res = 0;
        if (VirtTreeOnMessage(hwnd, vroot, msg, wparam, lparam, res)) {
            return res;
        }
    }

    WmEvent e{hwnd, msg, wparam, lparam, this->userData, this};

    if (msg == WM_CLOSE) {
        if (onClose.IsValid()) {
            WindowBase::CloseEvent ev;
            ev.e = &e;
            onClose.Call(&ev);
            if (ev.e->didHandle) {
                return 0;
            }
        }
        // TODO: should only send WM_DESTROY, the rest should be hooked in OnDestroy
        Destroy();
        return 0;
    }

    if (msg == WM_DESTROY) {
        if (onDestroy.IsValid()) {
            DestroyEvent ev;
            ev.e = &e;
            onDestroy.Call(&ev);
        }
        // no break because some controls require default processing.
    }

    switch (msg) {
        // windows don't support WM_GETFONT / WM_SETFONT
        // only controls do. not sure if we won't interfere
        // with control handling
        case WM_GETFONT: {
            return (LRESULT)GetHFont();
        }

        case WM_SETFONT: {
            font = GetPlatformFont((HFONT)wparam);
            if (!subclassId) {
                return 0;
            }
            // a subclassed window is a real control (edit, tree view etc.) that
            // draws its own text: remembering the font here isn't enough, the
            // control itself has to see WM_SETFONT. Fall through to the original
            // wndproc.
            break;
        }

        case WM_COMMAND: {
            // Subclassed controls (like ComboBox) receive WM_COMMAND from their
            // internal children. Don't handle it here — let the original wndproc
            // process it and send proper notifications to the parent window.
            if (subclassId) {
                break;
            }
            // Reflect this message if it's from a control.
            ControlBase* pWnd = ControlFromHwnd(reinterpret_cast<HWND>(lparam));
            bool didHandle = false;
            if (pWnd != nullptr) {
                didHandle = pWnd->DispatchCommand(wparam, lparam);
            }

            // Handle user commands.
            if (!didHandle && onCommand.IsValid()) {
                CommandEvent ev;
                ev.w = this;
                ev.wparam = wparam;
                ev.lparam = lparam;
                onCommand.Call(&ev);
                didHandle = ev.didHandle;
            }

            if (didHandle) {
                return 0;
            }
        } break; // Note: Some MDI commands require default processing.

        case WM_CREATE: {
            if (onCreate.IsValid()) {
                CreateEvent ev;
                ev.w = this;
                ev.cs = (CREATESTRUCT*)lparam;
                onCreate.Call(&ev);
            }
            break;
        }

        case WM_SETFOCUS: {
            if (onFocus.IsValid()) {
                FocusEvent ev;
                ev.w = this;
                onFocus.Call(&ev);
            }
            break;
        }

        case WM_ACTIVATE: {
            if (onActivate.IsValid()) {
                ActivateEvent ev;
                ev.w = this;
                ev.state = LOWORD(wparam);
                ev.minimized = HIWORD(wparam) != 0;
                ev.other = reinterpret_cast<HWND>(lparam);
                onActivate.Call(&ev);
                if (ev.didHandle) {
                    return 0;
                }
            }
            break;
        }

        case WM_DPICHANGED: {
            DpiSet((int)LOWORD(wparam), (int)HIWORD(wparam));
            if (onDpiChanged.IsValid()) {
                DpiChangedEvent ev;
                ev.w = this;
                ev.dpiX = LOWORD(wparam);
                ev.dpiY = HIWORD(wparam);
                ev.suggested = reinterpret_cast<RECT*>(lparam);
                onDpiChanged.Call(&ev);
                if (ev.didHandle) {
                    return 0;
                }
            }
            break;
        }

        case WM_NCHITTEST: {
            if (onNcHitTest.IsValid()) {
                NcHitTestEvent ev;
                ev.w = this;
                ev.screenPos = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
                onNcHitTest.Call(&ev);
                if (ev.didHandle) {
                    return ev.result;
                }
            }
            break;
        }

        case WM_SHOWWINDOW: {
            if (onShowWindow.IsValid()) {
                ShowWindowEvent ev;
                ev.w = this;
                ev.show = wparam != FALSE;
                ev.status = lparam;
                onShowWindow.Call(&ev);
                if (ev.didHandle) {
                    return 0;
                }
            }
            break;
        }

        case WM_KILLFOCUS: {
            // we were holding the focus for a virtual control; it's gone now
            if (vroot) {
                vroot->SetFocus(nullptr);
            }
            break;
        }

        case WM_NOTIFY: {
            // Do notification reflection if message came from a child window.
            // Restricting OnNotifyReflect to child windows avoids double handling.
            NMHDR* hdr = reinterpret_cast<NMHDR*>(lparam);
            HWND from = hdr->hwndFrom;
            ControlBase* wndFrom = ControlFromHwnd(from);

            if (wndFrom != nullptr) {
                if (::GetParent(from) == this->hwnd) {
                    result = wndFrom->DispatchNotifyReflect(wparam, lparam);
                }
            }

            // Handle user notifications
            if (result == 0 && onNotify.IsValid()) {
                NotifyEvent nev;
                nev.w = this;
                nev.controlId = (int)wparam;
                nev.nmh = (NMHDR*)lparam;
                onNotify.Call(&nev);
                result = nev.result;
            }
            if (result != 0) {
                return result;
            }
            break;
        }

        // the system broadcasts both to every top-level window when its palette
        // or visual style changed
        case WM_SYSCOLORCHANGE:
        case WM_THEMECHANGED:
            OnThemeChange();
            break;

        case WM_ERASEBKGND: {
            // claim handled so DefWindowProc does not fill with the class brush
            // (custom windows paint the full client in WM_PAINT). But fill with
            // our background when we have one: this message also arrives on
            // behalf of a child - a themed / darkmode checkbox erases its
            // background through the parent (DrawThemeParentBackground) - and
            // claiming "erased" while painting nothing left the child's pixels
            // stale, so its label text accumulated on every repaint (#5947)
            if (!shouldEraseBackground) {
                HDC hdc = (HDC)wparam;
                auto* br = BackgroundBrush();
                if (hdc && br) {
                    // A virt tree paints the full client through a
                    // double-buffer. Filling here on a full-window erase
                    // flashes during arrow-key list navigation.
                    // Still fill when the DC is clipped to a child.
                    bool fill = !vroot;
                    if (!fill) {
                        RECT clip{};
                        GetClipBox(hdc, &clip);
                        Rect client = HwndClientRect(hwnd);
                        fill = clip.left > 0 || clip.top > 0 || clip.right < client.dx || clip.bottom < client.dy;
                    }
                    if (fill) {
                        HdcFillRect(hdc, HwndClientRect(hwnd), br);
                    }
                }
                return TRUE;
            }
            break;
        }

        case WM_PRINTCLIENT: {
            // DrawThemeParentBackground: a darkmodelib-subclassed checkbox /
            // radio asks its parent to paint the background under it through
            // this (the dc is clipped and shifted to the child's rect). Not
            // answering leaves the child's pixels stale, so its label text
            // accumulated on every repaint (#5947). The resolved color, so a
            // window that paints in the gColsWin default answers correctly too
            if (subclassId) {
                // a subclassed native window answers it itself
                break;
            }
            HDC hdc = (HDC)wparam;
            auto* br = BackgroundBrush(GetColor(kColWinBg));
            if (hdc && br) {
                HdcFillRect(hdc, HwndClientRect(hwnd), br);
                return 0;
            }
            break;
        }

        case WM_PAINT: {
            if (subclassId) {
                // Allow window controls to do their default drawing.
                return FinalWindowProc(msg, wparam, lparam);
            }

            {
                PAINTSTRUCT ps;
                HDC hdc = ::BeginPaint(hwnd, &ps);
                if (onPaint.IsValid()) {
                    PaintEvent ev;
                    ev.w = this;
                    ev.hdc = hdc;
                    ev.ps = &ps;
                    onPaint.Call(&ev);
                } else {
                    WindowBaseDefaultPaint(this, hdc, &ps);
                }
                ::EndPaint(hwnd, &ps);
            }
            // No more drawing required
            return 0;
        }

        // A set of messages to be reflected back to the control that generated them.
        case WM_CTLCOLORBTN:
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORDLG:
        case WM_CTLCOLORLISTBOX:
        case WM_CTLCOLORSCROLLBAR:
        case WM_CTLCOLORSTATIC:
        case WM_DRAWITEM:
        case WM_MEASUREITEM:
        case WM_DELETEITEM:
        case WM_COMPAREITEM:
        case WM_CHARTOITEM:
        case WM_VKEYTOITEM:
        case WM_HSCROLL:
        case WM_VSCROLL:
        case WM_PARENTNOTIFY: {
            result = MessageReflect(msg, wparam, lparam);
            if (result != 0) return result; // Message processed so return.
        } break;                            // Do default processing when message not already processed.

        case WM_DROPFILES: {
            if (onDropFiles.IsValid()) {
                DropFilesEvent ev;
                ev.w = this;
                ev.dropInfo = reinterpret_cast<HDROP>(wparam);
                onDropFiles.Call(&ev);
            }
            break;
        }

        case WM_ENTERSIZEMOVE:
        case WM_EXITSIZEMOVE: {
            if (onSize.IsValid()) {
                SizeEvent ev;
                ev.w = this;
                ev.msg = msg;
                onSize.Call(&ev);
            }
            break;
        }
        case WM_GETMINMAXINFO: {
            if (onGetMinMaxInfo.IsValid()) {
                GetMinMaxInfoEvent ev;
                ev.w = this;
                ev.mmi = reinterpret_cast<MINMAXINFO*>(lparam);
                onGetMinMaxInfo.Call(&ev);
            }
            break;
        }
        case WM_LBUTTONDOWN:
        case WM_MBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_MBUTTONUP:
        case WM_RBUTTONUP:
        case WM_MOUSEACTIVATE:
        case WM_MOUSEHOVER:
        case WM_MOUSEHWHEEL:
        case WM_MOUSELEAVE:
        case WM_MOUSEMOVE:
        case WM_MOUSEWHEEL: {
            if (onMouseEvent.IsValid()) {
                MouseEvent ev;
                ev.w = this;
                ev.msg = msg;
                ev.wparam = wparam;
                ev.lparam = lparam;
                onMouseEvent.Call(&ev);
                if (ev.didHandle) {
                    return ev.result;
                }
            }
            break;
        }
        case WM_MOVE: {
            if (onMove.IsValid()) {
                POINTS pts = MAKEPOINTS(lparam);
                MoveEvent ev;
                ev.w = this;
                ev.pts = &pts;
                onMove.Call(&ev);
            }
            break;
        }

        case WM_SIZE: {
            Size size = {LOWORD(lparam), HIWORD(lparam)};
            // most WindowBase windows just want `layout` stretched to the new
            // client size. CreateCustom sends WM_SIZE before the tree exists,
            // so `layout` being null is the usual "not ready yet" guard.
            if (autoLayout && layout && (size.dx > 0) && (size.dy > 0)) {
                DoLayout(size);
                HwndInvalidate(hwnd);
            }
            if (onSize.IsValid()) {
                SizeEvent ev;
                ev.w = this;
                ev.msg = msg;
                ev.type = static_cast<UINT>(wparam);
                ev.size = size;
                onSize.Call(&ev);
            }
            break;
        }
        case WM_TIMER: {
            if (onTimer.IsValid()) {
                TimerEvent ev;
                ev.w = this;
                ev.timerId = static_cast<UINT_PTR>(wparam);
                onTimer.Call(&ev);
            }
            break;
        }
        case WM_WINDOWPOSCHANGING: {
            if (onWindowPosChanging.IsValid()) {
                WindowPosChangingEvent ev;
                ev.w = this;
                ev.windowPos = reinterpret_cast<LPWINDOWPOS>(lparam);
                onWindowPosChanging.Call(&ev);
            }
            break;
        }

        default: {
            if (msg == kWmTaskbarCreated || msg == kWmTaskbarButtonCreated || msg == kWmTaskbarCallback) {
                if (onTaskbarCallback.IsValid()) {
                    TaskbarCallbackEvent ev;
                    ev.w = this;
                    ev.msg = msg;
                    ev.lparam = lparam;
                    onTaskbarCallback.Call(&ev);
                }
                return 0;
            }
            break;
        }
    }

    // Now hand all messages to the default procedure.
    return FinalWindowProc(msg, wparam, lparam);
}

// PreTranslate: onPreTranslate first (WM_CHAR / KEYUP / etc.), then key-downs
// via onKeyDown (so dialog shortcuts work while focus is on a child HWND), then
// closeOnEsc / closeOnCtrlW, then Enter (focused or default button), then
// default Tab among mixed HWND + virtual controls.
// WM_CHAR Escape is needed because an Edit can eat KEYDOWN Escape (IME / some
// locales); KEYDOWN is still handled so we close before TranslateMessage.
bool WindowBase::PreTranslateMessage(MSG& msg) {
    // runs from the message loop, outside any wndproc's DpiScope
    DpiScope dpiScope(hwnd);
    if (onPreTranslate.IsValid()) {
        PreTranslateEvent pev;
        pev.w = this;
        pev.msg = &msg;
        onPreTranslate.Call(&pev);
        if (pev.didHandle) {
            return true;
        }
    }
    if (closeOnEsc && msg.message == WM_CHAR && msg.wParam == VK_ESCAPE) {
        Close();
        return true;
    }
    // Alt+<char> (WM_SYSCHAR), or a plain <char> when the focused control
    // doesn't consume characters: jump to the control labeled &<char>
    // (mnemonics, lost when resource dialogs became WindowBase)
    if (msg.message == WM_SYSCHAR || msg.message == WM_CHAR) {
        char c = (msg.wParam > 0x20 && msg.wParam < 128) ? (char)msg.wParam : 0;
        if (c && layout) {
            bool tryMnemonic = (msg.message == WM_SYSCHAR) && IsAltPressed();
            if (!tryMnemonic && msg.message == WM_CHAR) {
                HWND focus = ::GetFocus();
                LRESULT code = focus ? SendMessageW(focus, WM_GETDLGCODE, 0, 0) : 0;
                tryMnemonic = (code & DLGC_WANTCHARS) == 0;
            }
            if (tryMnemonic && MnemonicNavigate(c)) {
                return true;
            }
        }
    }
    if (msg.message != WM_KEYDOWN && msg.message != WM_SYSKEYDOWN) {
        return false;
    }
    KeyEvent ev;
    ev.hwnd = msg.hwnd;
    ev.vkey = (int)msg.wParam;
    ev.isCtrl = IsCtrlPressed();
    ev.isShift = IsShiftPressed();
    ev.isAlt = IsAltPressed();
    ev.isSysKey = (msg.message == WM_SYSKEYDOWN);
    if (onKeyDown.IsValid()) {
        onKeyDown.Call(&ev);
        if (ev.didHandle) {
            return true;
        }
    }
    if (closeOnEsc && ev.vkey == VK_ESCAPE) {
        Close();
        return true;
    }
    if (closeOnCtrlW && ev.vkey == 'W' && ev.isCtrl && !ev.isAlt) {
        Close();
        return true;
    }
    if (closeOnF1 && ev.vkey == VK_F1 && !ev.isCtrl && !ev.isShift && !ev.isAlt) {
        Close();
        return true;
    }
    if (ev.vkey == VK_RETURN && !ev.isCtrl && !ev.isAlt) {
        if (ActivateOnEnter()) {
            return true;
        }
    }
    // default Tab among mixed HWND + virtual controls
    if (ev.vkey != VK_TAB || !layout || ev.isCtrl || ev.isAlt) {
        return false;
    }
    return TabNavigate(ev.isShift);
}

void WindowBase::Attach(HWND hwnd) {
    ReportIf(!IsWindow(hwnd));
    ReportIf(HwndBaseFromHwnd(hwnd));

    this->hwnd = hwnd;
    DpiSetFromHwnd(hwnd);
    Subclass();
    if (onAttach.IsValid()) {
        AttachEvent ev;
        ev.w = this;
        onAttach.Call(&ev);
    }
}

int WindowBase::GetDpi() const {
    return hwnd ? RoundUp(DpiGetForHwnd(hwnd), 4) : DpiGet();
}

//--- ControlBase

ControlBase::ControlBase() {
    kind = kindControl;
    GuiColorsInitIfNeeded();
}

ControlBase* ControlBase::AsControlBase() {
    return this;
}

LRESULT ControlBase::OnMessage(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    DpiScope dpi(hwnd);
    if (onWndProc.IsValid()) {
        WndProcEvent ev;
        ev.w = this;
        ev.hwnd = hwnd;
        ev.msg = msg;
        ev.wparam = wparam;
        ev.lparam = lparam;
        onWndProc.Call(&ev);
        if (ev.didHandle) {
            return ev.result;
        }
    }
    return WndProcDefault(hwnd, msg, wparam, lparam);
}

void ControlBase::SetVisibility(Visibility newVisibility) {
    ReportIf(!hwnd);
    if (visibility == newVisibility) {
        return;
    }
    visibility = newVisibility;
    bool isVisible = IsVisible();
    // see WindowBase::SetVisibility(): only a WS_CHILD window can be shown by
    // flipping WS_VISIBLE
    if (!HwndIsWindowStyleSet(hwnd, WS_CHILD)) {
        ::ShowWindow(hwnd, isVisible ? SW_SHOW : SW_HIDE);
    } else {
        BOOL bIsVisible = toBOOL(isVisible);
        HwndSetWindowStyle(hwnd, WS_VISIBLE, bIsVisible);
    }
}

void ControlBase::SetIsVisible(bool isVisible) {
    SetVisibility(isVisible ? Visibility::Visible : Visibility::Collapse);
}

bool ControlBase::IsVisible() const {
    return visibility == Visibility::Visible;
}

bool ControlBase::DispatchCommand(WPARAM wparam, LPARAM lparam) {
    if (!onCommand.IsValid()) {
        return false;
    }
    CommandEvent ev;
    ev.w = this;
    ev.wparam = wparam;
    ev.lparam = lparam;
    onCommand.Call(&ev);
    return ev.didHandle;
}

LRESULT ControlBase::DispatchMessageReflect(UINT msg, WPARAM wparam, LPARAM lparam) {
    if (!onMessageReflect.IsValid()) {
        return 0;
    }
    MessageReflectEvent ev;
    ev.w = this;
    ev.msg = msg;
    ev.wparam = wparam;
    ev.lparam = lparam;
    onMessageReflect.Call(&ev);
    return ev.result;
}

LRESULT ControlBase::DispatchNotifyReflect(WPARAM wparam, LPARAM lparam) {
    if (!onNotifyReflect.IsValid()) {
        return 0;
    }
    NotifyReflectEvent ev;
    ev.w = this;
    ev.wparam = wparam;
    ev.lparam = lparam;
    onNotifyReflect.Call(&ev);
    return ev.result;
}

static void ControlBaseDefaultPaint(ControlBase* w, HDC hdc, PAINTSTRUCT* ps) {
    auto* br = w->BackgroundBrush();
    if (br != nullptr) {
        HdcFillRect(hdc, ToRect(ps->rcPaint), br);
    }
}

Size ControlBase::GetIdealSize() {
    return {};
}

Size ControlBase::Layout(const Constraints bc) {
    dbglayout(fmt("ControlBase::Layout() %s ", Str(GetKind())));
    LogConstraints(bc, StrL("\n"));

    auto hinset = insets.left + insets.right;
    auto vinset = insets.top + insets.bottom;
    auto innerConstraints = bc.Inset(hinset, vinset);

    int dx = MinIntrinsicWidth(0);
    int dy = MinIntrinsicHeight(0);
    childSize = innerConstraints.Constrain(Size{dx, dy});
    auto res = Size{
        childSize.dx + hinset,
        childSize.dy + vinset,
    };
    return res;
}

int ControlBase::MinIntrinsicHeight(int /*width*/) {
    Size s = GetIdealSize();
    return s.dy;
}

int ControlBase::MinIntrinsicWidth(int /*height*/) {
    Size s = GetIdealSize();
    return s.dx;
}

ControlBase* ControlBase::AsControl() {
    return this;
}

void ControlBase::SetBounds(Rect bounds) {
    dbglayout(
        fmt("ControlBase:SetBounds() %s %d,%d - %d, %d\n", Str(GetKind()), bounds.x, bounds.y, bounds.dx, bounds.dy));

    lastBounds = bounds;

    bounds.x += insets.left;
    bounds.y += insets.top;
    bounds.dx -= (insets.right + insets.left);
    bounds.dy -= (insets.bottom + insets.top);

    if (mapRtlX) {
        bounds.x = HwndMapChildXForRtlParent(GetParent(hwnd), bounds.x, bounds.dx);
    }
    // Skip a no-op MoveWindow (it still sends WM_WINDOWPOSCHANGED and flashes
    // the TOC). Compare the HWND, not lastBounds: a parent DeferWindowPos can
    // leave the child at a stale client y while lastBounds still matches layout
    // (toolbar page box ended up below the bar).
    if (hwnd && ChildPosWithinParent(hwnd) == bounds) {
        return;
    }
    // Do not MoveWindow(..., TRUE): each child would paint immediately as
    // layout walks the tree, which flashes when many dropdowns/trackbars
    // move. Position without painting, then invalidate so they paint with
    // the parent on the next WM_PAINT. SWP_NOCOPYBITS skips the smeared
    // copy of old pixels into the new place.
    // SWP_NOREDRAW also skips invalidating the parent where this child
    // used to be, so a dropdown first shown at (0,0) then moved left a
    // ghost over nearby contents.
    if (hwnd) {
        HWND parent = GetParent(hwnd);
        RECT oldOnParent{};
        if (parent) {
            Rect old = HwndMapRectToWindow(HwndClientRect(hwnd), hwnd, parent);
            oldOnParent = ToRECT(old);
        }
        SetWindowPos(hwnd, nullptr, bounds.x, bounds.y, bounds.dx, bounds.dy,
                     SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOCOPYBITS | SWP_NOREDRAW);
        if (parent && !IsRectEmpty(&oldOnParent)) {
            // Invalidate, don't erase: a parent fill here flashed the
            // window on every list selection. WM_PAINT covers virt
            // leftovers; ALLCHILDREN covers native child edits.
            RedrawWindow(parent, &oldOnParent, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN);
        }
        InvalidateRect(hwnd, nullptr, FALSE);
    }
}

LRESULT ControlBase::WndProcDefault(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    LRESULT result = 0;

    WmEvent e{hwnd, msg, wparam, lparam, this->userData, this};

    if (msg == WM_DESTROY) {
        if (onDestroy.IsValid()) {
            DestroyEvent ev;
            ev.e = &e;
            onDestroy.Call(&ev);
        }
        // no break because some controls require default processing.
    }

    switch (msg) {
        // windows don't support WM_GETFONT / WM_SETFONT
        // only controls do. not sure if we won't interfere
        // with control handling
        case WM_GETFONT: {
            return (LRESULT)GetHFont();
        }

        case WM_SETFONT: {
            font = GetPlatformFont((HFONT)wparam);
            if (!subclassId) {
                return 0;
            }
            // a subclassed window is a real control (edit, tree view etc.) that
            // draws its own text: remembering the font here isn't enough, the
            // control itself has to see WM_SETFONT. Fall through to the original
            // wndproc.
            break;
        }

        case WM_COMMAND: {
            // Subclassed controls (like ComboBox) receive WM_COMMAND from their
            // internal children. Don't handle it here — let the original wndproc
            // process it and send proper notifications to the parent window.
            if (subclassId) {
                break;
            }
            // Reflect this message if it's from a control.
            ControlBase* pWnd = ControlFromHwnd(reinterpret_cast<HWND>(lparam));
            bool didHandle = false;
            if (pWnd != nullptr) {
                didHandle = pWnd->DispatchCommand(wparam, lparam);
            }

            // Handle user commands.
            if (!didHandle) {
                didHandle = DispatchCommand(wparam, lparam);
            }

            if (didHandle) {
                return 0;
            }
        } break; // Note: Some MDI commands require default processing.

        case WM_CREATE: {
            if (onCreate.IsValid()) {
                CreateEvent ev;
                ev.w = this;
                ev.cs = (CREATESTRUCT*)lparam;
                onCreate.Call(&ev);
            }
            break;
        }

        case WM_DPICHANGED: {
            DpiSet((int)LOWORD(wparam), (int)HIWORD(wparam));
            break;
        }

        case WM_SETFOCUS: {
            if (onFocus.IsValid()) {
                FocusEvent ev;
                ev.w = this;
                onFocus.Call(&ev);
            }
            break;
        }

        case WM_NOTIFY: {
            // Do notification reflection if message came from a child window.
            // Restricting OnNotifyReflect to child windows avoids double handling.
            NMHDR* hdr = reinterpret_cast<NMHDR*>(lparam);
            HWND from = hdr->hwndFrom;
            ControlBase* wndFrom = ControlFromHwnd(from);

            if (wndFrom != nullptr) {
                if (::GetParent(from) == this->hwnd) {
                    result = wndFrom->DispatchNotifyReflect(wparam, lparam);
                }
            }

            // Handle user notifications
            if (result == 0 && onNotify.IsValid()) {
                NotifyEvent nev;
                nev.w = this;
                nev.controlId = (int)wparam;
                nev.nmh = (NMHDR*)lparam;
                onNotify.Call(&nev);
                result = nev.result;
            }
            if (result != 0) {
                return result;
            }
            break;
        }

        case WM_ERASEBKGND: {
            // TreeView sets shouldEraseBackground false so WM_PAINT covers.
            // Filling here blanks the control on every resize; the #5947
            // parent-background fill belongs on WindowBase (checkbox
            // DrawThemeParentBackground target), not on native controls.
            if (!shouldEraseBackground) {
                return TRUE;
            }
            break;
        }

        case WM_PRINTCLIENT: {
            // see WindowBase::WndProcDefault: background under a
            // darkmodelib-subclassed checkbox / radio child (#5947)
            if (subclassId) {
                break;
            }
            HDC hdc = (HDC)wparam;
            auto* br = BackgroundBrush();
            if (hdc && br) {
                HdcFillRect(hdc, HwndClientRect(hwnd), br);
                return 0;
            }
            break;
        }

        case WM_PAINT: {
            if (subclassId) {
                // Allow window controls to do their default drawing.
                return FinalWindowProc(msg, wparam, lparam);
            }

            {
                PAINTSTRUCT ps;
                HDC hdc = ::BeginPaint(hwnd, &ps);
                if (onPaint.IsValid()) {
                    PaintEvent pev;
                    pev.w = this;
                    pev.hdc = hdc;
                    pev.ps = &ps;
                    onPaint.Call(&pev);
                } else {
                    ControlBaseDefaultPaint(this, hdc, &ps);
                }
                ::EndPaint(hwnd, &ps);
            }
            // No more drawing required
            return 0;
        }

        // A set of messages to be reflected back to the control that generated them.
        case WM_CTLCOLORBTN:
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORDLG:
        case WM_CTLCOLORLISTBOX:
        case WM_CTLCOLORSCROLLBAR:
        case WM_CTLCOLORSTATIC:
        case WM_DRAWITEM:
        case WM_MEASUREITEM:
        case WM_DELETEITEM:
        case WM_COMPAREITEM:
        case WM_CHARTOITEM:
        case WM_VKEYTOITEM:
        case WM_HSCROLL:
        case WM_VSCROLL:
        case WM_PARENTNOTIFY: {
            result = MessageReflect(msg, wparam, lparam);
            if (result != 0) return result; // Message processed so return.
        } break;                            // Do default processing when message not already processed.

        case WM_ENTERSIZEMOVE:
        case WM_EXITSIZEMOVE: {
            if (onSize.IsValid()) {
                SizeEvent sev;
                sev.w = this;
                sev.msg = msg;
                onSize.Call(&sev);
            }
            break;
        }
        case WM_LBUTTONDOWN:
        case WM_MBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_MBUTTONUP:
        case WM_RBUTTONUP:
        case WM_MOUSEACTIVATE:
        case WM_MOUSEHOVER:
        case WM_MOUSEHWHEEL:
        case WM_MOUSELEAVE:
        case WM_MOUSEMOVE:
        case WM_MOUSEWHEEL: {
            if (onMouseEvent.IsValid()) {
                MouseEvent mev;
                mev.w = this;
                mev.msg = msg;
                mev.wparam = wparam;
                mev.lparam = lparam;
                onMouseEvent.Call(&mev);
                if (mev.didHandle) {
                    return mev.result;
                }
            }
            break;
        }
        case WM_CONTEXTMENU: {
            if (onContextMenu.IsValid()) {
                Point ptScreen = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
                // https://docs.microsoft.com/en-us/windows/win32/menurc/wm-contextmenu
                ContextMenuEvent cev;
                cev.w = this;
                cev.mouseScreen = ptScreen;
                Point ptW = ptScreen;
                if (ptScreen.x != -1) {
                    ptW = HwndMapWindowPoint(HWND_DESKTOP, hwnd, ptW);
                }
                cev.mouseWindow = ptW;
                onContextMenu.Call(&cev);
            }
            break;
        }

        case WM_SIZE: {
            if (onSize.IsValid()) {
                SizeEvent sev;
                sev.w = this;
                sev.msg = msg;
                sev.type = static_cast<UINT>(wparam);
                sev.size = {LOWORD(lparam), HIWORD(lparam)};
                onSize.Call(&sev);
            }
            break;
        }
        case WM_TIMER: {
            if (onTimer.IsValid()) {
                TimerEvent tev;
                tev.w = this;
                tev.timerId = static_cast<UINT_PTR>(wparam);
                onTimer.Call(&tev);
            }
            break;
        }
    }

    // Now hand all messages to the default procedure.
    return FinalWindowProc(msg, wparam, lparam);
}

void ControlBase::Attach(HWND hwnd) {
    ReportIf(!IsWindow(hwnd));
    ReportIf(HwndBaseFromHwnd(hwnd));

    this->hwnd = hwnd;
    DpiSetFromHwnd(hwnd);
    Subclass();
    if (onAttach.IsValid()) {
        AttachEvent ev;
        ev.w = this;
        onAttach.Call(&ev);
    }
}

// Attaches a CWnd object to a dialog item.
void ControlBase::AttachDlgItem(UINT id, HWND parent) {
    ReportIf(!::IsWindow(parent));
    HWND wnd = ::GetDlgItem(parent, (int)id);
    Attach(wnd);
}

HWND ControlBase::CreateControl(const CreateControlArgs& args) {
    ReportIf(len(args.className) == 0);
    // TODO: validate that className is one of the known controls?

    font = args.font;
    if (!font) {
        font = GetDefaultGuiFont();
    }

    DWORD style = args.style;
    if (args.parent) {
        style |= WS_CHILD;
    }
    if (args.visible) {
        style |= WS_VISIBLE;
    } else {
        style &= ~WS_VISIBLE;
    }
    DWORD exStyle = args.exStyle;
    if (args.isRtl) {
        exStyle |= WS_EX_LAYOUTRTL | WS_EX_NOINHERITLAYOUT;
    }
    int x = args.pos.x;
    int y = args.pos.y;
    int dx = args.pos.dx;
    int dy = args.pos.dy;
    HWND parent = args.parent;
    HMENU id = args.ctrlId;
    HINSTANCE inst = GetInstance();
    void* createParams = static_cast<HwndBase*>(this);
    hwnd = ::CreateWindowExW(exStyle, args.className.s, L"", style, x, y, dx, dy, parent, id, inst, createParams);
    ReportIf(!hwnd);
    if (!hwnd) {
        return nullptr;
    }
    HwndSetFont(hwnd, GetHFont());
    DpiSetFromHwnd(hwnd);

    Subclass();
    if (onAttach.IsValid()) {
        AttachEvent ev;
        ev.w = this;
        onAttach.Call(&ev);
    }

    if (args.text) {
        SetText(args.text);
    }
    return hwnd;
}

HWND ControlBase::CreateCustom(const CreateCustomArgs& args) {
    return CreateCustomHwnd(args, kControlClassName);
}

void ControlBase::SetInsetsPt(int uniform) {
    insets = DpiScaledInsets(uniform);
}

void ControlBase::SetInsetsPt(int topBottom, int leftRight) {
    insets = DpiScaledInsets(topBottom, leftRight);
}

void ControlBase::SetInsetsPt(int top, int right, int bottom, int left) {
    insets = DpiScaledInsets(top, right, bottom, left);
}

bool ControlBase::IsFocused() const {
    return hwnd && HwndIsFocused(hwnd);
}

void ControlBase::SetFocus() {
    if (hwnd) {
        HwndSetFocus(hwnd);
    }
}

// size a control to what it says it wants
void SizeToIdealSize(ControlBase* c) {
    if (!c || !c->hwnd) {
        return;
    }
    auto size = c->GetIdealSize();
    // TODO: don't change x,y, only dx/dy
    RECT r{0, 0, size.dx, size.dy};
    c->SetBounds(r);
}

//--- message loops

// application.cpp
bool PreTranslateMessage(MSG& msg) {
    bool shouldProcess = (WM_KEYFIRST <= msg.message && msg.message <= WM_KEYLAST) ||
                         (WM_MOUSEFIRST <= msg.message && msg.message <= WM_MOUSELAST);
    if (!shouldProcess) {
        return false;
    }
    for (HWND hwnd = msg.hwnd; hwnd != nullptr; hwnd = ::GetParent(hwnd)) {
        auto* wnd = WindowBaseFromHwnd(hwnd);
        if (wnd && wnd->PreTranslateMessage(msg)) {
            return true;
        }
    }
    return false;
}

int RunMessageLoop(HACCEL accelTable, HWND hwndDialog) {
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        if (PreTranslateMessage(msg)) {
            continue;
        }
        if (TranslateAccelerator(msg.hwnd, accelTable, &msg)) {
            continue;
        }
        if (hwndDialog && IsDialogMessage(hwndDialog, &msg)) {
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}

// Runs a message loop until hwndDialog is destroyed, making it modal to
// hwndParent (disabled for the duration). A WM_QUIT ends the loop too and is
// re-posted so the outer message loop sees it. Keyboard handling (Esc, Enter,
// Tab) goes through the same PreTranslateMessage as the main loop.
void RunModalWindow(HWND hwndDialog, HWND hwndParent) {
    // Disabling an ancestor also disables the dialog and deadlocks this loop.
    ReportIf(hwndParent && ::IsChild(hwndParent, hwndDialog));

    bool reEnableParent = false;
    if (hwndParent && IsWindowEnabled(hwndParent)) {
        EnableWindow(hwndParent, FALSE);
        reEnableParent = true;
    }

    MSG msg;
    while (::IsWindow(hwndDialog)) {
        BOOL ok = GetMessage(&msg, nullptr, 0, 0);
        if (!ok) {
            // WM_QUIT: leave and let the outer loop see it too
            PostQuitMessage((int)msg.wParam);
            break;
        }
        if (PreTranslateMessage(msg)) {
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    if (reEnableParent) {
        EnableWindow(hwndParent, TRUE);
        SetActiveWindow(hwndParent);
    }
}

// http://www.guyswithtowels.com/blog/10-things-i-hate-about-win32.html#ModelessDialogs
// to implement a standard dialog navigation we need to call
// IsDialogMessage(hwnd) in message loop.
// hwnd has to be current top-level window that is modeless dialog
// we need to manually maintain this window
static HWND g_currentModelessDialog = nullptr;

// TODO: those are hacks
HWND GetCurrentModelessDialog() {
    return g_currentModelessDialog;
}

// set to nullptr to disable
void SetCurrentModelessDialog(HWND hwnd) {
    g_currentModelessDialog = hwnd;
}
