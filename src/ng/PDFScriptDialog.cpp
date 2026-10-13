/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "gui/GpuiBridge.h"
#include "base/UITask.h"
#include "gui/UIModels.h"
#include "Settings.h"
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

struct ScriptArg {
    int id;
    gp::InputState* input;
    Str hint;
};

constexpr float kScriptListWidth = 360;
constexpr float kScriptColumnGap = 6;
constexpr float kScriptListRowHeight = 28;

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

void ClosePDFScriptDialog() {
    if (!gScript.visible) return;
    gScript.visible = false;
    gp::Window* host = ScriptHost();
    if (host && host->input) gp::InputBlur(host->input, host->app, host);
    DlgWindowClose(&gScript.tw);
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
    DlgWindowSpec spec;
    spec.name = "pdfscript";
    spec.title = ScriptTitle;
    spec.build = PDFScriptDialogBuild;
    spec.close = ClosePDFScriptDialog;
    gScript.tw = DlgWindowOpen(spec, win);
    AppShellInvalidate(win);
}

void PDFScriptView::OnAdd(PDFScriptView*, gp::Ctx* cx, const gp::ClickEvent*) {
    VecAppend(gScript.args, ScriptArg{++gScript.nextArg, ScriptInput(cx->app, {}), {}});
    gp::InputFocus(gScript.args[len(gScript.args) - 1].input, cx->app, cx->win);
}

void PDFScriptView::OnDelete(PDFScriptView*, gp::Ctx* cx, const gp::ClickEvent*, int64_t id) {
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
        if (gScript.tw) ToolWindowInvalidate(gScript.tw);
        AppShellInvalidate(gScript.win);
    }
    delete ctx;
}

void PDFScriptView::OnPick(PDFScriptView*, gp::Ctx*, const gp::ClickEvent*, int64_t id) {
    auto* ctx = new ScriptPickCtx{gScript.session, (int)id};
    if (!AppShellPickFileAsync(gScript.win, Tr("Select file"), StrL("*.*"), MkFunc1(ScriptPicked, ctx))) {
        ScriptPicked(ctx, AppShellPromptForPathTemp(gScript.win, Tr("Select file"), StrL("*.*")));
    }
}

static void ScriptFinished(u64* session, PDFScriptResult* result) {
    if (gScript.visible && gScript.session == *session) {
        gScript.running = false;
        gp::InputSetValue(gScript.output, ToGpui(result->output));
        if (gScript.tw) ToolWindowInvalidate(gScript.tw);
        AppShellInvalidate(gScript.win);
    }
    delete session;
    delete result;
}

void PDFScriptView::OnRun(PDFScriptView*, gp::Ctx*, const gp::ClickEvent*) {
    if (gScript.running || len(FromGpui(gp::InputValue(gScript.script))) == 0) return;
    StrVec args;
    for (auto& arg : gScript.args) args.Append(FromGpui(gp::InputValue(arg.input)));
    gScript.running = true;
    gp::InputSetValue(gScript.output, GStrL(""));
    StartPDFScript(FromGpui(gp::InputValue(gScript.script)), args, MkFunc1(ScriptFinished, new u64(gScript.session)));
}

void PDFScriptView::OnHelp(PDFScriptView*, gp::Ctx*, const gp::ClickEvent*) {
    LaunchDocumentation(StrL(kPDFScriptHelpDocUri));
}

void PDFScriptView::OnExamples(PDFScriptView*, gp::Ctx* cx, const gp::ClickEvent*) {
    gScript.showExamples = !gScript.showExamples;
    gp::Notify(cx);
}

void PDFScriptView::OnExample(PDFScriptView*, gp::Ctx* cx, const gp::ClickEvent*, int64_t index) {
    if (gScript.running) return;
    gScript.example = (int)index;
    gp::InputSetValue(gScript.script, ToGpui(PDFScriptSource(gScript.example)));
    LoadScriptArgs(cx->app);
    gp::Notify(cx);
}

void PDFScriptView::OnScroll(PDFScriptView*, gp::Ctx* cx, const gp::ScrollEvent* ev) {
    gScript.exampleScrollY = ev->offsetY;
    gp::Notify(cx);
}

void PDFScriptView::OnCancel(PDFScriptView*, gp::Ctx*, const gp::ClickEvent*) {
    ClosePDFScriptDialog();
}

gp::El* PDFScriptDialogBuild(MainWindow* win, gp::Ctx* cx) {
    if (!gScript.visible || gScript.win != win || (gScript.tw && !DlgWindowIsHost(cx))) return nullptr;
    if (!gScriptView.IsValid()) gScriptView = gp::EntityNewState<PDFScriptView>(cx->app);
    auto* body = gp::Div(cx->a)->FlexCol()->Gap(6)->W(gp::kFill);
    auto* rows = gp::Div(cx->a)->FlexCol()->Gap(4)->W(gp::kFill);
    for (int i = 0; i < len(gScript.args); i++) {
        auto& arg = gScript.args[i];
        Str label = len(arg.hint) > 0 ? fmt("%s %d: %s", Tr("Arg"), i + 1, arg.hint) : fmt("%s %d", Tr("Arg"), i + 1);
        auto* row = gp::Div(cx->a)->FlexRow()->ItemsCenter()->Gap(6)->W(gp::kFill);
        row->Child(gpc::Input::New(cx, ToGpui(fmt("pdf-arg-%d", arg.id)), arg.input)
                       ->AriaLabel(ToGpui(label))
                       ->W(gp::kFill)
                       ->IntoEl());
        row->Child(gpc::Button::New(cx, ToGpui(fmt("pdf-pick-%d", arg.id)))
                       ->Label(GStrL("..."))
                       ->Disabled(gScript.running)
                       ->OnClick(gp::ListenTo(gScriptView, &PDFScriptView::OnPick, arg.id))
                       ->IntoEl());
        row->Child(gpc::Button::New(cx, ToGpui(fmt("pdf-delete-%d", arg.id)))
                       ->Label(GStrL("x"))
                       ->Disabled(gScript.running)
                       ->OnClick(gp::ListenTo(gScriptView, &PDFScriptView::OnDelete, arg.id))
                       ->IntoEl());
        rows->Child(row);
    }
    body->Child(rows);
    body->Child(
        gp::Div(cx->a)->FlexRow()->JustifyEnd()->Child(gpc::Button::New(cx, GStrL("pdf-add-arg"))
                                                           ->Label(ToGpui(Tr("Add argument")))
                                                           ->Disabled(gScript.running)
                                                           ->OnClick(gp::ListenTo(gScriptView, &PDFScriptView::OnAdd))
                                                           ->IntoEl()));
    body->Child(gpc::Textarea::New(cx, GStrL("pdf-script-source"), gScript.script)->H(300)->SoftWrap(false)->IntoEl());
    if (len(FromGpui(gp::InputValue(gScript.output))) > 0) {
        body->Child(gpc::Textarea::New(cx, GStrL("pdf-script-output"), gScript.output)->H(140)->Readonly()->IntoEl());
    }
    auto* buttons = gp::Div(cx->a)->FlexRow()->Gap(7)->W(gp::kFill);
    buttons->Child(gpc::Button::New(cx, GStrL("pdf-script-examples"))
                       ->Label(ToGpui(Tr("Example Scripts")))
                       ->OnClick(gp::ListenTo(gScriptView, &PDFScriptView::OnExamples))
                       ->IntoEl());
    buttons->Child(gpc::Button::New(cx, GStrL("pdf-script-help"))
                       ->Label(ToGpui(Tr("Help")))
                       ->OnClick(gp::ListenTo(gScriptView, &PDFScriptView::OnHelp))
                       ->IntoEl());
    buttons->Child(gp::Div(cx->a)->Flex1());
    buttons->Child(gpc::Button::New(cx, GStrL("pdf-script-cancel"))
                       ->Label(ToGpui(Tr("Cancel")))
                       ->OnClick(gp::ListenTo(gScriptView, &PDFScriptView::OnCancel))
                       ->IntoEl());
    buttons->Child(gpc::Button::New(cx, GStrL("pdf-script-run"))
                       ->Label(ToGpui(gScript.running ? Tr("Running...") : Tr("Run Script")))
                       ->Disabled(gScript.running)
                       ->OnClick(gp::ListenTo(gScriptView, &PDFScriptView::OnRun))
                       ->IntoEl());
    body->Child(buttons);
    auto* columns = gp::Div(cx->a)->W(gp::kFill);
    columns->Child(body);
    if (gScript.showExamples) {
        const gp::Theme& theme = gp::ThemeNow(cx->app);
        columns->PadR(kScriptListWidth + kScriptColumnGap);
        auto* list = gp::Div(cx->a)
                         ->Id(GStrL("pdf-script-list"))
                         ->Role(gp::AccessibilityRole::List)
                         ->AriaLabel(ToGpui(Tr("Example Scripts")))
                         ->Absolute()
                         ->Right(0)
                         ->Top(0)
                         ->Bottom(0)
                         ->W(kScriptListWidth)
                         ->FlexCol()
                         ->Border(1, theme.border)
                         ->ScrollY(gScript.exampleScrollY)
                         ->ScrollFromPath()
                         ->OnScroll(gp::ListenTo(gScriptView, &PDFScriptView::OnScroll));
        for (int i = 0; i < PDFScriptCount(); i++) {
            auto* row = gp::Div(cx->a)
                            ->FlexRow()
                            ->Role(gp::AccessibilityRole::ListItem)
                            ->AriaLabel(ToGpui(PDFScriptName(i)))
                            ->W(gp::kFill)
                            ->H(kScriptListRowHeight)
                            ->Shrink0()
                            ->ItemsCenter()
                            ->PadX(4)
                            ->PathClick(GpuiDup(cx->a, fmt("pdf-script-row-%d", i)))
                            ->OnClick(gp::ListenTo(gScriptView, &PDFScriptView::OnExample, (intptr_t)i));
            if (i == gScript.example) row->Bg(DlgWinListSelBg(false));
            row->Child(gp::TextEl(cx->a, ToGpui(PDFScriptName(i)))->Truncate()->Fg(theme.foreground));
            list->Child(row);
        }
        columns->Child(list);
    }
    auto* dlg = gpc::Dialog::New(cx)
                    ->Open(true)
                    ->Title(ToGpui(ScriptTitle()))
                    ->Body(columns)
                    ->Footer(gp::Div(cx->a)->H(0))
                    ->W(720 + (gScript.showExamples ? kScriptListWidth + kScriptColumnGap : 0))
                    ->OnClose(gp::ListenTo(gScriptView, &PDFScriptView::OnCancel));
    if (gScript.wantFocus) {
        gp::InputFocus(gScript.script, cx->app, cx->win);
        gScript.wantFocus = cx->win->input != gScript.script;
    }
    return DlgIntoEl(cx, dlg);
}
