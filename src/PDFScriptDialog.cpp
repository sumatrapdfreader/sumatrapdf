/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Win.h"
#include "base/UITask.h"
#include "gui/Dpi.h"
#include "gui/UIModels.h"
#include "gui/Layout.h"
#include "gui/win/WinGui.h"
#include "gui/PlatformFont.h"
#include "gui/Gfx.h"
#include "gui/VirtCtrl.h"
#include "Settings.h"
#include "AppSettings.h"
#include "Theme.h"
#include "DocController.h"
#include "EngineBase.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "SumatraPDF.h"
#include "Translations.h"
#include "SumatraDialogs.h"
#include "PDFScript.h"

struct PDFScriptWnd;
struct ScriptArgRow {
    PDFScriptWnd* owner;
    Edit* edit;
};

struct ScriptArgsScroll : ScrollBox {
    ScriptArgsScroll(ILayout* child) : ScrollBox(child) {}
    Size Layout(Constraints bc) override {
        bc.min.dy = bc.max.dy = DpiScale(100);
        return ScrollBox::Layout(bc);
    }
    int MinIntrinsicHeight(int) override { return DpiScale(100); }
};

struct PDFScriptWnd : WindowBase {
    Vec<ScriptArgRow*> rows;
    VBox* argsBox = nullptr;
    ScrollBox* argsScroll = nullptr;
    Edit* script = nullptr;
    Edit* output = nullptr;
    DropDown* examples = nullptr;
    VirtButton* run = nullptr;
    MainWindow* win = nullptr;
    bool running = false;
    u64 runId = 0;

    ~PDFScriptWnd() override {
        for (auto* row : rows) delete row;
    }
    void AddArg(Str value);
    void LayoutArgs();
    void OnAdd();
    void OnRun();
    void OnExample();
    void OnHelp();
    void OnCancel();
    void OnSize(WindowBase::SizeEvent*);
    void Create(MainWindow* owner, Str path);
};

static PDFScriptWnd* gScriptWnd = nullptr;
static u64 gScriptRunId = 0;

static void DeleteScriptArg(ScriptArgRow* row) {
    auto* wnd = gScriptWnd;
    if (!wnd) return;
    if (wnd->running) return;
    for (int i = 0; i < len(wnd->rows); i++) {
        if (wnd->rows[i] != row) continue;
        delete wnd->argsBox->children[i].layout;
        VecRemoveAt(wnd->argsBox->children, i);
        VecRemoveAt(wnd->rows, i);
        delete row;
        wnd->LayoutArgs();
        return;
    }
}

static void QueueDeleteScriptArg(ScriptArgRow* row) {
    // Deleting the clicked virtual button must wait until its event returns.
    uitask::Post(MkFunc0(DeleteScriptArg, row));
}

static void PickScriptArg(ScriptArgRow* row) {
    if (row->owner->running) return;
    WCHAR path[MAX_PATH]{};
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = row->owner->hwnd;
    ofn.lpstrFile = path;
    ofn.nMaxFile = dimof(path);
    ofn.lpstrFilter = L"All files\0*.*\0\0";
    ofn.Flags = OFN_EXPLORER | OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST;
    if (!GetOpenFileNameW(&ofn) || !gScriptWnd) return;
    for (auto* current : gScriptWnd->rows) {
        if (current == row) current->edit->SetText(ToUtf8Temp(WStr(path)));
    }
}

void PDFScriptWnd::AddArg(Str value) {
    auto* row = new HBox();
    row->alignCross = CrossAxisAlign::CrossCenter;
    row->gap = DpiScale(6);
    row->AddChild(new VirtText(fmt("%s %d", Tr("Arg"), len(rows) + 1), GetFont()));
    auto* edit = new Edit();
    Edit::CreateArgs ea;
    ea.parent = hwnd;
    ea.font = GetFont();
    ea.withBorder = true;
    ea.text = value;
    edit->Create(ea);
    row->AddChild(edit, 1);
    auto* arg = new ScriptArgRow{this, edit};
    VecAppend(rows, arg);
    auto* pick = NewThemedButton(hwnd, StrL("..."), GetFont(), false);
    pick->onClick = MkFunc0(PickScriptArg, arg);
    row->AddChild(pick);
    auto* remove = NewThemedButton(hwnd, StrL("x"), GetFont(), false);
    remove->onClick = MkFunc0(QueueDeleteScriptArg, arg);
    row->AddChild(remove);
    argsBox->AddChild(row);
}

void PDFScriptWnd::LayoutArgs() {
    for (int i = 0; i < len(rows); i++) {
        auto* row = (HBox*)argsBox->children[i].layout;
        ((VirtText*)row->children[0].layout)->SetText(fmt("%s %d", Tr("Arg"), i + 1));
    }
    DoLayout();
    InvalidateRect(hwnd, nullptr, TRUE);
}

void PDFScriptWnd::OnAdd() {
    if (running) return;
    AddArg({});
    LayoutArgs();
    argsScroll->ScrollTo(argsScroll->MaxScrollY());
    rows[len(rows) - 1]->edit->SetFocus();
}

static void ScriptFinished(u64* runId, PDFScriptResult* result) {
    if (gScriptWnd && gScriptWnd->runId == *runId) {
        gScriptWnd->running = false;
        gScriptWnd->run->SetText(Tr("Run Script"));
        gScriptWnd->run->SetIsEnabled(true);
        gScriptWnd->output->SetText(result->output);
        gScriptWnd->output->SetIsVisible(len(result->output) > 0);
        gScriptWnd->DoLayout();
    }
    delete runId;
    delete result;
}

void PDFScriptWnd::OnRun() {
    if (running || len(script->GetTextTemp()) == 0) return;
    StrVec args;
    for (auto* row : rows) args.Append(row->edit->GetTextTemp());
    running = true;
    runId = ++gScriptRunId;
    run->SetText(Tr("Running..."));
    run->SetIsEnabled(false);
    output->SetText({});
    output->SetIsVisible(false);
    StartPDFScript(script->GetTextTemp(), args, MkFunc1(ScriptFinished, new u64(runId)));
}

void PDFScriptWnd::OnExample() {
    script->SetText(str::LFToCRLFTemp(PDFScriptSource(CbGetCurrentSelection(examples))));
}

void PDFScriptWnd::OnHelp() {
    SumatraLaunchBrowser(Str(kPDFScriptHelpUrl));
}

void PDFScriptWnd::OnCancel() {
    ScheduleDelete();
}

void PDFScriptWnd::OnSize(WindowBase::SizeEvent*) {
    DoLayout();
}

void PDFScriptWnd::Create(MainWindow* owner, Str path) {
    win = owner;
    SetFont(GetAppFont());
    CreateCustomArgs ca;
    ca.owner = win->hwndFrame;
    ca.title = Tr("Run PDF Script");
    ca.font = GetFont();
    ca.style = WS_OVERLAPPEDWINDOW;
    ca.visible = false;
    CreateCustom(ca);
    if (!hwnd) return;
    auto* body = new VBox();
    body->alignCross = CrossAxisAlign::Stretch;
    body->gap = DpiScale(6);
    argsBox = new VBox();
    argsBox->alignCross = CrossAxisAlign::Stretch;
    argsBox->gap = DpiScale(4);
    argsScroll = new ScriptArgsScroll(argsBox);
    body->AddChild(argsScroll);
    AddArg(path);
    AddArg(PDFScriptOutputPath(path));
    auto* addRow = new HBox();
    addRow->alignMain = MainAxisAlign::MainEnd;
    auto* add = NewThemedButton(hwnd, Tr("Add arg"), GetFont(), false);
    add->onClick = MkMethod0<PDFScriptWnd, &PDFScriptWnd::OnAdd>(this);
    addRow->AddChild(add);
    body->AddChild(addRow);
    examples = new DropDown();
    DropDown::CreateArgs da;
    da.parent = hwnd;
    da.font = GetFont();
    examples->Create(da);
    StrVec names;
    for (int i = 0; i < PDFScriptCount(); i++) names.Append(PDFScriptName(i));
    examples->SetItems(names);
    CbSetCurrentSelection(examples, 0);
    examples->onSelectionChanged = MkMethod0<PDFScriptWnd, &PDFScriptWnd::OnExample>(this);
    body->AddChild(examples);
    auto makeEdit = [&]() {
        auto* edit = new Edit();
        Edit::CreateArgs ea;
        ea.parent = hwnd;
        ea.font = GetFont();
        ea.isMultiLine = true;
        ea.noWrap = true;
        ea.withBorder = true;
        ea.idealSizeLines = 12;
        edit->Create(ea);
        return edit;
    };
    script = makeEdit();
    body->AddChild(script, 3);
    OnExample();
    output = makeEdit();
    SendMessageW(output->hwnd, EM_SETREADONLY, TRUE, 0);
    output->SetIsVisible(false);
    body->AddChild(output, 1);
    auto* buttons = new HBox();
    buttons->alignMain = MainAxisAlign::MainEnd;
    buttons->gap = DpiScale(7);
    auto* cancel = NewThemedButton(hwnd, Tr("Cancel"), GetFont(), false);
    cancel->onClick = MkMethod0<PDFScriptWnd, &PDFScriptWnd::OnCancel>(this);
    buttons->AddChild(cancel);
    auto* help = NewThemedButton(hwnd, Tr("Help"), GetFont(), false);
    help->onClick = MkMethod0<PDFScriptWnd, &PDFScriptWnd::OnHelp>(this);
    buttons->AddChild(help);
    run = NewThemedButton(hwnd, Tr("Run Script"), GetFont(), false);
    run->onClick = MkMethod0<PDFScriptWnd, &PDFScriptWnd::OnRun>(this);
    buttons->AddChild(run);
    body->AddChild(buttons);
    layout = new Padding(body, DpiScaledInsets(8));
    onSize = MkMethod1<PDFScriptWnd, WindowBase::SizeEvent*, &PDFScriptWnd::OnSize>(this);
    SetWindowPos(hwnd, nullptr, 0, 0, DpiScale(760), DpiScale(650), SWP_NOMOVE | SWP_NOZORDER);
    DoLayout();
    HwndCenterDialog(hwnd, owner->hwndFrame);
    HwndEnsureOnScreen(hwnd);
    UpdateTheme();
    SetIsVisible(true);
    script->SetFocus();
}

static void ClearScriptWnd() {
    gScriptWnd = nullptr;
    ++gScriptRunId;
}

static void CloseScriptWnd(WindowBase::CloseEvent*) {
    if (gScriptWnd) gScriptWnd->ScheduleDelete();
}

void ShowPDFScriptDialog(MainWindow* win) {
    auto* tab = win ? win->CurrentTab() : nullptr;
    if (!CanAccessDisk() || !tab || !tab->GetEngine() || !str::EqI(tab->GetEngine()->defaultExt, StrL(".pdf"))) return;
    if (gScriptWnd) {
        HwndSetFocus(gScriptWnd->hwnd);
        return;
    }
    auto* wnd = new PDFScriptWnd();
    wnd->closeOnEsc = true;
    wnd->onClose = MkFunc1Void<WindowBase::CloseEvent*>(CloseScriptWnd);
    wnd->onBeforeDelete = MkFunc0Void(ClearScriptWnd);
    wnd->Create(win, tab->filePath);
    if (!wnd->hwnd) {
        delete wnd;
        return;
    }
    gScriptWnd = wnd;
    RunModalWindow(wnd->hwnd, win->hwndFrame);
}
