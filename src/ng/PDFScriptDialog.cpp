/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "gui/GpuiBridge.h"
#include "base/UITask.h"
#include "gui/UIModels.h"
#include "Settings.h"
#include "Theme.h"
#include "DocController.h"
#include "EngineBase.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "SumatraPDF.h"
#include "Translations.h"
#include "gui/AppShell.h"
#include "gui/DialogWidgets.h"
#include "gui/ToolWindow.h"
#include "PDFScript.h"
#include "shared/PdfToolsCommon.h"

struct ScriptArg {
    int id;
    gp::InputState* input;
    Str hint;
};

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
constexpr float kScriptLabelDx = 96;
constexpr float kScriptLabelMaxDx = 240;
constexpr float kScriptListDx = 300;
constexpr float kScriptListRowDy = 28;
constexpr float kScriptListInsetDx = 8;
constexpr float kScriptListRadius = 4;
constexpr int kScriptListSelShift = 28;
constexpr float kScriptCodeFontPx = 14;
constexpr float kScriptOutputFontPx = 13;
constexpr float kScriptOutputLineDy = 18;
// gpui's inputs draw their text at UiInputFontPx(size), 0.875 of a custom size
constexpr float kUiSizePerFontPx = 1 / 0.875f;
constexpr int kScriptOutputLines = 7;
constexpr float kScriptEditorMinDy = 80;
constexpr float kScriptFrameEditorDy = 300; // in the frame, where the window cannot grow

static struct PDFScriptDlg {
    MainWindow* win = nullptr;
    ToolWindow* tw = nullptr;
    gp::InputState* script = nullptr;
    gp::InputState* output = nullptr;
    Str input;
    bool showExamples = false;
    int example = 0;
    float exampleScrollY = 0;
    Vec<ScriptArg> args;
    bool visible = false;
    bool running = false;
    bool wantFocus = false;
    int nextArg = 0;
    u64 session = 0;
} gScript;

static u64 gScriptSession = 0;

struct PDFScriptView {
    static void OnAdd(PDFScriptView*, gp::Ctx*, const gp::ClickEvent*);
    static void OnDelete(PDFScriptView*, gp::Ctx*, const gp::ClickEvent*, int64_t);
    static void OnPick(PDFScriptView*, gp::Ctx*, const gp::ClickEvent*, int64_t);
    static void OnRun(PDFScriptView*, gp::Ctx*, const gp::ClickEvent*);
    static void OnExamples(PDFScriptView*, gp::Ctx*, const gp::ClickEvent*);
    static void OnExample(PDFScriptView*, gp::Ctx*, const gp::ClickEvent*, int64_t);
    static void OnReset(PDFScriptView*, gp::Ctx*, const gp::ClickEvent*);
    static void OnScroll(PDFScriptView*, gp::Ctx*, const gp::ScrollEvent*);
    static void OnHelp(PDFScriptView*, gp::Ctx*, const gp::ClickEvent*);
    static void OnCancel(PDFScriptView*, gp::Ctx*, const gp::ClickEvent*);
};
static gp::Entity<PDFScriptView> gScriptView;

gp::El* PDFScriptDialogBuild(MainWindow* win, gp::Ctx* cx);

bool IsPDFScriptDialogVisible() {
    return gScript.visible && !gScript.tw;
}

static gp::Window* ScriptHost() {
    return gScript.tw ? ToolWindowGpui(gScript.tw) : gScript.win ? gScript.win->gpuiWin : nullptr;
}

static void ScriptInvalidate() {
    if (gScript.tw) ToolWindowInvalidate(gScript.tw);
    if (IsMainWindowValid(gScript.win)) AppShellInvalidate(gScript.win);
}

void ClosePDFScriptDialog() {
    if (!gScript.visible) return;
    gScript.visible = false;
    gp::Window* host = ScriptHost();
    if (host && host->input) gp::InputBlur(host->input, host->app, host);
    if (gScript.tw) {
        ToolWindow* tw = gScript.tw;
        gScript.tw = nullptr;
        ToolWindowClose(tw);
    }
    delete gScript.script;
    delete gScript.output;
    gScript.script = gScript.output = nullptr;
    for (auto& arg : gScript.args) {
        delete arg.input;
        str::Free(arg.hint);
    }
    VecClear(gScript.args);
    str::Free(gScript.input);
    gScript.input = {};
    ++gScriptSession;
    if (IsMainWindowValid(gScript.win)) AppShellInvalidate(gScript.win);
    gScript.win = nullptr;
}

static gp::InputState* ScriptInput(gp::App* app, Str text, gp::InputKind kind = gp::InputKind::Input) {
    auto* input = new gp::InputState();
    input->focus = gp::FocusHandleNew(app);
    input->kind = kind;
    gp::InputSetValue(input, ToGpui(text));
    return input;
}

static Str ScriptTitle() {
    return Tr("Run PDF Script");
}

static void LoadScriptArgs(gp::App* app) {
    gp::Window* host = ScriptHost();
    StrVec values;
    StrVec hints;
    GetPDFScriptArgs(PDFScriptSource(gScript.example), gScript.input, values, hints);
    while (len(gScript.args) > len(values)) {
        auto& arg = gScript.args[len(gScript.args) - 1];
        gp::InputBlur(arg.input, app, host);
        delete arg.input;
        str::Free(arg.hint);
        VecRemoveAt(gScript.args, len(gScript.args) - 1);
    }
    for (int i = 0; i < len(values); i++) {
        if (i >= len(gScript.args)) {
            VecAppend(gScript.args, ScriptArg{++gScript.nextArg, ScriptInput(app, {}), {}});
        }
        auto& arg = gScript.args[i];
        gp::InputSetValue(arg.input, ToGpui(values[i]));
        gp::InputSetPlaceholder(arg.input, ToGpui(hints[i]));
        str::ReplaceWithCopy(&arg.hint, hints[i]);
    }
}

static void RunScript();

// Esc closes, Ctrl+Enter runs
static bool ScriptToolOnKey(MainWindow* win, gp::Ctx* cx, const gp::KeyEvent* ev) {
    if (!gScript.visible || gScript.win != win) return false;
    if (ev->vk == VK_ESCAPE) {
        ClosePDFScriptDialog();
        return true;
    }
    if (ev->vk == VK_RETURN && ev->ctrl) {
        RunScript();
        gp::Notify(cx);
        return true;
    }
    return false;
}

// the close box, Alt+F4 or the main window going
static void ScriptToolOnClosed(MainWindow*) {
    gScript.tw = nullptr;
    ClosePDFScriptDialog();
}

static ToolWindowDesc ScriptToolDesc() {
    ToolWindowDesc desc;
    desc.name = "pdfscript";
    desc.title = ScriptTitle;
    desc.frame = ToolWinFrame::Caption;
    desc.resize = ToolWinResize::Resizable;
    desc.owner = ToolWinOwner::Owned;
    desc.minClient = Size(kToolDlgMinDx, kScriptDlgMinDy);
    desc.build = PDFScriptDialogBuild;
    desc.onKey = ScriptToolOnKey;
    desc.onClosed = ScriptToolOnClosed;
    return desc;
}

void ShowPDFScriptDialog(MainWindow* win) {
    auto* tab = win ? win->CurrentTab() : nullptr;
    if (!CanAccessDisk() || !tab || !tab->GetEngine() || !str::EqI(tab->GetEngine()->defaultExt, StrL(".pdf"))) return;
    if (gScript.visible) {
        if (gScript.tw) ToolWindowActivate(gScript.tw);
        return;
    }
    gp::App* app = win->gpuiWin->app;
    gScript.win = win;
    gScript.input = str::Dup(tab->filePath);
    gScript.visible = true;
    gScript.running = false;
    gScript.showExamples = false;
    gScript.example = 0;
    gScript.exampleScrollY = 0;
    gScript.wantFocus = true;
    gScript.session = ++gScriptSession;
    LoadScriptArgs(app);
    gScript.script = ScriptInput(app, PDFScriptSource(0), gp::InputKind::Textarea);
    gScript.output = ScriptInput(app, {}, gp::InputKind::Textarea);
    if (ToolWindowsAvailable()) {
        ToolWindowDesc desc = ScriptToolDesc();
        gScript.tw = ToolWindowOpen(desc, win, ToolWindowCenteredRect(desc, win, Size(kScriptDlgDx, kScriptDlgDy)));
    }
    AppShellInvalidate(win);
}

void PDFScriptView::OnAdd(PDFScriptView*, gp::Ctx* cx, const gp::ClickEvent*) {
    if (gScript.running) return;
    VecAppend(gScript.args, ScriptArg{++gScript.nextArg, ScriptInput(cx->app, {}), {}});
    gp::InputFocus(gScript.args[len(gScript.args) - 1].input, cx->app, cx->win);
}

void PDFScriptView::OnDelete(PDFScriptView*, gp::Ctx* cx, const gp::ClickEvent*, int64_t id) {
    if (gScript.running) return;
    for (int i = 0; i < len(gScript.args); i++) {
        auto& arg = gScript.args[i];
        if (arg.id != id) continue;
        gp::InputBlur(arg.input, cx->app, cx->win);
        delete arg.input;
        str::Free(arg.hint);
        VecRemoveAt(gScript.args, i);
        return;
    }
}

struct ScriptPickCtx {
    u64 session;
    int id;
};

static void ScriptPicked(ScriptPickCtx* ctx, Str path) {
    if (gScript.visible && gScript.session == ctx->session && len(path) > 0) {
        for (auto& arg : gScript.args) {
            if (arg.id == ctx->id) gp::InputSetValue(arg.input, ToGpui(path));
        }
        ScriptInvalidate();
    }
    delete ctx;
}

void PDFScriptView::OnPick(PDFScriptView*, gp::Ctx*, const gp::ClickEvent*, int64_t id) {
    if (gScript.running) return;
    auto* ctx = new ScriptPickCtx{gScript.session, (int)id};
    if (!AppShellPickFileAsync(gScript.win, Tr("Select file"), StrL("*.*"), MkFunc1(ScriptPicked, ctx))) {
        ScriptPicked(ctx, AppShellPromptForPathTemp(gScript.win, Tr("Select file"), StrL("*.*")));
    }
}

static void ScriptFinished(u64* session, PDFScriptResult* result) {
    if (gScript.visible && gScript.session == *session) {
        gScript.running = false;
        gp::InputSetValue(gScript.output, ToGpui(result->output));
        ScriptInvalidate();
    }
    delete session;
    delete result;
}

static void RunScript() {
    if (gScript.running || len(FromGpui(gp::InputValue(gScript.script))) == 0) return;
    StrVec args;
    for (auto& arg : gScript.args) args.Append(FromGpui(gp::InputValue(arg.input)));
    gScript.running = true;
    gp::InputSetValue(gScript.output, GStrL(""));
    StartPDFScript(FromGpui(gp::InputValue(gScript.script)), args, MkFunc1(ScriptFinished, new u64(gScript.session)));
}

void PDFScriptView::OnRun(PDFScriptView*, gp::Ctx*, const gp::ClickEvent*) {
    RunScript();
}

void PDFScriptView::OnHelp(PDFScriptView*, gp::Ctx*, const gp::ClickEvent*) {
    LaunchDocumentation(StrL(kPDFScriptHelpDocUri));
}

// the list beside the editor; the window grows and shrinks with it
void PDFScriptView::OnExamples(PDFScriptView*, gp::Ctx* cx, const gp::ClickEvent*) {
    gScript.showExamples = !gScript.showExamples;
    if (gScript.tw) {
        gp::WinSize ws = gp::WindowSize(cx->win);
        float extra = kScriptListDx + 2 * kScriptListInsetDx + 1;
        float dx = ws.dipW + (gScript.showExamples ? extra : -extra);
        ToolWindowSetClientSize(gScript.tw, Size((int)(dx + 0.5f), (int)(ws.dipH + 0.5f)));
    }
    gp::Notify(cx);
}

// the picked example replaces the name, the source and the argument rows
void PDFScriptView::OnExample(PDFScriptView*, gp::Ctx* cx, const gp::ClickEvent*, int64_t index) {
    if (gScript.running) return;
    gScript.example = (int)index;
    gp::InputSetValue(gScript.script, ToGpui(PDFScriptSource(gScript.example)));
    LoadScriptArgs(cx->app);
    gp::Notify(cx);
}

// the dialog as it opened: the first script, its arguments, no output
void PDFScriptView::OnReset(PDFScriptView*, gp::Ctx* cx, const gp::ClickEvent*) {
    if (gScript.running) return;
    gp::InputSetValue(gScript.output, GStrL(""));
    gScript.exampleScrollY = 0;
    OnExample(nullptr, cx, nullptr, 0);
}

void PDFScriptView::OnScroll(PDFScriptView*, gp::Ctx* cx, const gp::ScrollEvent* ev) {
    gScript.exampleScrollY = ev->offsetY;
    gp::Notify(cx);
}

void PDFScriptView::OnCancel(PDFScriptView*, gp::Ctx*, const gp::ClickEvent*) {
    ClosePDFScriptDialog();
}

static gp::El* ScriptText(gp::Ctx* cx, Str s, float font, Color col) {
    return gp::TextEl(cx->a, GpuiDup(cx->a, s))->Font(font)->Fg(ToGpui(col));
}

// a long hint is cut with an ellipsis, so it does not push its field aside
// (the field's placeholder has all of it)
static gp::El* ScriptLabel(gp::Ctx* cx, Str label) {
    return gp::Div(cx->a)
        ->MinW(kScriptLabelDx)
        ->MaxW(kScriptLabelMaxDx)
        ->Shrink0()
        ->Child(ScriptText(cx, label, kToolDlgTextFontPx, ThemeWindowTextDisabledColor())->Truncate()->MinW(0));
}

static gp::El* ScriptGhostButton(gp::Ctx* cx, Str id, Str label, gp::Listener onClick, bool disabled = false) {
    return gpc::Button::New(cx, GpuiDup(cx->a, id))
        ->Label(GpuiDup(cx->a, label))
        ->Ghost()
        ->WithSize(gp::UiSize::Small)
        ->Disabled(disabled)
        ->OnClick(onClick)
        ->IntoEl();
}

static gp::El* ScriptCodeBox(gp::Ctx* cx, gp::El* editor) {
    return gp::Div(cx->a)->W(gp::kFill)->Shrink0()->PadX(kToolDlgPadDx)->PadY(kToolDlgFilesPadDy)->Child(editor);
}

// the "Script" line, the arguments, the editor, the output, the footer
static gp::El* ScriptColumn(gp::Ctx* cx, float editorDy) {
    Color colMuted = ThemeWindowTextDisabledColor();
    gp::El* col = gp::Div(cx->a)->FlexCol()->Flex1()->MinW(0)->Shrink0();

    gp::El* args = gp::Div(cx->a)->FlexCol()->W(gp::kFill)->Shrink0()->PadX(kToolDlgPadDx)->PadY(kToolDlgFilesPadDy);
    for (int i = 0; i < len(gScript.args); i++) {
        auto& arg = gScript.args[i];
        Str label = len(arg.hint) > 0 ? arg.hint : fmt(Tr("Argument %d").s, i + 1);
        gp::El* row =
            gp::Div(cx->a)->FlexRow()->ItemsCenter()->W(gp::kFill)->H(kToolDlgRowDy)->Shrink0()->Gap(kToolDlgGapDx);
        row->Child(ScriptLabel(cx, label));
        row->Child(DlgUnderlineInput(cx, ToGpui(fmt("pdf-arg-%d", arg.id)), arg.input, kToolDlgTextFontPx,
                                     ThemeEdgeColor(), false, gScript.running)
                       ->Flex1()
                       ->MinW(0));
        row->Child(ScriptGhostButton(cx, fmt("pdf-pick-%d", arg.id), Tr("Browse..."),
                                     gp::ListenTo(gScriptView, &PDFScriptView::OnPick, arg.id), gScript.running));
        row->Child(ScriptGhostButton(cx, fmt("pdf-delete-%d", arg.id), Tr("Remove"),
                                     gp::ListenTo(gScriptView, &PDFScriptView::OnDelete, arg.id), gScript.running));
        args->Child(row);
    }
    // "Add argument" lines up with the fields
    args->Child(gp::Div(cx->a)
                    ->FlexRow()
                    ->ItemsCenter()
                    ->W(gp::kFill)
                    ->H(kToolDlgRowDy)
                    ->Shrink0()
                    ->PadL(kScriptLabelDx + kToolDlgGapDx - kDlgBtnPadDx)
                    ->Child(ScriptGhostButton(cx, StrL("pdf-add-arg"), Tr("Add argument"),
                                              gp::ListenTo(gScriptView, &PDFScriptView::OnAdd), gScript.running)));
    col->Child(args);
    col->Child(DlgHairline(cx));

    gp::El* editor = gpc::Textarea::New(cx, GStrL("pdf-script-source"), gScript.script)
                         ->WithSize(gp::UiSize::Custom(kScriptCodeFontPx * kUiSizePerFontPx))
                         ->H(editorDy)
                         ->SoftWrap(false)
                         ->Appearance(false)
                         ->Bordered(false)
                         ->FocusRing(false)
                         ->IntoEl()
                         ->Mono()
                         ->Font(kScriptCodeFontPx);
    col->Child(ScriptCodeBox(cx, editor));

    if (len(FromGpui(gp::InputValue(gScript.output))) > 0) {
        col->Child(DlgHairline(cx));
        gp::El* out = gpc::Textarea::New(cx, GStrL("pdf-script-output"), gScript.output)
                          ->WithSize(gp::UiSize::Custom(kScriptOutputFontPx * kUiSizePerFontPx))
                          ->H(kScriptOutputLines * kScriptOutputLineDy)
                          ->SoftWrap(false)
                          ->Readonly()
                          ->Appearance(false)
                          ->Bordered(false)
                          ->FocusRing(false)
                          ->IntoEl()
                          ->Mono()
                          ->Font(kScriptOutputFontPx)
                          ->Fg(ToGpui(colMuted));
        col->Child(ScriptCodeBox(cx, out));
    }
    col->Child(DlgHairline(cx));

    gp::El* footer = gp::Div(cx->a)
                         ->FlexRow()
                         ->ItemsCenter()
                         ->W(gp::kFill)
                         ->Shrink0()
                         ->PadX(kToolDlgPadDx)
                         ->PadY(kToolDlgFootPadDy)
                         ->Gap(kToolDlgGapDx);
    footer->Child(gp::Div(cx->a)
                      ->FlexRow()
                      ->ItemsCenter()
                      ->Gap(4)
                      ->Shrink0()
                      ->Child(DlgKeyCap(cx, StrL("Ctrl+Enter")))
                      ->Child(ScriptText(cx, Tr("Run Script"), kToolDlgMetaFontPx, colMuted)));
    footer->Child(ScriptGhostButton(cx, StrL("pdf-script-examples"), Tr("Example Scripts"),
                                    gp::ListenTo(gScriptView, &PDFScriptView::OnExamples)));
    footer->Child(
        ScriptGhostButton(cx, StrL("pdf-script-help"), Tr("Help"), gp::ListenTo(gScriptView, &PDFScriptView::OnHelp)));
    footer->Child(ScriptGhostButton(cx, StrL("pdf-reset"), Tr("Reset"),
                                    gp::ListenTo(gScriptView, &PDFScriptView::OnReset), gScript.running));
    footer->Child(gp::Div(cx->a)->Flex1());
    footer->Child(gpc::Button::New(cx, GStrL("pdf-script-cancel"))
                      ->Label(ToGpui(Tr("Cancel")))
                      ->Outline()
                      ->WithSize(gp::UiSize::Small)
                      ->OnClick(gp::ListenTo(gScriptView, &PDFScriptView::OnCancel))
                      ->IntoEl());
    footer->Child(DlgFilledButton(cx, GStrL("pdf-script-run"), gScript.running ? Tr("Running...") : Tr("Run Script"),
                                  gp::ListenTo(gScriptView, &PDFScriptView::OnRun), gScript.running));
    col->Child(footer);
    return col;
}

// the included scripts, with a rounded selection like the palette's
static gp::El* ScriptExamplesList(gp::Ctx* cx) {
    Color colText = ThemeWindowTextColor();
    Color colBg = ThemeWindowControlBackgroundColor();
    gp::El* list = gp::Div(cx->a)
                       ->Id(GStrL("pdf-script-list"))
                       ->Role(gp::AccessibilityRole::List)
                       ->AriaLabel(ToGpui(Tr("Example Scripts")))
                       ->W(kScriptListDx)
                       ->Shrink0()
                       ->FlexCol()
                       ->Pad(kScriptListInsetDx)
                       ->ClipY()
                       ->ScrollY(gScript.exampleScrollY)
                       ->ScrollFromPath()
                       ->OnScroll(gp::ListenTo(gScriptView, &PDFScriptView::OnScroll));
    for (int i = 0; i < PDFScriptCount(); i++) {
        gp::El* row = gp::Div(cx->a)
                          ->FlexRow()
                          ->Role(gp::AccessibilityRole::ListItem)
                          ->AriaLabel(ToGpui(PDFScriptName(i)))
                          ->W(gp::kFill)
                          ->H(kScriptListRowDy)
                          ->Shrink0()
                          ->ItemsCenter()
                          ->PadX(kScriptListInsetDx)
                          ->Radius(kScriptListRadius)
                          ->Cursor(gp::CursorKind::Pointer)
                          ->PathClick(GpuiDup(cx->a, fmt("pdf-script-row-%d", i)))
                          ->OnClick(gp::ListenTo(gScriptView, &PDFScriptView::OnExample, (intptr_t)i));
        if (i == gScript.example) row->Bg(ToGpui(AccentColor(colBg, kScriptListSelShift)));
        row->Child(ScriptText(cx, PDFScriptName(i), kToolDlgTextFontPx, colText)->Truncate()->Flex1()->MinW(0));
        list->Child(row);
    }
    return list;
}

// the column and, past a vertical hairline, the list
static gp::El* ScriptColumns(gp::Ctx* cx, float editorDy) {
    gp::El* columns =
        gp::Div(cx->a)->FlexRow()->ItemsStretch()->W(gp::kFill)->Shrink0()->Child(ScriptColumn(cx, editorDy));
    if (gScript.showExamples) {
        columns->Child(gp::Div(cx->a)->W(1)->Shrink0()->Bg(ToGpui(ThemeEdgeColor())));
        columns->Child(ScriptExamplesList(cx));
    }
    return columns;
}

// what the column takes besides the editor, so the editor gets the rest
static float ScriptFixedDy() {
    float dy = 2 * kToolDlgFilesPadDy + (float)(len(gScript.args) + 1) * kToolDlgRowDy + 1;
    dy += 2 * kToolDlgFilesPadDy;
    if (len(FromGpui(gp::InputValue(gScript.output))) > 0) {
        dy += 1 + 2 * kToolDlgFilesPadDy + kScriptOutputLines * kScriptOutputLineDy;
    }
    dy += 1 + 2 * kToolDlgFootPadDy + kDlgBtnDy;
    return dy;
}

gp::El* PDFScriptDialogBuild(MainWindow* win, gp::Ctx* cx) {
    if (!gScript.visible || gScript.win != win || (gScript.tw && cx->win != ToolWindowGpui(gScript.tw))) return nullptr;
    if (!gScriptView.IsValid()) gScriptView = gp::EntityNewState<PDFScriptView>(cx->app);

    gp::El* result;
    if (gScript.tw) {
        gp::WinSize ws = gp::WindowSize(cx->win);
        float editorDy = std::max(kScriptEditorMinDy, ws.dipH - ScriptFixedDy());
        result = gp::Div(cx->a)->FlexCol()->SizeFull()->Child(ScriptColumns(cx, editorDy));
    } else {
        float dx = kScriptDlgDx + (gScript.showExamples ? kScriptListDx + 2 * kScriptListInsetDx + 1 : 0);
        auto* dlg = gpc::Dialog::New(cx)
                        ->Open(true)
                        ->Title(ToGpui(ScriptTitle()))
                        ->Body(ScriptColumns(cx, kScriptFrameEditorDy))
                        ->Footer(gp::Div(cx->a)->H(0))
                        ->W(dx)
                        ->OnClose(gp::ListenTo(gScriptView, &PDFScriptView::OnCancel));
        result = DlgIntoEl(cx, dlg);
    }
    if (gScript.wantFocus) {
        gp::InputFocus(gScript.script, cx->app, cx->win);
        gScript.wantFocus = cx->win->input != gScript.script;
    }
    return result;
}
