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
#include "SumatraConfig.h"
#include "SumatraPDF.h"
#include "Translations.h"
#include "SumatraDialogs.h"
#include "PDFScript.h"

struct PDFScriptWnd;
constexpr int kScriptListWidth = 360;
constexpr int kScriptColumnGap = 6;
struct ScriptArgRow {
    PDFScriptWnd* owner;
    Edit* edit;
};

struct PDFScriptWnd : WindowBase {
    Vec<ScriptArgRow*> rows;
    VBox* argsBox = nullptr;
    Edit* script = nullptr;
    Edit* output = nullptr;
    VirtListBox* examples = nullptr;
    VirtButton* run = nullptr;
    MainWindow* win = nullptr;
    Str input;
    bool running = false;
    u64 runId = 0;

    ~PDFScriptWnd() override {
        for (auto* row : rows) delete row;
        str::Free(input);
    }
    void AddArg(Str value, Str hint = {});
    void LayoutArgs();
    void OnAdd();
    void OnRun();
    void OnExample();
    void OnExamples();
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

void PDFScriptWnd::AddArg(Str value, Str hint) {
    auto* row = new HBox();
    row->alignCross = CrossAxisAlign::CrossCenter;
    row->gap = DpiScale(6);
    auto* edit = new Edit();
    Edit::CreateArgs ea;
    ea.parent = hwnd;
    ea.font = GetFont();
    ea.withBorder = true;
    ea.text = value;
    ea.cueText = hint;
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
    DoLayout();
    InvalidateRect(hwnd, nullptr, FALSE);
}

void PDFScriptWnd::OnAdd() {
    if (running) return;
    AddArg({});
    LayoutArgs();
    UpdateTheme();
    auto* edit = rows[len(rows) - 1]->edit;
    edit->SetFocus();
    RedrawWindow(edit->hwnd, nullptr, nullptr, RDW_FRAME | RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW);
}

static void ScriptFinished(u64* runId, PDFScriptResult* result) {
    if (gScriptWnd && gScriptWnd->runId == *runId) {
        gScriptWnd->running = false;
        gScriptWnd->run->SetText(Tr("Run Script"));
        gScriptWnd->run->SetIsEnabled(true);
        gScriptWnd->examples->SetIsEnabled(true);
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
    examples->SetIsEnabled(false);
    output->SetText({});
    output->SetIsVisible(false);
    StartPDFScript(script->GetTextTemp(), args, MkFunc1(ScriptFinished, new u64(runId)));
}

void PDFScriptWnd::OnExample() {
    if (running) return;
    bool listFocused = vroot && vroot->focused == examples;
    Str source = PDFScriptSource(examples->GetCurrentSelection());
    StrVec values;
    StrVec hints;
    GetPDFScriptArgs(source, input, values, hints);

    // Reuse edits and paint only after the new script and rows are laid out.
    bool redraw = IsWindowVisible(hwnd);
    if (redraw) SuspendRedraw();
    script->SuspendRedraw();
    script->SetText(str::LFToCRLFTemp(source));
    int oldCount = len(rows);
    while (len(rows) > len(values)) {
        int last = len(rows) - 1;
        delete argsBox->children[last].layout;
        VecRemoveAt(argsBox->children, last);
        delete rows[last];
        VecRemoveAt(rows, last);
    }
    for (int i = 0; i < len(values); i++) {
        if (i >= len(rows)) {
            AddArg(values[i], hints[i]);
            continue;
        }
        rows[i]->edit->SuspendRedraw();
        rows[i]->edit->SetText(values[i]);
        EditSetCueText(rows[i]->edit, hints[i]);
        rows[i]->edit->ResumeRedraw();
    }
    if (layout) {
        LayoutArgs();
        if (len(rows) > oldCount) UpdateTheme();
    }
    script->ResumeRedraw();
    if (redraw) ResumeRedraw();
    if (listFocused) SetFocusTo(examples);
    RedrawWindow(hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_FRAME | RDW_NOERASE);
}

void PDFScriptWnd::OnExamples() {
    bool show = examples->GetVisibility() == Visibility::Collapse;
    examples->SetVisibility(show ? Visibility::Visible : Visibility::Collapse);
    Rect rect = HwndWindowRect(hwnd);
    int extra = DpiScale(kScriptListWidth + kScriptColumnGap);
    SetWindowPos(hwnd, nullptr, 0, 0, rect.dx + (show ? extra : -extra), rect.dy, SWP_NOMOVE | SWP_NOZORDER);
    HwndEnsureOnScreen(hwnd);
    DoLayout();
    InvalidateRect(hwnd, nullptr, TRUE);
}

void PDFScriptWnd::OnHelp() {
    LaunchDocumentation(StrL(kPDFScriptHelpDocUri));
}

void PDFScriptWnd::OnCancel() {
    ScheduleDelete();
}

void PDFScriptWnd::OnSize(WindowBase::SizeEvent*) {
    DoLayout();
}

void PDFScriptWnd::Create(MainWindow* owner, Str path) {
    win = owner;
    input = str::Dup(path);
    SetFont(GetAppFont());
    CreateCustomArgs ca;
    ca.owner = win->hwndFrame;
    ca.title = Tr("Run PDF Script");
    ca.font = GetFont();
    ca.icon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(GetAppIconID()));
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
    body->AddChild(argsBox);
    auto* addRow = new HBox();
    addRow->alignMain = MainAxisAlign::MainEnd;
    auto* add = NewThemedButton(hwnd, Tr("Add argument"), GetFont(), false);
    add->onClick = MkMethod0<PDFScriptWnd, &PDFScriptWnd::OnAdd>(this);
    addRow->AddChild(add);
    body->AddChild(addRow);
    examples = new VirtListBox();
    examples->font = GetFont();
    examples->dpi = GetDpi();
    examples->idealSizeDx = DpiScale(kScriptListWidth);
    auto* model = new ListBoxModelStrings();
    for (int i = 0; i < PDFScriptCount(); i++) model->strings.Append(PDFScriptName(i));
    examples->SetModel(model);
    examples->SetCurrentSelection(0);
    examples->SetVisibility(Visibility::Collapse);
    examples->onSelectionChanged = MkMethod0<PDFScriptWnd, &PDFScriptWnd::OnExample>(this);
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
    output = makeEdit();
    SendMessageW(output->hwnd, EM_SETREADONLY, TRUE, 0);
    output->SetIsVisible(false);
    body->AddChild(output, 1);
    auto* buttons = new HBox();
    buttons->alignMain = MainAxisAlign::MainStart;
    buttons->gap = DpiScale(7);
    auto* showExamples = NewThemedButton(hwnd, Tr("Example Scripts"), GetFont(), false);
    showExamples->onClick = MkMethod0<PDFScriptWnd, &PDFScriptWnd::OnExamples>(this);
    buttons->AddChild(showExamples);
    auto* help = NewThemedButton(hwnd, Tr("Help"), GetFont(), false);
    help->onClick = MkMethod0<PDFScriptWnd, &PDFScriptWnd::OnHelp>(this);
    buttons->AddChild(help);
    buttons->AddChild(new Spacer(0, 0), 1);
    auto* cancel = NewThemedButton(hwnd, Tr("Cancel"), GetFont(), false);
    cancel->onClick = MkMethod0<PDFScriptWnd, &PDFScriptWnd::OnCancel>(this);
    buttons->AddChild(cancel);
    run = NewThemedButton(hwnd, Tr("Run Script"), GetFont(), false);
    run->onClick = MkMethod0<PDFScriptWnd, &PDFScriptWnd::OnRun>(this);
    buttons->AddChild(run);
    body->AddChild(buttons);
    auto* columns = new HBox();
    columns->alignCross = CrossAxisAlign::Stretch;
    columns->gap = DpiScale(kScriptColumnGap);
    columns->AddChild(body, 1);
    columns->AddChild(examples);
    layout = new Padding(columns, DpiScaledInsets(8));
    OnExample();
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
