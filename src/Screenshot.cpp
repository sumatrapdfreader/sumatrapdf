/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// SumatraPDF's half of the screenshot feature: the global hotkey, the dialog
// that sets it, and the hooks the shared capture code calls back into.
// Capturing and the picker overlay live in ScreenshotCapture.cpp.

#include "base/Base.h"
#include "base/ScopedWin.h"
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
#include "Screenshot.h"
#include "Theme.h"
#include "SumatraConfig.h"
#include "DarkMode_win.h"
#include "Commands.h"
#include "Accelerators.h"
#include "Settings.h"
#include "GlobalPrefs.h"
#include "AppSettings.h"
#include "MainWindow.h"
#include "SumatraPDF.h"
#include "Translations.h"

static bool IsAppFrame(HWND hwnd) {
    for (MainWindow* win : gWindows) {
        if (win->hwndFrame == hwnd) {
            return true;
        }
    }
    return false;
}

static TempStr GetScreenshotSaveDirTemp() {
    TempStr dataDir = GetAppDataDirTemp();
    return path::JoinTemp(dataDir, StrL("Screenshots"));
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

static bool IsOtherSumatraProcessRunning() {
    DWORD myPid = GetCurrentProcessId();
    HWND hwnd = nullptr;
    while ((hwnd = FindWindowEx(HWND_DESKTOP, hwnd, L"SUMATRA_PDF_FRAME", nullptr)) != nullptr) {
        DWORD pid;
        GetWindowThreadProcessId(hwnd, &pid);
        if (pid != myPid) {
            return true;
        }
    }
    return false;
}

// find custom shortcut key string for CmdScreenshot, or empty if none
static Str FindScreenshotShortcut() {
    // check gGlobalPrefs->shortcuts first (may have been updated at runtime)
    for (Shortcut* sc : *gGlobalPrefs->shortcuts) {
        if (str::EqI(sc->cmd, StrL("CmdScreenshot")) && sc->key) {
            return sc->key;
        }
    }
    // fall back to custom commands (built at startup)
    auto* curr = gFirstCustomCommand;
    while (curr) {
        if (curr->origId == CmdScreenshot && curr->key) {
            return curr->key;
        }
        curr = curr->next;
    }
    return {};
}

static UINT AccelFVirtToHotkeyMod(BYTE fVirt) {
    UINT mod = 0;
    if (fVirt & FALT) {
        mod |= MOD_ALT;
    }
    if (fVirt & FCONTROL) {
        mod |= MOD_CONTROL;
    }
    if (fVirt & FSHIFT) {
        mod |= MOD_SHIFT;
    }
    return mod;
}

void RegisterScreenshotHotkey(HWND hwnd) {
    Str shortcut = FindScreenshotShortcut();
    if (!shortcut) {
        // don't register global hotkey by default; require explicit
        // Shortcuts entry (e.g. Key = PrtSc, CmdScreenshot)
        return;
    }
    UINT mod = 0;
    UINT vk = VK_SNAPSHOT;
    ACCEL accel{};
    if (ParseShortcutString(shortcut, accel)) {
        mod = AccelFVirtToHotkeyMod(accel.fVirt);
        vk = accel.key;
    }
    BOOL ok = RegisterHotKey(hwnd, kScreenshotHotkeyId, mod, vk);
    if (!ok && !IsOtherSumatraProcessRunning()) {
        MaybeDelayedWarningNotification(fmt("Couldn't register '%s' global hotkey for taking screenshots", shortcut));
    }
}

void UnregisterScreenshotHotkey(HWND hwnd) {
    UnregisterHotKey(hwnd, kScreenshotHotkeyId);
}

// --- Set Screenshot Hotkey dialog ---

// serialize VK code + modifiers to a shortcut string like "Ctrl+Shift+F5"
static TempStr SerializeHotkeyTemp(UINT vk, bool ctrl, bool shift, bool alt) {
    str::Builder s;
    if (ctrl) {
        s.Append("Ctrl+");
    }
    if (alt) {
        s.Append("Alt+");
    }
    if (shift) {
        s.Append("Shift+");
    }
    bool isAlphaNumKey = (vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9');
    if (vk >= VK_F1 && vk <= VK_F24) {
        s.Append(fmt("F%d", (int)(vk - VK_F1 + 1)));
    } else if (isAlphaNumKey) {
        s.AppendChar((char)vk);
    } else if (vk == VK_SNAPSHOT) {
        s.Append("PrtSc");
    } else if (vk == VK_DELETE) {
        s.Append("Delete");
    } else if (vk == VK_INSERT) {
        s.Append("Insert");
    } else if (vk == VK_HOME) {
        s.Append("Home");
    } else if (vk == VK_END) {
        s.Append("End");
    } else if (vk == VK_PRIOR) {
        s.Append("PageUp");
    } else if (vk == VK_NEXT) {
        s.Append("PageDown");
    } else if (vk == VK_SPACE) {
        s.Append("Space");
    } else if (vk == VK_PAUSE) {
        s.Append("Pause");
    } else if (vk == VK_SCROLL) {
        s.Append("ScrollLock");
    } else if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) {
        s.Append(fmt("Numpad%d", (int)(vk - VK_NUMPAD0)));
    } else {
        // unknown key
        return {};
    }
    return ToStrTemp(s);
}

// find existing Shortcut entry for CmdScreenshot, or nullptr
static Shortcut* FindScreenshotShortcutEntry() {
    for (Shortcut* sc : *gGlobalPrefs->shortcuts) {
        if (str::EqI(sc->cmd, StrL("CmdScreenshot"))) {
            return sc;
        }
    }
    return nullptr;
}

// WM_KEYDOWN doesn't fire for VK_SNAPSHOT (PrtSc) because Windows intercepts it.
// Use a low-level keyboard hook to capture it and post a custom message.
constexpr UINT WM_HOTKEY_CAPTURED = WM_APP + 100;
static HHOOK gKeyboardHook = nullptr;
static HWND gHotkeyDlgHwnd = nullptr;

static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wp, LPARAM lp) {
    if (nCode == HC_ACTION && (wp == WM_KEYDOWN || wp == WM_SYSKEYDOWN) && gHotkeyDlgHwnd) {
        auto* kb = (KBDLLHOOKSTRUCT*)lp;
        if (kb->vkCode == VK_SNAPSHOT) {
            PostMessageW(gHotkeyDlgHwnd, WM_HOTKEY_CAPTURED, kb->vkCode, 0);
            return 1; // consume the key
        }
    }
    return CallNextHookEx(gKeyboardHook, nCode, wp, lp);
}

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

    VirtButton* NewButton(Str text, bool isDefault);
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
        btnSet->SetFlag(vwfEnabled, len(newHotkey) > 0);
    }
    if (btnRemove) {
        btnRemove->SetFlag(vwfEnabled, len(currentHotkey) > 0 || len(newHotkey) > 0);
    }
    // the key-cap changes width with the combination, so lay out again
    DoLayout();
    HwndScheduleRepaint(hwnd);
}

bool SetHotkeyWnd::HandleKeyDown(UINT vk) {
    if (vk == VK_CONTROL || vk == VK_SHIFT || vk == VK_MENU || vk == VK_LWIN || vk == VK_RWIN) {
        return true;
    }
    if (vk == VK_ESCAPE) {
        OnCancel();
        return true;
    }
    bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    bool alt = (GetKeyState(VK_MENU) & 0x8000) != 0;
    TempStr hotkey = SerializeHotkeyTemp(vk, ctrl, shift, alt);
    if (hotkey) {
        str::ReplaceWithCopy(&newHotkey, hotkey);
        UpdateUI();
    }
    return true;
}

void SetHotkeyWnd::DoSet(VirtMouseEvent*) {
    if (!newHotkey) {
        return;
    }
    logf("SetHotkeyDoSet: setting screenshot hotkey to '%s'\n", newHotkey);

    Shortcut* sc = FindScreenshotShortcutEntry();
    if (sc) {
        str::ReplaceWithCopy(&sc->key, newHotkey);
    } else {
        sc = new Shortcut();
        sc->cmd = str::Dup(StrL("CmdScreenshot"));
        sc->key = str::Dup(newHotkey);
        sc->name = nullptr;
        sc->toolbarText = nullptr;
        sc->toolbarSvgIcon = nullptr;
        sc->cmdId = 0;
        gGlobalPrefs->shortcuts->Append(sc);
    }
    SaveSettings();

    for (MainWindow* win : gWindows) {
        RegisterScreenshotHotkey(win->hwndFrame);
    }
    committed = true;
    Close();
}

void SetHotkeyWnd::DoRemove(VirtMouseEvent*) {
    logf("SetHotkeyDoRemove: removing screenshot hotkey\n");

    Shortcut* sc = FindScreenshotShortcutEntry();
    if (sc) {
        gGlobalPrefs->shortcuts->Remove(sc);
    }
    auto* curr = gFirstCustomCommand;
    while (curr) {
        if (curr->origId == CmdScreenshot) {
            str::ReplaceWithCopy(&curr->key, Str{});
        }
        curr = curr->next;
    }
    SaveSettings();
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

VirtButton* SetHotkeyWnd::NewButton(Str text, bool isDefault) {
    return NewThemedButton(hwnd, text, font, isDefault);
}

void SetHotkeyWnd::WndProc(WindowBase::WndProcEvent* ev) {
    if (ev->msg == WM_HOTKEY_CAPTURED || ev->msg == WM_KEYDOWN || ev->msg == WM_SYSKEYDOWN) {
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
        for (MainWindow* win : gWindows) {
            RegisterScreenshotHotkey(win->hwndFrame);
        }
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
        currentHotkey = str::Dup(existing);
    }

    bool isRtl = IsUIRtl();
    {
        CreateCustomArgs args;
        args.title = _TRA("Set Screenshot Hotkey");
        args.visible = false;
        args.style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU;
        args.exStyle = WS_EX_DLGMODALFRAME;
        args.font = GetFont();
        args.icon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(GetAppIconID()));
        CreateCustom(args);
    }
    if (!hwnd) {
        return false;
    }

    auto* vbox = new VBox();
    vbox->alignMain = MainAxisAlign::MainStart;
    vbox->alignCross = CrossAxisAlign::Stretch;

    prompt = NewVirtText({
        .s = _TRA("Press a key combination:"),
        .font = font,
        .isRtl = isRtl,
    });
    vbox->AddChild(prompt);

    {
        auto* t = new VirtRichText();
        t->font = font;
        t->padding = DpiScaledInsets(6, 0, 0, 0);
        hotkeyDisplay = t;
        vbox->AddChild(t);
    }

    {
        auto* hbox = new HBox();
        hbox->alignMain = MainAxisAlign::MainEnd;
        hbox->alignCross = CrossAxisAlign::CrossCenter;
        hbox->gap = font->averageCharWidth;

        btnCancel = NewButton(_TRA("Cancel"), false);
        btnCancel->onClick = MkMethod1<SetHotkeyWnd, VirtMouseEvent*, &SetHotkeyWnd::OnCancel>(this);
        hbox->AddChild(btnCancel);
        btnRemove = NewButton(_TRA("Remove"), false);
        btnRemove->onClick = MkMethod1<SetHotkeyWnd, VirtMouseEvent*, &SetHotkeyWnd::DoRemove>(this);
        hbox->AddChild(btnRemove);
        btnSet = NewButton(_TRA("Set"), true);
        btnSet->onClick = MkMethod1<SetHotkeyWnd, VirtMouseEvent*, &SetHotkeyWnd::DoSet>(this);
        hbox->AddChild(btnSet);

        auto* hboxPad = new Padding(hbox, DpiScaledInsets(8, 0, 0, 0));
        vbox->AddChild(hboxPad);
    }

    layout = new Padding(vbox, DpiScaledInsets(8, 12));

    // fill in the key-cap and the button states before measuring: an empty
    // rich text has no height, and the window is sized to its content
    UpdateUI();
    int minDx = DpiScale(320);
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

    for (MainWindow* win : gWindows) {
        UnregisterScreenshotHotkey(win->hwndFrame);
    }

    auto* wnd = new SetHotkeyWnd();
    wnd->hwndOwner = hwndOwner;
    wnd->onClose = MkFunc1Void<WindowBase::CloseEvent*>(OnSetHotkeyClose);
    wnd->onDestroy = MkFunc1Void<WindowBase::DestroyEvent*>(OnSetHotkeyDestroy);
    wnd->onWndProc = MkMethod1<SetHotkeyWnd, WindowBase::WndProcEvent*, &SetHotkeyWnd::WndProc>(wnd);
    wnd->onKeyDown = MkMethod1<SetHotkeyWnd, KeyEvent*, &SetHotkeyWnd::OnKeyDown>(wnd);
    wnd->SetFont(GetAppFont());
    if (!wnd->Create(hwndOwner)) {
        delete wnd;
        for (MainWindow* win : gWindows) {
            RegisterScreenshotHotkey(win->hwndFrame);
        }
        return;
    }

    gSetHotkeyWnd = wnd;
    gHotkeyDlgHwnd = wnd->hwnd;
    HINSTANCE h = GetModuleHandleW(nullptr);
    gKeyboardHook = SetWindowsHookExW(WH_KEYBOARD_LL, LowLevelKeyboardProc, h, 0);
}
