/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// orig's message boxes (base's MsgBox(): MessageBoxW) and its "Unsaved
// changes" task dialog (ShouldSaveAnnotationsDialog in SumatraPDF.cpp).
// ng: both block in a message loop of their own, which a gpui listener must
// not do, so they are shown from the ui task queue and report through a
// callback. The owner is the main window, or its tool window that was the
// active one, as orig's box is owned by the window that asks.

#include "base/Base.h"
#include "base/UITask.h"
#include "base/Win.h"

#include <commctrl.h>

#include "gui/UIModels.h"
#include "Settings.h"
#include "DisplayMode.h"
#include "DocController.h"
#include "EngineBase.h"
#include "Translations.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "gui/AppShell.h"
#include "gui/ToolWindow.h"
#include "SumatraDialogs.h"
#include "gui/NativeMsgBox.h"

#include "SumatraLog.h"

static bool gNativeMsgBoxOff = false;

bool NativeMsgBoxEnabled() {
    return !gNativeMsgBoxOff && ToolWindowsAvailable();
}

void NativeMsgBoxSetEnabled(bool enabled) {
    gNativeMsgBoxOff = !enabled;
}

static HWND OwnerHwnd(MainWindow* win) {
    if (!IsMainWindowValid(win)) {
        return nullptr;
    }
    if (ToolWindow* tw = ToolWindowActiveOf(win)) {
        if (HWND hwnd = ToolWindowHwnd(tw)) {
            return hwnd;
        }
    }
    return AppShellNativeHwnd(win);
}

struct NativeMsgBoxArgs {
    MainWindow* win = nullptr;
    WStr text;
    WStr caption;
    uint flags = 0;
    Func1<int> onResult;
};

// the strings are on the heap: the box's message loop lets gpui render, which
// resets the temp arena
static void NativeMsgBoxRun(NativeMsgBoxArgs* args) {
    HWND owner = OwnerHwnd(args->win);
    int res = MessageBoxW(owner, args->text.s, args->caption.s, args->flags);
    logf("NativeMsgBox: result %d\n", res);
    Func1<int> onResult = args->onResult;
    MainWindow* win = args->win;
    wstr::Free(args->text);
    wstr::Free(args->caption);
    delete args;
    onResult.Call(res);
    if (IsMainWindowValid(win)) {
        AppShellInvalidate(win);
    }
}

void NativeMsgBox(MainWindow* win, Str text, Str caption, uint flags, Func1<int> onResult) {
    auto* args = new NativeMsgBoxArgs();
    args->win = win;
    args->text = ToWStr(text);
    args->caption = ToWStr(caption);
    args->flags = flags;
    args->onResult = onResult;
    uitask::Post(MkFunc0(NativeMsgBoxRun, args), "NativeMsgBox");
}

struct NativeUnsavedArgs {
    MainWindow* win = nullptr;
    WStr mainInstr;
    Func1<int> onChoice;
};

// orig's button ids
constexpr int kBtnIdDiscard = 100;
constexpr int kBtnIdSaveToExisting = 101;
constexpr int kBtnIdSaveToNew = 102;

// orig's ShouldSaveAnnotationsDialog
static void NativeUnsavedRun(NativeUnsavedArgs* args) {
    HWND owner = OwnerHwnd(args->win);
    WStr title = ToWStr(Tr("Unsaved changes"));
    WStr content = ToWStr(Tr("Save changes?"));
    WStr btnExisting = ToWStr(Tr("&Save to existing PDF"));
    WStr btnNew = ToWStr(Tr("Save to &new PDF"));
    WStr btnDiscard = ToWStr(Tr("&Discard changes"));
    WStr btnCancel = ToWStr(Tr("&Cancel"));

    TASKDIALOG_BUTTON buttons[4];
    buttons[0].nButtonID = kBtnIdSaveToExisting;
    buttons[0].pszButtonText = btnExisting.s;
    buttons[1].nButtonID = kBtnIdSaveToNew;
    buttons[1].pszButtonText = btnNew.s;
    buttons[2].nButtonID = kBtnIdDiscard;
    buttons[2].pszButtonText = btnDiscard.s;
    buttons[3].nButtonID = IDCANCEL;
    buttons[3].pszButtonText = btnCancel.s;

    DWORD flags =
        TDF_ALLOW_DIALOG_CANCELLATION | TDF_SIZE_TO_CONTENT | TDF_ENABLE_HYPERLINKS | TDF_POSITION_RELATIVE_TO_WINDOW;
    if (trans::IsCurrLangRtl()) {
        flags |= TDF_RTL_LAYOUT;
    }
    TASKDIALOGCONFIG cfg{};
    cfg.cbSize = sizeof(TASKDIALOGCONFIG);
    cfg.pszWindowTitle = title.s;
    cfg.pszMainInstruction = args->mainInstr.s;
    cfg.pszContent = content.s;
    cfg.nDefaultButton = IDCANCEL;
    cfg.dwFlags = (TASKDIALOG_FLAGS)flags;
    cfg.cButtons = dimof(buttons);
    cfg.pButtons = &buttons[0];
    cfg.pszMainIcon = TD_INFORMATION_ICON;
    cfg.hwndParent = owner;

    int pressed = 0;
    HRESULT hr = TaskDialogIndirect(&cfg, &pressed, nullptr, nullptr);
    // as orig: a dialog that could not be shown discards
    SaveChoice choice = SaveChoice::Discard;
    if (hr == S_OK) {
        if (pressed == kBtnIdSaveToExisting) {
            choice = SaveChoice::SaveExisting;
        } else if (pressed == kBtnIdSaveToNew) {
            choice = SaveChoice::SaveNew;
        } else if (pressed == IDCANCEL) {
            choice = SaveChoice::Cancel;
        }
    }
    logf("NativeUnsavedDialog: hr 0x%x button %d choice %d\n", (int)hr, pressed, (int)choice);
    wstr::Free(title);
    wstr::Free(content);
    wstr::Free(btnExisting);
    wstr::Free(btnNew);
    wstr::Free(btnDiscard);
    wstr::Free(btnCancel);
    Func1<int> onChoice = args->onChoice;
    MainWindow* win = args->win;
    wstr::Free(args->mainInstr);
    delete args;
    onChoice.Call((int)choice);
    if (IsMainWindowValid(win)) {
        AppShellInvalidate(win);
    }
}

void NativeUnsavedDialog(MainWindow* win, Str fileName, Func1<int> onChoice) {
    auto* args = new NativeUnsavedArgs();
    args->win = win;
    args->mainInstr = ToWStr(fmt(Tr("Unsaved changes in '%s'").s, fileName));
    args->onChoice = onChoice;
    uitask::Post(MkFunc0(NativeUnsavedRun, args), "NativeUnsavedDialog");
}
