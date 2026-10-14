/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// SumatraPDF's half of the screenshot feature: the global hotkey, the dialog
// that sets it, and the hooks the shared capture code calls back into.
// Capturing and the picker overlay live in ScreenshotCapture.cpp.

#include "base/Base.h"
#include "base/AutoWin.h"
#include "base/File.h"
#include "base/Win.h"
#include "gui/Dpi.h"

#include "gui/UIModels.h"
#include "gui/Layout.h"
#include "gui/win/WinGui.h"
#include "gui/PlatformFont.h"
#include "gui/Gfx.h"
#include "gui/VirtCtrl.h"

#include "Notifications.h"
#include "AppTools.h"
#include "ScreenshotCapture.h"
#include "ShortcutParse.h"
#include "Theme.h"
#include "SumatraConfig.h"
#include "DarkMode.h"
#include "Commands.h"
#include "Accelerators.h"
#include "Settings.h"
#include "AppSettings.h"
#include "MainWindow.h"
#include "SumatraPDF.h"
#include "Translations.h"
#include "GlobalHotkeys.h"
#include "SumatraDialogs.h"
#include "shared/PdfToolsCommon.h"
#include "Screenshot.h"
#include "AppHelpersCommon.h"
#include "ScreenshotCommon.h"

static bool IsAppFrame(HWND hwnd) {
    for (MainWindow* win : gWindows) {
        if (win->hwndFrame == hwnd) {
            return true;
        }
    }
    return false;
}

static HWND GetScreenshotOwnerHwnd() {
    return len(gWindows) > 0 ? gWindows[0]->hwndFrame : nullptr;
}

// wires up the hooks the shared capture code (ScreenshotCapture.h) calls back
// into; TakeScreenshots() itself is declared there
void InitScreenshotHost() {
    gScreenshotHost.IsAppFrame = IsAppFrame;
    gScreenshotHost.GetSaveDirTemp = GetScreenshotSaveDirTemp;
    gScreenshotHost.GetOwnerHwnd = GetScreenshotOwnerHwnd;
}

// --- Set Screenshot Hotkey dialog ---

// WM_KEYDOWN doesn't fire for VK_SNAPSHOT (PrtSc) because Windows intercepts it.
// Use a low-level keyboard hook to capture it and post a custom message.
constexpr UINT kWmHotkeyCaptured = WM_APP + 100;
static HHOOK gKeyboardHook = nullptr;
static HWND gHotkeyDlgHwnd = nullptr;

static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wp, LPARAM lp) {
    if (nCode == HC_ACTION && (wp == WM_KEYDOWN || wp == WM_SYSKEYDOWN) && gHotkeyDlgHwnd) {
        auto* kb = (KBDLLHOOKSTRUCT*)lp;
        if (kb->vkCode == VK_SNAPSHOT) {
            PostMessageW(gHotkeyDlgHwnd, kWmHotkeyCaptured, kb->vkCode, 0);
            return 1; // consume the key
        }
    }
    return CallNextHookEx(gKeyboardHook, nCode, wp, lp);
}

constexpr int kHotkeyDlgDx = 340;
struct SetHotkeyWnd : WindowBase {
    ~SetHotkeyWnd() override;

    HWND hwndOwner = nullptr;
    // the dialog is made only of virtual controls: it turns every key into a
    // hotkey to capture, so nothing in it can hold the keyboard focus anyway
    VirtText* prompt = nullptr;
    // the current / captured combination, drawn as a key-cap
    VirtRichText* hotkeyDisplay = nullptr;
    VirtButton* btnSet = nullptr;
    VirtButton* btnRemove = nullptr;
    VirtButton* btnCancel = nullptr;

    Str currentHotkey;      // current hotkey string, or empty if none
    Str newHotkey;          // newly captured hotkey string
    bool committed = false; // true if Set or Remove was pressed

    bool Create(HWND owner);
    void WndProc(WindowBase::WndProcEvent* ev);
    void OnKeyDown(KeyEvent* ev);

    VirtButton* NewButton(Str text, DlgButtonStyle style);
    void UpdateUI();
    bool HandleKeyDown(UINT vk);
    void DoSet(VirtMouseEvent* ev = nullptr);
    void DoRemove(VirtMouseEvent* ev = nullptr);
    void OnCancel(VirtMouseEvent* ev = nullptr);
    static void CleanupHook();
};

static SetHotkeyWnd* gSetHotkeyWnd = nullptr;

SetHotkeyWnd::~SetHotkeyWnd() {
    str::Free(currentHotkey);
    str::Free(newHotkey);
}

void SetHotkeyWnd::UpdateUI() {
    Str display = StrL("None");
    if (newHotkey) {
        display = newHotkey;
    } else if (currentHotkey) {
        display = currentHotkey;
    }
    if (hotkeyDisplay) {
        // a real combination draws as a key-cap; "None" is just text
        bool isNone = str::Eq(display, StrL("None"));
        TempStr markup = isNone ? str::DupTemp(display) : str::JoinTemp(StrL("(Kbd/"), display, StrL(")"));
        hotkeyDisplay->Reset();
        ParseTipInto(hotkeyDisplay, markup);
    }
    if (btnSet) {
        SetDlgButtonEnabled(btnSet, DlgButtonStyle::Filled, len(newHotkey) > 0);
    }
    if (btnRemove) {
        SetDlgButtonEnabled(btnRemove, DlgButtonStyle::Ghost, len(currentHotkey) > 0 || len(newHotkey) > 0);
    }
    // the key-cap changes width with the combination, so lay out again
    DoLayout();
    HwndScheduleRepaint(hwnd);
}

bool SetHotkeyWnd::HandleKeyDown(UINT vk) {
    if (vk == VK_CONTROL || vk == VK_LCONTROL || vk == VK_RCONTROL || vk == VK_SHIFT || vk == VK_MENU ||
        vk == VK_LMENU || vk == VK_RMENU || vk == VK_LWIN || vk == VK_RWIN) {
        return true;
    }
    if (vk == VK_ESCAPE) {
        OnCancel();
        return true;
    }
    bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    bool alt = (GetKeyState(VK_MENU) & 0x8000) != 0;
    bool altGr = (GetKeyState(VK_RMENU) & 0x8000) != 0;
    // Return / arrows as a global hotkey without a modifier would steal
    // Enter and cursor keys from every app
    bool needsMod = (vk == VK_RETURN) || (vk == VK_LEFT) || (vk == VK_RIGHT) || (vk == VK_UP) || (vk == VK_DOWN);
    if (needsMod && !ctrl && !shift && !alt) {
        return true;
    }
    TempStr hotkey = SerializeHotkeyTemp(vk, ctrl, shift, alt, altGr);
    if (hotkey) {
        str::ReplaceWithCopy(&newHotkey, hotkey);
        UpdateUI();
    }
    return true;
}

void SetHotkeyWnd::DoSet(VirtMouseEvent*) {
    if (len(newHotkey) == 0) {
        return;
    }
    logf("SetHotkeyDoSet: setting screenshot hotkey to '%s'\n", newHotkey);

    TempStr globalKey = str::JoinTemp(StrL("Global "), newHotkey);
    Shortcut* sc = FindScreenshotShortcutEntry();
    if (sc) {
        str::ReplaceWithCopy(&sc->key, globalKey);
    } else {
        sc = new Shortcut();
        sc->cmd = str::Dup(StrL("CmdScreenshot"));
        sc->key = str::Dup(globalKey);
        sc->name = {};
        sc->toolbarText = {};
        sc->toolbarSvgIcon = {};
        sc->cmdId = 0;
        VecAppend(*gSettings->shortcuts, sc);
    }
    ScheduleSaveSettings();

    ReRegisterGlobalHotkeys();
    committed = true;
    Close();
}

void SetHotkeyWnd::DoRemove(VirtMouseEvent*) {
    logf("SetHotkeyDoRemove: removing screenshot hotkey\n");

    Shortcut* sc = FindScreenshotShortcutEntry();
    if (sc) {
        VecRemove(*gSettings->shortcuts, sc);
    }
    auto* curr = gFirstCustomCommand;
    while (curr) {
        if (curr->origId == CmdScreenshot) {
            str::ReplaceWithCopy(&curr->key, Str{});
        }
        curr = curr->next;
    }
    ScheduleSaveSettings();
    ReRegisterGlobalHotkeys();
    committed = true;
    Close();
}

void SetHotkeyWnd::OnCancel(VirtMouseEvent*) {
    Close();
}

void SetHotkeyWnd::CleanupHook() {
    if (gKeyboardHook) {
        UnhookWindowsHookEx(gKeyboardHook);
        gKeyboardHook = nullptr;
    }
    gHotkeyDlgHwnd = nullptr;
}

VirtButton* SetHotkeyWnd::NewButton(Str text, DlgButtonStyle style) {
    return NewDlgButton(hwnd, text, font, style, style == DlgButtonStyle::Filled);
}

void SetHotkeyWnd::WndProc(WindowBase::WndProcEvent* ev) {
    if (ev->msg == kWmHotkeyCaptured || ev->msg == WM_KEYDOWN || ev->msg == WM_SYSKEYDOWN) {
        HandleKeyDown((UINT)ev->wparam);
        ev->result = 0;
        ev->didHandle = true;
        return;
    }
}

void SetHotkeyWnd::OnKeyDown(KeyEvent* ev) {
    if (!hwnd) {
        return;
    }
    if (ev->hwnd != hwnd && !IsChild(hwnd, ev->hwnd)) {
        return;
    }
    ev->didHandle = HandleKeyDown((UINT)ev->vkey);
}

static void TeardownSetHotkeyWnd() {
    if (!gSetHotkeyWnd) {
        return;
    }
    SetHotkeyWnd* w = gSetHotkeyWnd;
    gSetHotkeyWnd = nullptr;
    SetHotkeyWnd::CleanupHook();
    if (!w->committed) {
        ReRegisterGlobalHotkeys();
    }
    w->ScheduleDelete();
}

static void OnSetHotkeyClose(WindowBase::CloseEvent* ev) {
    if (gSetHotkeyWnd == (SetHotkeyWnd*)ev->e->self) {
        TeardownSetHotkeyWnd();
    }
}

static void OnSetHotkeyDestroy(WindowBase::DestroyEvent* ev) {
    if (gSetHotkeyWnd == (SetHotkeyWnd*)ev->e->self) {
        TeardownSetHotkeyWnd();
    }
}

bool SetHotkeyWnd::Create(HWND owner) {
    hwndOwner = owner;
    Str existing = FindScreenshotShortcut();
    if (existing) {
        TrimGlobalPrefix(existing);
        currentHotkey = str::Dup(existing);
    }

    //   Press a key combination:
    //   [Ctrl] + [Shift] + [S]
    //   ---------------------------------------------- hairline
    //   Remove                           [Cancel] [Set]
    bool isRtl = IsUIRtl();
    Color colTxt = ThemeWindowTextColor();
    Color colBg = ThemeWindowControlBackgroundColor();
    PlatformFont* fontText = DlgUiFont(kToolDlgTextFontPx);
    SetFont(fontText);
    {
        CreateCustomArgs args;
        args.owner = owner;
        args.title = Tr("Set Screenshot Hotkey");
        args.visible = false;
        args.style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU;
        args.exStyle = WS_EX_DLGMODALFRAME;
        args.font = fontText;
        args.isRtl = isRtl;
        args.bgColor = colBg;
        args.icon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(GetAppIconID()));
        CreateCustom(args);
    }
    if (!hwnd) {
        return false;
    }
    SetColors(colTxt, colBg);

    auto* vbox = new VBox();
    vbox->alignCross = CrossAxisAlign::Stretch;
    prompt = NewDlgCaption(Tr("Press a key combination:"), fontText, ThemeWindowTextDisabledColor());
    vbox->AddChild(prompt);
    {
        // the combination, a key-cap a key, on a line of its own
        auto* t = new VirtRichText();
        t->font = fontText;
        hotkeyDisplay = t;
        HBox* row = NewDlgRow();
        row->minDy = DpiScale(kToolDlgLineDy);
        row->AddChild(t, 1);
        vbox->AddChild(new Padding(row, DpiScaledInsets(0, kToolDlgPadDx)));
    }

    btnCancel = NewButton(Tr("Cancel"), DlgButtonStyle::Outline);
    btnCancel->onClick = MkMethod1<SetHotkeyWnd, VirtMouseEvent*, &SetHotkeyWnd::OnCancel>(this);
    btnRemove = NewButton(Tr("Remove"), DlgButtonStyle::Ghost);
    btnRemove->onClick = MkMethod1<SetHotkeyWnd, VirtMouseEvent*, &SetHotkeyWnd::DoRemove>(this);
    btnSet = NewButton(Tr("Set"), DlgButtonStyle::Filled);
    btnSet->onClick = MkMethod1<SetHotkeyWnd, VirtMouseEvent*, &SetHotkeyWnd::DoSet>(this);
    vbox->AddChild(NewDlgFooter(btnCancel, btnSet, btnRemove));
    layout = vbox;

    // fill in the key-cap and the button states before measuring: an empty
    // rich text has no height, and the window is sized to its content
    UpdateUI();
    int minDx = DpiScale(kHotkeyDlgDx);
    LayoutAndSizeToContent(layout, minDx, 0, hwnd);
    // pick up the virtual controls so we paint them and they get their input
    DoLayout(HwndClientRect(hwnd).Size());
    HwndCenterDialog(hwnd, owner);
    UpdateTheme();

    SetIsVisible(true);
    HwndSetFocus(hwnd);
    return true;
}

void ShowSetScreenshotHotkeyDialog(HWND hwndOwner) {
    if (gSetHotkeyWnd) {
        if (gSetHotkeyWnd->hwnd && IsWindow(gSetHotkeyWnd->hwnd)) {
            HwndSetFocus(gSetHotkeyWnd->hwnd);
            return;
        }
        TeardownSetHotkeyWnd();
    }

    UnregisterGlobalHotkeys(GetGlobalHotkeysHwnd());

    auto* wnd = new SetHotkeyWnd();
    wnd->hwndOwner = hwndOwner;
    wnd->onClose = MkFunc1Void<WindowBase::CloseEvent*>(OnSetHotkeyClose);
    wnd->onDestroy = MkFunc1Void<WindowBase::DestroyEvent*>(OnSetHotkeyDestroy);
    wnd->onWndProc = MkMethod1<SetHotkeyWnd, WindowBase::WndProcEvent*, &SetHotkeyWnd::WndProc>(wnd);
    wnd->onKeyDown = MkMethod1<SetHotkeyWnd, KeyEvent*, &SetHotkeyWnd::OnKeyDown>(wnd);
    wnd->SetFont(GetAppFont());
    if (!wnd->Create(hwndOwner)) {
        delete wnd;
        ReRegisterGlobalHotkeys();
        return;
    }

    gSetHotkeyWnd = wnd;
    gHotkeyDlgHwnd = wnd->hwnd;
    HINSTANCE h = GetModuleHandleW(nullptr);
    gKeyboardHook = SetWindowsHookExW(WH_KEYBOARD_LL, LowLevelKeyboardProc, h, 0);
    RunModalWindow(wnd->hwnd, hwndOwner);
}
