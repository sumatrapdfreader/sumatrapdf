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
#include "shared/PdfToolsCommon.h"
#include "SumatraDialogs.h"
#include "ImageSaveCropResize.h"
#if OS_WASM
#include "gui/WasmBridge.h"
#endif
#include "Screenshot.h"
#include "AppHelpersCommon.h"
#include "ScreenshotCommon.h"
#if OS_WIN
#include "ScreenshotCapture.h"
#endif

#include "SumatraLog.h"

static Kind kNotifScreenshot = "notifScreenshot";

// --- rendering a page region (orig's PdfTools RenderSelectionPixmap) --------

constexpr float kScreenshotDpi = 150;
constexpr i64 kMaxScreenshotPixels = 80 * 1000 * 1000;

static Pixmap* RenderRegionPixmap(EngineBase* engine, int rotation, int pageNo, RectF rect, float dpi) {
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
        logf("RenderRegionPixmap: %lld x %lld at %.0f DPI is too large\n", estW, estH, dpi);
        return nullptr;
    }
    RenderPageArgs args(pageNo, zoom, rotation, &rect, RenderTarget::Export);
    Pixmap* px = engine->RenderPage(args);
    if (!px) {
        logf("RenderRegionPixmap: RenderPage failed page %d\n", pageNo);
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
    return RenderRegionPixmap(engine, dm->GetRotation(), pageNo, rect, kScreenshotDpi);
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

#if OS_WIN
static bool IsScreenshotFrame(HWND hwnd) {
    for (MainWindow* w : gWindows) {
        if (AppShellNativeHwnd(w) == hwnd) {
            return true;
        }
    }
    return false;
}

static HWND ScreenshotOwnerHwnd() {
    return len(gWindows) > 0 ? AppShellNativeHwnd(gWindows[0]) : nullptr;
}

void ShowScreenshotPicker(HWND hwndRestore) {
    gScreenshotHost.IsAppFrame = IsScreenshotFrame;
    gScreenshotHost.GetSaveDirTemp = GetScreenshotSaveDirTemp;
    gScreenshotHost.GetOwnerHwnd = ScreenshotOwnerHwnd;
    TakeScreenshots(hwndRestore);
}
#endif

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

constexpr float kHotkeyDlgDx = 340;

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
    spec.clientDx = kHotkeyDlgDx;
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
    TempStr hotkey = SerializeHotkeyTemp((uint)vk, ctrl, shift, alt, false);
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

// orig's layout: the prompt, the combination as a key cap, Remove at the left
// of the footer
gp::El* SetScreenshotHotkeyDialogBuild(MainWindow* win, gp::Ctx* cx) {
    if (!gHotkey.visible || gHotkey.win != win) {
        return nullptr;
    }
    if (gHotkeyTw && !DlgWindowIsHost(cx)) {
        return nullptr;
    }
    if (!gHotkeyView.IsValid()) {
        gHotkeyView = gp::EntityNewState<SetHotkeyView>(cx->app);
    }
    Str display = StrL("None");
    if (len(gHotkey.newHotkey) > 0) {
        display = gHotkey.newHotkey;
    } else if (len(gHotkey.currentHotkey) > 0) {
        display = gHotkey.currentHotkey;
    }
    bool hasKey = len(gHotkey.newHotkey) > 0 || len(gHotkey.currentHotkey) > 0;
    gp::Listener onSet = gp::ListenTo(gHotkeyView, &SetHotkeyView::OnSet);
    gp::Listener onCancel = gp::ListenTo(gHotkeyView, &SetHotkeyView::OnCancel);

    gp::El* col = gp::Div(cx->a)->FlexCol()->W(gp::kFill);
    col->Child(DlgCaption(cx, Tr("Press a key combination:")));
    gp::El* shown =
        hasKey
            ? DlgKeyCap(cx, display, true)
            : gp::TextEl(cx->a, GpuiDup(cx->a, display))->Font(kToolDlgTextFontPx)->Fg(ToGpui(ThemeWindowTextColor()));
    col->Child(gp::Div(cx->a)
                   ->FlexRow()
                   ->ItemsCenter()
                   ->W(gp::kFill)
                   ->H(kToolDlgLineDy)
                   ->Shrink0()
                   ->PadX(kToolDlgPadDx)
                   ->Child(shown));
    gp::El* remove = DlgGhostButton(cx, GStrL("hotkey-remove"), Tr("Remove"),
                                    gp::ListenTo(gHotkeyView, &SetHotkeyView::OnRemove), !hasKey);
    col->Child(DlgFooter(cx, Tr("Set"), onSet, onCancel, remove, len(gHotkey.newHotkey) == 0));
    return DlgToolContent(cx, col, Tr("Set Screenshot Hotkey"), kHotkeyDlgDx, onCancel);
}
