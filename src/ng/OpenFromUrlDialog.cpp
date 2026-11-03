/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: orig's OpenFromUrlDialog.cpp is a WS_POPUPWINDOW with the URL, the folder
// with Browse... and Cancel / Download. Here it is the same column of gpui
// elements, in a window of its own where the platform has one, else in a card
// over the frame. wasm has no folder: the download is stored like a dropped file.

#include "gui/GpuiBridge.h"

#include "gui/Dpi.h"
#include "gui/UIModels.h"

#include "Settings.h"
#include "AppSettings.h"
#include "EngineBase.h"
#include "DocController.h"
#include "MainWindow.h"
#include "Theme.h"
#include "SumatraConfig.h"
#include "SumatraPDF.h"
#include "Translations.h"
#include "gui/AppShell.h"
#include "gui/DialogWidgets.h"
#include "gui/ToolWindow.h"
#include "SumatraDialogs.h"
#include "PdfToolsCommon.h"
#include "AppHelpersCommon.h"
#include "OpenFromUrlCommon.h"

struct OpenFromUrlDlg {
    MainWindow* win = nullptr;
    bool visible = false;
    bool wantFocus = false;
    gpui::InputState* editUrl = nullptr;
    gpui::InputState* editDir = nullptr;
};

static OpenFromUrlDlg gUrlDlg;

constexpr float kOpenFromUrlDlgDx = 520;

struct OpenFromUrlView {
    static void OnOk(OpenFromUrlView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnCancel(OpenFromUrlView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnBrowse(OpenFromUrlView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnInput(OpenFromUrlView* self, gp::Ctx* cx, const gp::InputEvent* ev);
};

static gp::Entity<OpenFromUrlView> gUrlView;

// orig's modal window, where the platform can have one (DlgWindowOpen); null: a
// dialog in the frame
static ToolWindow* gOpenFromUrlTw = nullptr;

bool IsOpenFromUrlDialogVisible() {
    return gUrlDlg.visible && !gOpenFromUrlTw;
}

static void FreeInput(gp::Window* host, gp::InputState** edit) {
    if (host && *edit) {
        gp::InputBlur(*edit, host->app, host);
    }
    delete *edit;
    *edit = nullptr;
}

void CloseOpenFromUrlDialog() {
    if (!gUrlDlg.visible) {
        return;
    }
    gUrlDlg.visible = false;
    MainWindow* win = gUrlDlg.win;
    // Blur the window that holds the fields before freeing them. The tool
    // window's close otherwise blurs the freed input.
    gp::Window* host = gOpenFromUrlTw ? ToolWindowGpui(gOpenFromUrlTw) : (win ? win->gpuiWin : nullptr);
    FreeInput(host, &gUrlDlg.editUrl);
    FreeInput(host, &gUrlDlg.editDir);
    DlgWindowClose(&gOpenFromUrlTw);
    AppShellInvalidate(win);
}

// orig's OpenFromUrlWnd::OnOk: the dialog stays open when the URL or the
// folder won't do
static void OpenFromUrlOk() {
    MainWindow* win = gUrlDlg.win;
    TempStr url = str::DupTemp(FromGpui(gp::InputValue(gUrlDlg.editUrl)));
    TempStr dir;
    if (gUrlDlg.editDir) {
        dir = str::DupTemp(FromGpui(gp::InputValue(gUrlDlg.editDir)));
    }
    if (StartOpenFromUrl(win, url, dir)) {
        CloseOpenFromUrlDialog();
    }
}

void OpenFromUrlView::OnOk(OpenFromUrlView*, gp::Ctx* cx, const gp::ClickEvent*) {
    OpenFromUrlOk();
    gp::Notify(cx);
}

void OpenFromUrlView::OnCancel(OpenFromUrlView*, gp::Ctx* cx, const gp::ClickEvent*) {
    CloseOpenFromUrlDialog();
    gp::Notify(cx);
}

void OpenFromUrlView::OnBrowse(OpenFromUrlView*, gp::Ctx* cx, const gp::ClickEvent*) {
    TempStr dir = AppShellPromptForFolderTemp(gUrlDlg.win, Tr("Save downloaded files to"));
    if (len(dir) > 0 && gUrlDlg.editDir) {
        gp::InputSetValue(gUrlDlg.editDir, ToGpui(dir));
    }
    gp::Notify(cx);
}

void OpenFromUrlView::OnInput(OpenFromUrlView*, gp::Ctx* cx, const gp::InputEvent* ev) {
    if (ev->kind != gp::InputEventKind::PressEnter) {
        return;
    }
    OpenFromUrlOk();
    gp::Notify(cx);
}

static Str OpenFromUrlDlgTitle() {
    return Tr("Open from URL");
}

static gp::InputState* NewInput(MainWindow* win, Str value) {
    auto* edit = new gp::InputState();
    edit->focus = gp::FocusHandleNew(win->gpuiWin ? win->gpuiWin->app : nullptr);
    gp::InputSetValue(edit, ToGpui(value));
    return edit;
}

void ShowOpenFromUrlDialog(MainWindow* win) {
    if (!IsMainWindowValidAndNotClosing(win)) {
        return;
    }
    if (!gUrlDlg.visible) {
        gUrlDlg.editUrl = NewInput(win, {});
        if (!OS_WASM) {
            // the remembered folder, else the Downloads folder
            gUrlDlg.editDir = NewInput(win, DownloadDirTemp());
        }
    }
    gUrlDlg.win = win;
    gUrlDlg.visible = true;
    DlgWindowSpec spec;
    spec.name = "openfromurl";
    spec.title = OpenFromUrlDlgTitle;
    spec.modal = true;
    spec.build = OpenFromUrlDialogBuild;
    spec.close = CloseOpenFromUrlDialog;
    spec.clientDx = kOpenFromUrlDlgDx;
    gOpenFromUrlTw = DlgWindowOpen(spec, win);
    gUrlDlg.wantFocus = true;
    AppShellInvalidate(win);
}

// orig's layout: the prompt, the URL over a hairline, Save to [folder]
// [Browse...], Cancel / Download
gp::El* OpenFromUrlDialogBuild(MainWindow* win, gp::Ctx* cx) {
    if (!gUrlDlg.visible || gUrlDlg.win != win) {
        return nullptr;
    }
    if (gOpenFromUrlTw && !DlgWindowIsHost(cx)) {
        return nullptr;
    }
    if (!gUrlView.IsValid()) {
        gUrlView = gp::EntityNewState<OpenFromUrlView>(cx->app);
    }
    gp::Listener onInput = gp::ListenTo(gUrlView, &OpenFromUrlView::OnInput);
    gUrlDlg.editUrl->onChange = onInput;

    gp::Listener onOk = gp::ListenTo(gUrlView, &OpenFromUrlView::OnOk);
    gp::Listener onCancel = gp::ListenTo(gUrlView, &OpenFromUrlView::OnCancel);
    DlgSetDefault(cx, onOk);
    gp::El* col = gp::Div(cx->a)->FlexCol()->W(gp::kFill);
    col->Child(DlgCaption(cx, Tr("URL of a document to download and open:")));
    col->Child(DlgLine(cx, {}, 0, StrL("openurl-url"), gUrlDlg.editUrl, DlgField::Underlined));
    if (gUrlDlg.editDir) {
        gUrlDlg.editDir->onChange = onInput;
        gp::El* row = DlgFieldRow(cx);
        row->Child(DlgFieldLabel(cx, Tr("Save to"), 0));
        row->Child(DlgUnderlineInput(cx, GStrL("openurl-dir"), gUrlDlg.editDir, kToolDlgTextFontPx, ThemeEdgeColor())
                       ->Flex1()
                       ->MinW(0));
        row->Child(DlgGhostButton(cx, GStrL("openurl-browse"), Tr("Browse..."),
                                  gp::ListenTo(gUrlView, &OpenFromUrlView::OnBrowse)));
        col->Child(DlgSection(cx)->Child(row));
    }
    col->Child(DlgFooter(cx, Tr("Download"), onOk, onCancel));
    gp::El* content = DlgToolContent(cx, col, Tr("Open from URL"), kOpenFromUrlDlgDx, onCancel);

    if (gUrlDlg.wantFocus) {
        gp::InputFocus(gUrlDlg.editUrl, cx->app, cx->win);
        gUrlDlg.wantFocus = cx->win->input != gUrlDlg.editUrl;
    }
    return content;
}
