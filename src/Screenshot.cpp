/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// SumatraPDF's half of the screenshot feature: the global hotkey, the dialog
// that sets it, and the hooks the shared capture code calls back into.
// Capturing and the picker overlay live in ScreenshotCapture.cpp.

#include "base/Base.h"
#include "base/ScopedWin.h"
#include "base/File.h"
#include "base/Win.h"
#include "base/Dpi.h"
#include "base/UITask.h"

#include "wingui/UIModels.h"
#include "wingui/Layout.h"
#include "wingui/WinGui.h"

#include "Notifications.h"
#include "AppTools.h"
#include "ScreenshotCapture.h"
#include "ShortcutParse.h"
#include "Screenshot.h"
#include "Theme.h"
#include "SumatraConfig.h"
#include "DarkModeSubclass.h"
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

    HFONT font = nullptr;
    HWND hwndOwner = nullptr;
    Static* staticPrompt = nullptr;
    Edit* editHotkey = nullptr;
    Button* btnSet = nullptr;
    Button* btnRemove = nullptr;
    Button* btnCancel = nullptr;

    Str currentHotkey;      // current hotkey string, or empty if none
    Str newHotkey;          // newly captured hotkey string
    bool committed = false; // true if Set or Remove was pressed

    bool Create(HWND owner);
    LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) override;
    bool PreTranslateMessage(MSG& msg) override;

    void UpdateTheme();
    void UpdateUI();
    bool HandleKeyDown(UINT vk);
    void DoSet();
    void DoRemove();
    void OnCancel();
    static void CleanupHook();
    void ScheduleDelete();
};

static SetHotkeyWnd* gSetHotkeyWnd = nullptr;

SetHotkeyWnd::~SetHotkeyWnd() {
    str::Free(currentHotkey);
    str::Free(newHotkey);
}

static void DeleteSetHotkeyWndInstance(SetHotkeyWnd* w) {
    delete w;
}

void SetHotkeyWnd::ScheduleDelete() {
    auto fn = MkFunc0<SetHotkeyWnd>(DeleteSetHotkeyWndInstance, this);
    uitask::Post(fn, "SafeDeleteSetHotkeyWnd");
}

void SetHotkeyWnd::UpdateUI() {
    Str display = StrL("None");
    if (newHotkey) {
        display = newHotkey;
    } else if (currentHotkey) {
        display = currentHotkey;
    }
    if (editHotkey) {
        editHotkey->SetText(display);
    }
    if (btnSet) {
        btnSet->SetIsEnabled(len(newHotkey) > 0);
    }
    if (btnRemove) {
        btnRemove->SetIsEnabled(len(currentHotkey) > 0 || len(newHotkey) > 0);
    }
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

void SetHotkeyWnd::DoSet() {
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

void SetHotkeyWnd::DoRemove() {
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

void SetHotkeyWnd::OnCancel() {
    Close();
}

void SetHotkeyWnd::CleanupHook() {
    if (gKeyboardHook) {
        UnhookWindowsHookEx(gKeyboardHook);
        gKeyboardHook = nullptr;
    }
    gHotkeyDlgHwnd = nullptr;
}

void SetHotkeyWnd::UpdateTheme() {
    COLORREF colBg = ThemeWindowControlBackgroundColor();
    COLORREF colTxt = ThemeWindowTextColor();
    SetColors(colTxt, colBg);
    if (staticPrompt) {
        staticPrompt->SetColors(colTxt, colBg);
    }
    if (editHotkey) {
        editHotkey->SetColors(colTxt, colBg);
    }
    if (btnSet) {
        btnSet->SetColors(colTxt, colBg);
    }
    if (btnRemove) {
        btnRemove->SetColors(colTxt, colBg);
    }
    if (btnCancel) {
        btnCancel->SetColors(colTxt, colBg);
    }
    if (UseDarkModeLib()) {
        DarkMode::setDarkWndSafe(hwnd);
        DarkMode::setWindowEraseBgSubclass(hwnd);
    }
    RedrawWindow(hwnd, nullptr, nullptr, RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN);
}

LRESULT SetHotkeyWnd::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_HOTKEY_CAPTURED || msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN) {
        HandleKeyDown((UINT)wp);
        return 0;
    }
    return WndProcDefault(hwnd, msg, wp, lp);
}

bool SetHotkeyWnd::PreTranslateMessage(MSG& msg) {
    if (!hwnd) {
        return false;
    }
    if (msg.hwnd != hwnd && !IsChild(hwnd, msg.hwnd)) {
        return false;
    }
    if (msg.message == WM_KEYDOWN || msg.message == WM_SYSKEYDOWN) {
        return HandleKeyDown((UINT)msg.wParam);
    }
    return false;
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
        args.font = font;
        args.icon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(GetAppIconID()));
        CreateCustom(args);
    }
    if (!hwnd) {
        return false;
    }

    Str display = currentHotkey ? currentHotkey : StrL("None");

    auto* vbox = new VBox();
    vbox->alignMain = MainAxisAlign::MainStart;
    vbox->alignCross = CrossAxisAlign::Stretch;

    {
        Static::CreateArgs args;
        args.parent = hwnd;
        args.font = font;
        args.text = _TRA("Press a key combination:");
        args.isRtl = isRtl;
        auto* c = new Static();
        c->Create(args);
        staticPrompt = c;
        vbox->AddChild(c);
    }

    {
        Edit::CreateArgs args;
        args.parent = hwnd;
        args.font = font;
        args.text = display;
        args.withBorder = true;
        args.isRtl = isRtl;
        auto* c = new Edit();
        c->Create(args);
        SendMessageW(c->hwnd, EM_SETREADONLY, TRUE, 0);
        editHotkey = c;
        c->SetInsetsPt(6, 0, 0, 0);
        vbox->AddChild(c);
    }

    {
        auto* hbox = new HBox();
        hbox->alignMain = MainAxisAlign::MainEnd;
        hbox->alignCross = CrossAxisAlign::CrossCenter;

        btnCancel = CreateButton(hwnd, _TRA("Cancel"), MkMethod0<SetHotkeyWnd, &SetHotkeyWnd::OnCancel>(this), isRtl);
        hbox->AddChild(btnCancel);
        btnRemove = CreateButton(hwnd, _TRA("Remove"), MkMethod0<SetHotkeyWnd, &SetHotkeyWnd::DoRemove>(this), isRtl);
        btnRemove->SetInsetsPt(0, 0, 0, 4);
        hbox->AddChild(btnRemove);
        {
            auto* btn = new Button();
            btn->isDefault = true;
            btn->onClick = MkMethod0<SetHotkeyWnd, &SetHotkeyWnd::DoSet>(this);
            Button::CreateArgs args;
            args.parent = hwnd;
            args.font = font;
            args.text = _TRA("Set");
            args.isRtl = isRtl;
            btn->Create(args);
            btn->SetInsetsPt(0, 0, 0, 4);
            btnSet = btn;
            hbox->AddChild(btnSet);
        }
        auto* hboxPad = new Padding(hbox, DpiScaledInsets(hwnd, 8, 0, 0, 0));
        vbox->AddChild(hboxPad);
    }

    layout = new Padding(vbox, DpiScaledInsets(hwnd, 8, 12));

    int minDx = DpiScale(hwnd, 320);
    LayoutAndSizeToContent(layout, minDx, 0, hwnd);
    HwndCenterDialog(hwnd, owner);
    UpdateTheme();
    UpdateUI();

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
    wnd->font = GetAppFont(hwndOwner);
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
