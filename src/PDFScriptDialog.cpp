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
#include "gui/GuiColors.h"
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
#include "shared/PdfToolsCommon.h"

// The dialog, drawn like the tool dialogs: the script's arguments as
// labelled fields over hairlines, the source in a bare
// monospaced editor that takes the rest of the window, the output under a
// hairline once there is some, and the footer with the Ctrl+Enter hint,
// Example Scripts, Help, Cancel and the one filled Run Script. Example
// Scripts opens the list of the included scripts beside the editor, past a
// vertical hairline, with a rounded selection like the palette's.
constexpr int kScriptDlgDx = 760;
constexpr int kScriptDlgDy = 600;
constexpr int kScriptDlgMinDy = 400;
constexpr int kScriptLabelDx = 96;
constexpr int kScriptLabelMaxDx = 240;
constexpr int kScriptListDx = 300;
constexpr int kScriptListRowDy = 28;
constexpr int kScriptListInsetDx = 8;
constexpr int kScriptListRadius = 4;
constexpr int kScriptListSelShift = 28;
constexpr int kScriptOutputLines = 7;
constexpr int kScriptCodeFontPx = 14;
constexpr int kScriptOutputFontPx = 13;

struct PDFScriptWnd;
struct ScriptArgRow {
    PDFScriptWnd* owner;
    VirtText* label;
    Edit* edit;
    Str hint; // owned

    ~ScriptArgRow() { str::Free(hint); }
};

struct PDFScriptWnd : WindowBase {
    Vec<ScriptArgRow*> rows;
    VBox* argsBox = nullptr;
    Edit* script = nullptr;
    Edit* output = nullptr;
    VirtLine* outputLine = nullptr;
    ILayout* outputBox = nullptr;
    VirtListBox* examples = nullptr;
    ILayout* examplesBox = nullptr;
    VirtLine* examplesLine = nullptr;
    VirtButton* run = nullptr;
    MainWindow* win = nullptr;
    Str input;
    bool running = false;
    u64 runId = 0;
    Color colTxt = kColorUnset;
    Color colBg = kColorUnset;
    Color colMuted = kColorUnset;
    PlatformFont* fontText = nullptr;
    PlatformFont* fontMeta = nullptr;
    PlatformFont* fontCode = nullptr;
    PlatformFont* fontOutput = nullptr;

    ~PDFScriptWnd() override {
        for (auto* row : rows) delete row;
        str::Free(input);
    }
    VirtText* NewLabel(Str s, PlatformFont* font, Color col, int dy = 0, int dx = 0);
    Edit* NewEdit(Str text, PlatformFont* font, Color col, bool isMultiLine = false);
    void AddArg(Str value, Str hint);
    void SetArgLabel(ScriptArgRow* row, int idx, Str hint);
    void LayoutArgs();
    void ShowOutput(bool show);
    void DrawExample(VirtListBox::DrawItemEvent* ev);
    void OnAdd(VirtMouseEvent* = nullptr);
    void OnRun(VirtMouseEvent* = nullptr);
    void OnExample();
    void OnReset(VirtMouseEvent* = nullptr);
    void OnExamples(VirtMouseEvent* = nullptr);
    void OnHelp(VirtMouseEvent* = nullptr);
    void OnCancel(VirtMouseEvent* = nullptr);
    void OnGetMinMaxInfo(WindowBase::GetMinMaxInfoEvent* ev);
    void PreTranslate(WindowBase::PreTranslateEvent* ev);
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
        // the rows after it are numbered again
        for (int j = i; j < len(wnd->rows); j++) {
            ScriptArgRow* r = wnd->rows[j];
            wnd->SetArgLabel(r, j, r->hint);
        }
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

// `dy`: the label pads itself to that height, and so sets its row's;
// `dx`: the label column's width
VirtText* PDFScriptWnd::NewLabel(Str s, PlatformFont* font, Color col, int dy, int dx) {
    VirtText* t = NewVirtText({.s = s, .font = font, .textColor = col, .isRtl = IsUIRtl(), .ellipsis = true});
    if (dy > 0) {
        int lineDy = PlatformFontLineHeight(font);
        t->padding.top = std::max(0, (DpiScale(dy) - lineDy) / 2);
        t->padding.bottom = std::max(0, DpiScale(dy) - lineDy - t->padding.top);
    }
    if (dx > 0) {
        int textDx = PlatformFontMeasureText(font, s).dx;
        t->padding.right = std::max(0, DpiScale(dx) - textDx);
    }
    return t;
}

// a bare edit, in the dialog's colors
Edit* PDFScriptWnd::NewEdit(Str text, PlatformFont* font, Color col, bool isMultiLine) {
    Edit::CreateArgs args;
    args.parent = hwnd;
    args.withBorder = false;
    args.font = font;
    args.text = text;
    args.isRtl = IsUIRtl();
    args.isMultiLine = isMultiLine;
    args.noWrap = isMultiLine;
    args.noScrollBars = isMultiLine;
    auto* e = new Edit();
    e->SetColors(col, colBg);
    e->Create(args);
    return e;
}

// `s` cut to `maxDx` with an ellipsis; a long hint keeps the label column
// from pushing its field aside (the field's cue text has all of it)
static TempStr FitTextTemp(PlatformFont* font, Str s, int maxDx) {
    if (PlatformFontMeasureText(font, s).dx <= maxDx) return str::DupTemp(s);
    const Str ellipsis = StrL("\xe2\x80\xa6");
    int n = len(s);
    while (n > 0) {
        n--;
        // never inside a UTF-8 sequence
        if (n > 0 && (s.s[n] & 0xc0) == 0x80) continue;
        TempStr cut = str::JoinTemp(Str(s.s, n), ellipsis);
        if (PlatformFontMeasureText(font, cut).dx <= maxDx) return cut;
    }
    return str::DupTemp(ellipsis);
}

// the argument's hint, or its number
void PDFScriptWnd::SetArgLabel(ScriptArgRow* row, int idx, Str hint) {
    if (hint.s != row->hint.s) str::ReplaceWithCopy(&row->hint, hint);
    Str s = len(hint) > 0 ? hint : fmt(Tr("Argument %d").s, idx + 1);
    s = FitTextTemp(fontText, s, DpiScale(kScriptLabelMaxDx));
    row->label->SetText(s);
    int textDx = PlatformFontMeasureText(fontText, s).dx;
    row->label->padding.right = std::max(0, DpiScale(kScriptLabelDx) - textDx);
    row->label->padding.left = 0;
    EditSetCueText(row->edit, hint);
}

// a row: the label, the value over a hairline, Browse... and Remove
void PDFScriptWnd::AddArg(Str value, Str hint) {
    auto* row = new HBox();
    row->alignCross = CrossAxisAlign::CrossCenter;
    row->rtl = IsUIRtl();
    row->gap = DpiScale(kToolDlgGapDx);
    VirtText* label = NewLabel({}, fontText, colMuted, kToolDlgRowDy, kScriptLabelDx);
    label->SetFlag(vwfNoHitTest, true);
    row->AddChild(label);
    Edit* edit = NewEdit(value, fontText, colTxt);
    auto* underlined = new VBox();
    underlined->alignCross = CrossAxisAlign::Stretch;
    underlined->AddChild(edit);
    underlined->AddChild(new VirtLine());
    row->AddChild(underlined, 1);
    auto* arg = new ScriptArgRow{this, label, edit, {}};
    VecAppend(rows, arg);
    SetArgLabel(arg, len(rows) - 1, hint);
    auto* pick = NewDlgButton(hwnd, Tr("Browse..."), fontMeta, DlgButtonStyle::Ghost, false);
    pick->onClick = MkFunc0(PickScriptArg, arg);
    row->AddChild(pick);
    auto* remove = NewDlgButton(hwnd, Tr("Remove"), fontMeta, DlgButtonStyle::Ghost, false);
    remove->onClick = MkFunc0(QueueDeleteScriptArg, arg);
    row->AddChild(remove);
    argsBox->AddChild(row);
}

// the output and the hairline over it take no room until there is some
void PDFScriptWnd::ShowOutput(bool show) {
    output->SetIsVisible(show);
    Visibility v = show ? Visibility::Visible : Visibility::Collapse;
    outputLine->SetVisibility(v);
    outputBox->SetVisibility(v);
    DoLayout();
    InvalidateRect(hwnd, nullptr, FALSE);
}

void PDFScriptWnd::LayoutArgs() {
    DoLayout();
    InvalidateRect(hwnd, nullptr, FALSE);
}

void PDFScriptWnd::OnAdd(VirtMouseEvent*) {
    if (running) return;
    AddArg({}, {});
    LayoutArgs();
    UpdateTheme();
    auto* edit = rows[len(rows) - 1]->edit;
    edit->SetFocus();
    RedrawWindow(edit->hwnd, nullptr, nullptr, RDW_FRAME | RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW);
}

static void ScriptFinished(u64* runId, PDFScriptResult* result) {
    if (gScriptWnd && gScriptWnd->runId == *runId) {
        auto* wnd = gScriptWnd;
        wnd->running = false;
        wnd->run->SetText(Tr("Run Script"));
        SetDlgButtonEnabled(wnd->run, DlgButtonStyle::Filled, true);
        wnd->examples->SetIsEnabled(true);
        wnd->output->SetText(str::LFToCRLFTemp(result->output));
        wnd->ShowOutput(len(result->output) > 0);
    }
    delete runId;
    delete result;
}

void PDFScriptWnd::OnRun(VirtMouseEvent*) {
    if (running || len(script->GetTextTemp()) == 0) return;
    StrVec args;
    for (auto* row : rows) args.Append(row->edit->GetTextTemp());
    running = true;
    runId = ++gScriptRunId;
    run->SetText(Tr("Running..."));
    SetDlgButtonEnabled(run, DlgButtonStyle::Filled, false);
    examples->SetIsEnabled(false);
    output->SetText({});
    ShowOutput(false);
    script->SetFocus();
    StartPDFScript(script->GetTextTemp(), args, MkFunc1(ScriptFinished, new u64(runId)));
}

// the picked example replaces the name, the source and the argument rows
void PDFScriptWnd::OnExample() {
    if (running) return;
    bool listFocused = vroot && vroot->focused == examples;
    int idx = examples->GetCurrentSelection();
    Str source = PDFScriptSource(idx);
    StrVec values;
    StrVec hints;
    GetPDFScriptArgs(source, input, values, hints);

    // Reuse edits and paint only after the new script and rows are laid out.
    bool redraw = IsWindowVisible(hwnd);
    if (redraw) SuspendRedraw();
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
        SetArgLabel(rows[i], i, hints[i]);
        rows[i]->edit->ResumeRedraw();
    }
    if (layout) {
        LayoutArgs();
        if (len(rows) > oldCount) UpdateTheme();
    }
    if (redraw) ResumeRedraw();
    // a bare edit without scrollbars repaints only the lines it had
    InvalidateRect(script->hwnd, nullptr, TRUE);
    if (listFocused) SetFocusTo(examples);
    RedrawWindow(hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_FRAME | RDW_NOERASE);
}

// the dialog as it opened: the first script, its arguments, no output
void PDFScriptWnd::OnReset(VirtMouseEvent*) {
    if (running) return;
    output->SetText({});
    ShowOutput(false);
    if (examples->GetCurrentSelection() != 0) {
        examples->SetCurrentSelection(0);
    }
    OnExample();
    script->SetFocus();
}

// a row of the list: the name, on a rounded selection
void PDFScriptWnd::DrawExample(VirtListBox::DrawItemEvent* ev) {
    Gfx* gfx = ev->gfx;
    Rect rc = ev->itemRect;
    gfx->FillRect(rc, colBg);
    if (ev->selected) {
        gfx->FillRoundedRect(rc, DpiScale(kScriptListRadius), AccentColor(colBg, kScriptListSelShift));
    }
    int inset = DpiScale(kScriptListInsetDx);
    Rect rcText{rc.x + inset, rc.y, rc.dx - 2 * inset, rc.dy};
    u32 fmt = gfxTextEllipsis | gfxTextVCenter;
    fmt |= IsUIRtl() ? (gfxTextRight | gfxTextRtl) : gfxTextLeft;
    gfx->DrawText(PDFScriptName(ev->itemIndex), rcText, fmt, fontText, colTxt);
}

// the list beside the editor; the window grows and shrinks with it
void PDFScriptWnd::OnExamples(VirtMouseEvent*) {
    bool show = examplesBox->GetVisibility() == Visibility::Collapse;
    Visibility v = show ? Visibility::Visible : Visibility::Collapse;
    examplesBox->SetVisibility(v);
    examplesLine->SetVisibility(v);
    Rect rect = HwndWindowRect(hwnd);
    int extra = DpiScale(kScriptListDx + 2 * kScriptListInsetDx) + examplesLine->thickness;
    SetWindowPos(hwnd, nullptr, 0, 0, rect.dx + (show ? extra : -extra), rect.dy, SWP_NOMOVE | SWP_NOZORDER);
    HwndEnsureOnScreen(hwnd);
    DoLayout();
    InvalidateRect(hwnd, nullptr, TRUE);
    // the keys go back to the editor, not to the button
    script->SetFocus();
}

void PDFScriptWnd::OnHelp(VirtMouseEvent*) {
    LaunchDocumentation(StrL(kPDFScriptHelpDocUri));
}

void PDFScriptWnd::OnCancel(VirtMouseEvent*) {
    ScheduleDelete();
}

void PDFScriptWnd::OnGetMinMaxInfo(WindowBase::GetMinMaxInfoEvent* ev) {
    if (!ev->mmi || !hwnd) return;
    Rect wr = HwndWindowRect(hwnd);
    Rect cr = HwndClientRect(hwnd);
    int minDx = kToolDlgMinDx;
    if (examplesBox && examplesBox->GetVisibility() != Visibility::Collapse) {
        minDx += kScriptListDx + 2 * kScriptListInsetDx;
    }
    ev->mmi->ptMinTrackSize.x = DpiScale(minDx) + wr.dx - cr.dx;
    ev->mmi->ptMinTrackSize.y = DpiScale(kScriptDlgMinDy) + wr.dy - cr.dy;
}

// Ctrl+Enter runs the script from anywhere in the dialog
void PDFScriptWnd::PreTranslate(WindowBase::PreTranslateEvent* ev) {
    MSG& msg = *ev->msg;
    if (msg.message != WM_KEYDOWN || msg.wParam != VK_RETURN || !IsCtrlPressed()) return;
    OnRun();
    ev->didHandle = true;
}

void PDFScriptWnd::Create(MainWindow* owner, Str path) {
    win = owner;
    input = str::Dup(path);
    colTxt = ThemeWindowTextColor();
    colBg = ThemeWindowControlBackgroundColor();
    colMuted = ThemeWindowTextDisabledColor();
    fontText = DlgUiFont(kToolDlgTextFontPx);
    fontMeta = DlgUiFont(kToolDlgMetaFontPx);
    fontCode = GetUserGuiFont(StrL("Consolas"), DpiScale(kScriptCodeFontPx));
    fontOutput = GetUserGuiFont(StrL("Consolas"), DpiScale(kScriptOutputFontPx));
    SetFont(fontText);
    CreateCustomArgs ca;
    ca.owner = win->hwndFrame;
    ca.title = Tr("Run PDF Script");
    ca.font = fontText;
    ca.icon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(GetAppIconID()));
    ca.style = WS_OVERLAPPEDWINDOW;
    ca.visible = false;
    ca.bgColor = colBg;
    onGetMinMaxInfo = MkMethod1<PDFScriptWnd, WindowBase::GetMinMaxInfoEvent*, &PDFScriptWnd::OnGetMinMaxInfo>(this);
    onPreTranslate = MkMethod1<PDFScriptWnd, WindowBase::PreTranslateEvent*, &PDFScriptWnd::PreTranslate>(this);
    CreateCustom(ca);
    if (!hwnd) return;
    SetColors(colTxt, colBg);
    bool isRtl = IsUIRtl();

    auto* body = new VBox();
    body->alignCross = CrossAxisAlign::Stretch;

    // the arguments, then the quiet "Add argument" under them
    auto* argsBlock = new VBox();
    argsBlock->alignCross = CrossAxisAlign::Stretch;
    argsBox = new VBox();
    argsBox->alignCross = CrossAxisAlign::Stretch;
    argsBlock->AddChild(argsBox);
    auto* addRow = new HBox();
    addRow->alignCross = CrossAxisAlign::CrossCenter;
    addRow->rtl = isRtl;
    auto* add = NewDlgButton(hwnd, Tr("Add argument"), fontMeta, DlgButtonStyle::Ghost, false);
    add->onClick = MkMethod1<PDFScriptWnd, VirtMouseEvent*, &PDFScriptWnd::OnAdd>(this);
    // lines up with the fields; the button pads its own text by 8
    addRow->AddChild(new Spacer(DpiScale(kScriptLabelDx + kToolDlgGapDx) - add->textPadding.left, 0));
    addRow->AddChild(add);
    argsBlock->AddChild(addRow);
    body->AddChild(new Padding(argsBlock, DpiScaledInsets(kToolDlgFilesPadDy, kToolDlgPadDx)));
    body->AddChild(new VirtLine());

    // the source takes what the window has left
    script = NewEdit({}, fontCode, colTxt, true);
    body->AddChild(new Padding(script, DpiScaledInsets(kToolDlgFilesPadDy, kToolDlgPadDx)), 1);

    // the output, muted, once a run produced some
    outputLine = new VirtLine();
    outputLine->SetVisibility(Visibility::Collapse);
    body->AddChild(outputLine);
    output = NewEdit({}, fontOutput, colMuted, true);
    SendMessageW(output->hwnd, EM_SETREADONLY, TRUE, 0);
    output->idealSizeLines = kScriptOutputLines;
    output->SetIsVisible(false);
    outputBox = new Padding(output, DpiScaledInsets(kToolDlgFilesPadDy, kToolDlgPadDx));
    outputBox->SetVisibility(Visibility::Collapse);
    body->AddChild(outputBox);
    body->AddChild(new VirtLine());

    // the footer: the key hint and the quiet buttons on the left, Cancel and
    // Run on the right
    auto* footer = new HBox();
    footer->alignCross = CrossAxisAlign::CrossCenter;
    footer->rtl = isRtl;
    footer->gap = DpiScale(kToolDlgGapDx);
    footer->AddChild(NewKeyHint(fontMeta, StrL("Ctrl+Enter"), Tr("Run Script"), colMuted, colBg));
    auto* showExamples = NewDlgButton(hwnd, Tr("Example Scripts"), fontText, DlgButtonStyle::Ghost, false);
    showExamples->onClick = MkMethod1<PDFScriptWnd, VirtMouseEvent*, &PDFScriptWnd::OnExamples>(this);
    footer->AddChild(showExamples);
    auto* help = NewDlgButton(hwnd, Tr("Help"), fontText, DlgButtonStyle::Ghost, false);
    help->onClick = MkMethod1<PDFScriptWnd, VirtMouseEvent*, &PDFScriptWnd::OnHelp>(this);
    footer->AddChild(help);
    auto* reset = NewDlgButton(hwnd, Tr("Reset"), fontText, DlgButtonStyle::Ghost, false);
    reset->onClick = MkMethod1<PDFScriptWnd, VirtMouseEvent*, &PDFScriptWnd::OnReset>(this);
    footer->AddChild(reset);
    footer->AddChild(new Spacer(0, 0), 1);
    auto* cancel = NewDlgButton(hwnd, Tr("Cancel"), fontText, DlgButtonStyle::Outline, false);
    cancel->onClick = MkMethod1<PDFScriptWnd, VirtMouseEvent*, &PDFScriptWnd::OnCancel>(this);
    footer->AddChild(cancel);
    run = NewDlgButton(hwnd, Tr("Run Script"), fontText, DlgButtonStyle::Filled, true);
    run->onClick = MkMethod1<PDFScriptWnd, VirtMouseEvent*, &PDFScriptWnd::OnRun>(this);
    footer->AddChild(run);
    body->AddChild(new Padding(footer, DpiScaledInsets(kToolDlgFootPadDy, kToolDlgPadDx)));

    // the list of the included scripts, past a vertical hairline; hidden
    // until Example Scripts
    examples = new VirtListBox();
    examples->font = fontText;
    examples->dpi = GetDpi();
    examples->itemDy = DpiScale(kScriptListRowDy);
    examples->idealSizeDx = DpiScale(kScriptListDx);
    examples->SetColor(kColListText, colTxt);
    examples->SetColor(kColListBg, colBg);
    examples->onDrawItem = MkMethod1<PDFScriptWnd, VirtListBox::DrawItemEvent*, &PDFScriptWnd::DrawExample>(this);
    auto* model = new ListBoxModelStrings();
    for (int i = 0; i < PDFScriptCount(); i++) model->strings.Append(PDFScriptName(i));
    examples->SetModel(model);
    examples->SetCurrentSelection(0);
    examples->onSelectionChanged = MkMethod0<PDFScriptWnd, &PDFScriptWnd::OnExample>(this);
    examplesLine = new VirtLine();
    examplesLine->isVertical = true;
    examplesLine->SetVisibility(Visibility::Collapse);
    examplesBox = new Padding(examples, DpiScaledInsets(kScriptListInsetDx));
    examplesBox->SetVisibility(Visibility::Collapse);

    auto* columns = new HBox();
    columns->alignCross = CrossAxisAlign::Stretch;
    columns->rtl = isRtl;
    columns->AddChild(body, 1);
    columns->AddChild(examplesLine);
    columns->AddChild(examplesBox);
    layout = columns;

    OnExample();
    ResizeHwndToClientArea(hwnd, DpiScale(kScriptDlgDx), DpiScale(kScriptDlgDy), false);
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
