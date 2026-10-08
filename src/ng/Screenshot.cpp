/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// SumatraPDF's half of the screenshot feature: where the files go and the
// dialog that sets the hotkey. ng: orig's capture (ScreenshotCapture.cpp) walks
// the desktop's top-level windows, blits them and shows a layered picker
// overlay. All UI here is gpui, so what CmdScreenshot does instead is render
// the rectangular selection - or the current page - out of the engine, which
// is what the picker was mostly used for and works on every platform.

#include "gui/GpuiBridge.h"
#include "base/File.h"
#include "base/Pixmap.h"

#include "gui/UIModels.h"

#include "Settings.h"
#include "AppSettings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "base/GuessFileType.h"
#include "EngineAll.h"
#include "DisplayModel.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "Selection.h"
#include "Notifications.h"
#include "AppTools.h"
#include "ShortcutParse.h"
#include "Theme.h"
#include "SumatraConfig.h"
#include "Commands.h"
#include "Accelerators.h"
#include "SumatraPDF.h"
#include "Translations.h"
#include "VirtKeys.h"
#include "gui/AppShell.h"
#include "gui/DialogWidgets.h"
#include "SumatraDialogs.h"
#include "ImageSaveCropResize.h"
#if OS_WASM
#include "gui/WasmBridge.h"
#endif
#include "Screenshot.h"

#include "SumatraLog.h"

static Kind kNotifScreenshot = "notifScreenshot";

TempStr GetScreenshotSaveDirTemp() {
    TempStr dataDir = GetAppDataDirTemp();
    return path::JoinTemp(dataDir, StrL("Screenshots"));
}

// --- rendering a page region (orig's PdfTools RenderSelectionPixmap) --------

constexpr float kScreenshotDpi = 150;
constexpr i64 kMaxScreenshotPixels = 80 * 1000 * 1000;

static Pixmap* RenderSelectionPixmap(EngineBase* engine, int rotation, int pageNo, RectF rect, float dpi) {
    if (!engine || rect.IsEmpty()) {
        return nullptr;
    }
    if (pageNo < 1 || pageNo > engine->PageCount()) {
        return nullptr;
    }
    float zoom = dpi / 72.f;
    i64 estW = (i64)(rect.dx * zoom);
    i64 estH = (i64)(rect.dy * zoom);
    if (estW < 1 || estH < 1 || estW * estH > kMaxScreenshotPixels) {
        logf("RenderSelectionPixmap: %lld x %lld at %.0f DPI is too large\n", estW, estH, dpi);
        return nullptr;
    }
    RenderPageArgs args(pageNo, zoom, rotation, &rect, RenderTarget::Export);
    Pixmap* px = engine->RenderPage(args);
    if (!px) {
        logf("RenderSelectionPixmap: RenderPage failed page %d\n", pageNo);
        return nullptr;
    }
    px->xres = dpi;
    px->yres = dpi;
    if (px->format != PixmapFormat::Native) {
        return px;
    }
#if OS_WIN
    Pixmap* converted = PixmapCopyAs32bppDIB(px);
    FreePixmap(px);
    return converted;
#else
    FreePixmap(px);
    return nullptr;
#endif
}

// the rectangular selection, or the whole current page when there is none
static Pixmap* RenderScreenshotPixmap(MainWindow* win, TempStr* whatOut) {
    DisplayModel* dm = win ? win->AsFixed() : nullptr;
    EngineBase* engine = dm ? dm->GetEngine() : nullptr;
    if (!engine) {
        return nullptr;
    }
    WindowTab* tab = win->CurrentTab();
    int pageNo = dm->CurrentPageNo();
    RectF rect;
    if (tab && tab->selectionOnPage) {
        for (auto& sel : *tab->selectionOnPage) {
            if (sel.rect.IsEmpty()) {
                continue;
            }
            if (rect.IsEmpty()) {
                pageNo = sel.pageNo;
                rect = sel.rect;
            } else if (sel.pageNo == pageNo) {
                rect = rect.Union(sel.rect);
            }
        }
    }
    if (rect.IsEmpty()) {
        rect = engine->PageMediabox(pageNo);
        if (whatOut) {
            *whatOut = fmt("page %d", pageNo);
        }
    } else if (whatOut) {
        *whatOut = fmt("selection on page %d", pageNo);
    }
    return RenderSelectionPixmap(engine, dm->GetRotation(), pageNo, rect, kScreenshotDpi);
}

bool CopySelectionAsImage(MainWindow* win) {
    if (!HasPermission(Perm::CopySelection)) {
        return false;
    }
    Pixmap* px = RenderScreenshotPixmap(win, nullptr);
    if (!px) {
        return false;
    }
    bool ok = ImageEditCopyToClipboard(px);
    FreePixmap(px);
    logf("CopySelectionAsImage: %s\n", Str(ok ? "ok" : "failed"));
    return ok;
}

void TakeScreenshots(MainWindow* win) {
    if (!IsMainWindowValidAndNotClosing(win)) {
        return;
    }
    // wasm writes a temp PNG and hands it to the browser. The disk permission
    // is about the user's files, which a tab does not have.
#if !OS_WASM
    if (!CanAccessDisk()) {
        return;
    }
#endif
    InitImageEditHost();
    TempStr what;
    Pixmap* px = RenderScreenshotPixmap(win, &what);
    if (!px) {
        ShowWarningNotification(win, Tr("Nothing to take a screenshot of"), kNotif5SecsTimeOut);
        return;
    }
#if OS_WASM
    TempStr dir = GetTempDirPathTemp();
#else
    TempStr dir = GetScreenshotSaveDirTemp();
#endif
    dir::CreateAll(dir);
    TempStr base = path::JoinTemp(dir, StrL("screenshot.png"));
    TempStr destPath = MakeUniqueFilePathTemp(base);
    bool ok = gImageEditHost.SavePixmapAsImage && gImageEditHost.SavePixmapAsImage(px, destPath, StrL(".png"));
    if (ok) {
#if OS_WASM
        // MEMFS is not a folder the user can open. Download the PNG, and copy
        // it when the browser allows an image on the clipboard.
        ok = WasmDownloadFile(destPath);
        if (ok) {
            WasmCopyImageFile(destPath);
        }
        file::Delete(destPath);
#else
        ImageEditCopyToClipboard(px);
#endif
    }
    FreePixmap(px);
    logf("TakeScreenshots: %s -> '%s' %s\n", what, destPath, Str(ok ? "ok" : "FAILED"));

    NotificationCreateArgs args;
    args.win = win;
    args.groupId = kNotifScreenshot;
    args.timeoutMs = kNotif5SecsTimeOut;
    args.warning = !ok;
#if OS_WASM
    TempStr shown = path::GetBaseNameTemp(destPath);
#else
    TempStr shown = destPath;
#endif
    args.msg = ok ? fmt(Tr("Saved screenshot to '%s'").s, shown) : Tr("Failed to save the screenshot");
    ShowNotification(args);
}

// --- Set Screenshot Hotkey dialog -------------------------------------------

// serialize VK code + modifiers to a shortcut string like "Ctrl+Shift+F5"
static TempStr SerializeHotkeyTemp(uint vk, bool ctrl, bool shift, bool alt) {
    str::Builder s;
    if (ctrl) {
        s.Append(StrL("Ctrl+"));
    }
    if (alt) {
        s.Append(StrL("Alt+"));
    }
    if (shift) {
        s.Append(StrL("Shift+"));
    }
    bool isAlphaNumKey = (vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9');
    if (vk >= VK_F1 && vk <= VK_F24) {
        s.Append(fmt("F%d", (int)(vk - VK_F1 + 1)));
    } else if (isAlphaNumKey) {
        s.AppendChar((char)vk);
    } else if (vk == VK_SNAPSHOT) {
        s.Append(StrL("PrtSc"));
    } else if (vk == VK_RETURN) {
        s.Append(StrL("Return"));
    } else if (vk == VK_LEFT) {
        s.Append(StrL("Left"));
    } else if (vk == VK_RIGHT) {
        s.Append(StrL("Right"));
    } else if (vk == VK_UP) {
        s.Append(StrL("Up"));
    } else if (vk == VK_DOWN) {
        s.Append(StrL("Down"));
    } else if (vk == VK_DELETE) {
        s.Append(StrL("Delete"));
    } else if (vk == VK_INSERT) {
        s.Append(StrL("Insert"));
    } else if (vk == VK_HOME) {
        s.Append(StrL("Home"));
    } else if (vk == VK_END) {
        s.Append(StrL("End"));
    } else if (vk == VK_PRIOR) {
        s.Append(StrL("PageUp"));
    } else if (vk == VK_NEXT) {
        s.Append(StrL("PageDown"));
    } else if (vk == VK_SPACE) {
        s.Append(StrL("Space"));
    } else if (vk == VK_PAUSE) {
        s.Append(StrL("Pause"));
    } else if (vk == VK_SCROLL) {
        s.Append(StrL("ScrollLock"));
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
    for (Shortcut* sc : *gSettings->shortcuts) {
        if (str::EqI(sc->cmd, StrL("CmdScreenshot"))) {
            return sc;
        }
    }
    return nullptr;
}

// find custom shortcut key string for CmdScreenshot, or empty if none
static Str FindScreenshotShortcut() {
    // check gSettings->shortcuts first (may have been updated at runtime)
    for (Shortcut* sc : *gSettings->shortcuts) {
        if (str::EqI(sc->cmd, StrL("CmdScreenshot")) && len(sc->key) > 0) {
            return sc->key;
        }
    }
    auto* curr = gFirstCustomCommand;
    while (curr) {
        if (curr->origId == CmdScreenshot && len(curr->key) > 0) {
            return curr->key;
        }
        curr = curr->next;
    }
    return {};
}

struct SetHotkeyDlg {
    MainWindow* win = nullptr;
    bool visible = false;
    Str currentHotkey; // owned; current hotkey string, or empty if none
    Str newHotkey;     // owned; newly captured hotkey string
};

static SetHotkeyDlg gHotkey;

struct SetHotkeyView {
    static void OnSet(SetHotkeyView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnRemove(SetHotkeyView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnCancel(SetHotkeyView* self, gp::Ctx* cx, const gp::ClickEvent*);
};

static gp::Entity<SetHotkeyView> gHotkeyView;

// orig's modal window, where the platform can have one (DlgWindowOpen); null: a
// dialog in the frame
static ToolWindow* gHotkeyTw = nullptr;

// orig's window at 96 dpi: at least 320 wide, 8 / 12 around; the prompt, the
// combination 6 under it in an 18 high row (a key cap, or "None"), the three
// buttons 8 under that
constexpr float kHotkeyWinDx = 320;
constexpr float kHotkeyWinPadX = 12;
constexpr float kHotkeyWinPadY = 8;
constexpr float kHotkeyWinCapGap = 6;
constexpr float kHotkeyWinCapDy = 18;
constexpr float kHotkeyWinCapPadDx = 7;

// the dialog in the frame; a window of its own is not the frame's business
bool IsSetScreenshotHotkeyDialogVisible() {
    return gHotkey.visible && !gHotkeyTw;
}

void CloseSetScreenshotHotkeyDialog() {
    if (!gHotkey.visible) {
        return;
    }
    gHotkey.visible = false;
    DlgWindowClose(&gHotkeyTw);
    str::ReplaceWithCopy(&gHotkey.currentHotkey, {});
    str::ReplaceWithCopy(&gHotkey.newHotkey, {});
    AppShellInvalidate(gHotkey.win);
}

static Str SetHotkeyDlgTitle() {
    return Tr("Set Screenshot Hotkey");
}

// ng: Windows sends no key-down for PrtSc, only the key-up
static void SetHotkeyOnKeyUp(MainWindow* win, int vk, bool ctrl, bool shift, bool alt) {
    if (vk == VK_SNAPSHOT) {
        SetScreenshotHotkeyOnKey(win, vk, ctrl, shift, alt);
    }
}

void ShowSetScreenshotHotkeyDialog(MainWindow* win) {
    if (!IsMainWindowValidAndNotClosing(win)) {
        return;
    }
    CloseSetScreenshotHotkeyDialog();
    gHotkey.win = win;
    Str key = FindScreenshotShortcut();
    // the settings file spells a global shortcut "Global Ctrl+Shift+F5"
    Str shown = key;
    str::TrimPrefix(shown, StrL("Global "));
    gHotkey.currentHotkey = str::Dup(shown);
    gHotkey.newHotkey = {};
    gHotkey.visible = true;
    logf("ShowSetScreenshotHotkeyDialog: current '%s'\n", gHotkey.currentHotkey);
    DlgWindowSpec spec;
    spec.name = "screenshothotkey";
    spec.title = SetHotkeyDlgTitle;
    spec.build = SetScreenshotHotkeyDialogBuild;
    spec.close = CloseSetScreenshotHotkeyDialog;
    // every key is the combination being captured, PrtSc on its way up
    spec.onKey = SetScreenshotHotkeyOnKey;
    spec.onKeyUp = SetHotkeyOnKeyUp;
    spec.clientDx = kHotkeyWinDx;
    gHotkeyTw = DlgWindowOpen(spec, win);
    AppShellInvalidate(win);
}

bool SetScreenshotHotkeyOnKey(MainWindow* win, int vk, bool ctrl, bool shift, bool alt) {
    if (!gHotkey.visible || gHotkey.win != win) {
        return false;
    }
    if (vk == VK_CONTROL || vk == VK_SHIFT || vk == VK_MENU || vk == VK_LWIN || vk == VK_RWIN) {
        return true;
    }
    if (vk == VK_ESCAPE) {
        CloseSetScreenshotHotkeyDialog();
        return true;
    }
    // Return / arrows as a global hotkey without a modifier would steal
    // Enter and cursor keys from every app
    bool needsMod = (vk == VK_RETURN) || (vk == VK_LEFT) || (vk == VK_RIGHT) || (vk == VK_UP) || (vk == VK_DOWN);
    if (needsMod && !ctrl && !shift && !alt) {
        return true;
    }
    TempStr hotkey = SerializeHotkeyTemp((uint)vk, ctrl, shift, alt);
    if (len(hotkey) > 0) {
        str::ReplaceWithCopy(&gHotkey.newHotkey, hotkey);
        AppShellInvalidate(win);
    }
    return true;
}

void SetHotkeyView::OnSet(SetHotkeyView*, gp::Ctx* cx, const gp::ClickEvent*) {
    if (len(gHotkey.newHotkey) == 0) {
        return;
    }
    logf("SetHotkeyDoSet: setting screenshot hotkey to '%s'\n", gHotkey.newHotkey);
    TempStr globalKey = str::JoinTemp(StrL("Global "), gHotkey.newHotkey);
    Shortcut* sc = FindScreenshotShortcutEntry();
    if (sc) {
        str::ReplaceWithCopy(&sc->key, globalKey);
    } else {
        sc = new Shortcut();
        sc->cmd = str::Dup(StrL("CmdScreenshot"));
        sc->key = str::Dup(globalKey);
        VecAppend(*gSettings->shortcuts, sc);
    }
    ScheduleSaveSettings();
    // ng: registering the global hotkey with the OS is GlobalHotkeys.cpp, step 14
    CloseSetScreenshotHotkeyDialog();
    gp::Notify(cx);
}

void SetHotkeyView::OnRemove(SetHotkeyView*, gp::Ctx* cx, const gp::ClickEvent*) {
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
    CloseSetScreenshotHotkeyDialog();
    gp::Notify(cx);
}

void SetHotkeyView::OnCancel(SetHotkeyView*, gp::Ctx* cx, const gp::ClickEvent*) {
    CloseSetScreenshotHotkeyDialog();
    gp::Notify(cx);
}

static gp::El* SetHotkeyWinBuild(gp::Ctx* cx, Str display) {
    const gp::Theme& th = gp::ThemeNow(cx->app);
    float font = DlgWinFont(cx);
    gp::El* col = gp::Div(cx->a)->FlexCol()->W(gp::kFill)->PadX(kHotkeyWinPadX)->PadY(kHotkeyWinPadY);
    col->Child(DlgWinLabel(cx, ToGpui(Tr("Press a key combination:")), font, 0));

    bool hasKey = len(gHotkey.newHotkey) > 0 || len(gHotkey.currentHotkey) > 0;
    gp::El* shown = gp::TextEl(cx->a, GpuiDup(cx->a, display))->Font(font)->Fg(th.foreground);
    if (hasKey) {
        // orig's key cap
        shown = gp::Div(cx->a)
                    ->FlexRow()
                    ->ItemsCenter()
                    ->H(kHotkeyWinCapDy)
                    ->PadX(kHotkeyWinCapPadDx)
                    ->Radius(3)
                    ->Bg(ToGpui(AccentColor(ThemeWindowControlBackgroundColor(), 16)))
                    ->Border(1, ToGpui(AccentColor(ThemeWindowControlBackgroundColor(), 40)))
                    ->Child(shown);
    }
    col->Child(gp::Div(cx->a)
                   ->FlexRow()
                   ->ItemsCenter()
                   ->H(kHotkeyWinCapDy + kHotkeyWinCapGap)
                   ->PadT(kHotkeyWinCapGap)
                   ->Shrink0()
                   ->Child(shown));

    gp::El* buttons = gp::Div(cx->a)
                          ->FlexRow()
                          ->JustifyEnd()
                          ->ItemsCenter()
                          ->W(gp::kFill)
                          ->H(kDlgWinBtnDy + kHotkeyWinPadY)
                          ->PadT(kHotkeyWinPadY)
                          ->Gap(kDlgWinBtnGap)
                          ->Shrink0();
    buttons->Child(DlgWinButton(cx, GStrL("dlg-cancel"), Tr("Cancel"),
                                gp::ListenTo(gHotkeyView, &SetHotkeyView::OnCancel), false));
    buttons->Child(DlgWinButton(cx, GStrL("hotkey-remove"), Tr("Remove"),
                                gp::ListenTo(gHotkeyView, &SetHotkeyView::OnRemove), false, !hasKey));
    buttons->Child(DlgWinButton(cx, GStrL("dlg-ok"), Tr("Set"), gp::ListenTo(gHotkeyView, &SetHotkeyView::OnSet), true,
                                len(gHotkey.newHotkey) == 0));
    col->Child(buttons);
    return DlgWinContent(cx, col);
}

gp::El* SetScreenshotHotkeyDialogBuild(MainWindow* win, gp::Ctx* cx) {
    if (!gHotkey.visible || gHotkey.win != win) {
        return nullptr;
    }
    if (!gHotkeyView.IsValid()) {
        gHotkeyView = gp::EntityNewState<SetHotkeyView>(cx->app);
    }
    const gp::Theme& th = gp::ThemeNow(cx->app);
    Str display = StrL("None");
    if (len(gHotkey.newHotkey) > 0) {
        display = gHotkey.newHotkey;
    } else if (len(gHotkey.currentHotkey) > 0) {
        display = gHotkey.currentHotkey;
    }

    if (gHotkeyTw) {
        return DlgWindowIsHost(cx) ? SetHotkeyWinBuild(cx, display) : nullptr;
    }

    gp::El* body = gp::Div(cx->a)->FlexCol()->Gap(10)->ItemsCenter();
    body->Child(gp::TextEl(cx->a, ToGpui(Tr("Press a key combination:")))->Font(13)->Fg(th.foreground));
    body->Child(gp::Div(cx->a)
                    ->PadX(14)
                    ->PadY(8)
                    ->Radius(4)
                    ->Bg(th.tokens.muted)
                    ->Border(1, th.border)
                    ->Child(gp::TextEl(cx->a, GpuiDup(cx->a, display))->Font(15)->Fg(th.foreground)));

    gp::El* remove = gpc::Button::New(cx, GStrL("hotkey-remove"))
                         ->Label(ToGpui(Tr("Remove")))
                         ->WithSize(gp::UiSize::Small)
                         ->Disabled(len(gHotkey.currentHotkey) == 0 && len(gHotkey.newHotkey) == 0)
                         ->OnClick(gp::ListenTo(gHotkeyView, &SetHotkeyView::OnRemove))
                         ->IntoEl();
    gp::El* footer =
        DialogFooter(cx, remove, gHotkeyView, Tr("Set"), Tr("Cancel"), &SetHotkeyView::OnSet, &SetHotkeyView::OnCancel);

    return gpc::Dialog::New(cx)
        ->Open(true)
        ->Title(ToGpui(Tr("Set Screenshot Hotkey")))
        ->Body(body)
        ->Footer(footer)
        ->W(420)
        ->OnClose(gp::ListenTo(gHotkeyView, &SetHotkeyView::OnCancel))
        ->IntoEl(gp::WindowSize(cx->win));
}
