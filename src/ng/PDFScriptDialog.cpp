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
};

static struct PDFScriptDlg {
    MainWindow* win = nullptr;
    ToolWindow* tw = nullptr;
    gp::InputState* script = nullptr;
    gp::InputState* output = nullptr;
    DialogSelect examples;
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
    for (auto& arg : gScript.args) delete arg.input;
    VecClear(gScript.args);
    gScript.examples.Free();
    ++gScriptSession;
    if (IsMainWindowValid(gScript.win)) AppShellInvalidate(gScript.win);
    gScript.win = nullptr;
}

static gp::InputState* ScriptInput(gp::App* app, Str text) {
    auto* input = new gp::InputState();
    input->focus = gp::FocusHandleNew(app);
    gp::InputSetValue(input, ToGpui(text));
    return input;
}

static Str ScriptTitle() {
    return Tr("Run PDF Script");
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
    gScript.visible = true;
    gScript.running = false;
    gScript.wantFocus = true;
    gScript.session = ++gScriptSession;
    VecAppend(gScript.args, ScriptArg{++gScript.nextArg, ScriptInput(app, tab->filePath)});
    VecAppend(gScript.args, ScriptArg{++gScript.nextArg, ScriptInput(app, PDFScriptOutputPath(tab->filePath))});
    gScript.script = ScriptInput(app, PDFScriptSource(0));
    gScript.output = ScriptInput(app, {});
    gScript.examples.Init(app);
    StrVec names;
    for (int i = 0; i < PDFScriptCount(); i++) names.Append(PDFScriptName(i));
    gScript.examples.SetItems(names, 0);
    DlgWindowSpec spec;
    spec.name = "pdfscript";
    spec.title = ScriptTitle;
    spec.build = PDFScriptDialogBuild;
    spec.close = ClosePDFScriptDialog;
    gScript.tw = DlgWindowOpen(spec, win);
    AppShellInvalidate(win);
}

void PDFScriptView::OnAdd(PDFScriptView*, gp::Ctx* cx, const gp::ClickEvent*) {
    VecAppend(gScript.args, ScriptArg{++gScript.nextArg, ScriptInput(cx->app, {})});
    gp::InputFocus(gScript.args[len(gScript.args) - 1].input, cx->app, cx->win);
}

void PDFScriptView::OnDelete(PDFScriptView*, gp::Ctx* cx, const gp::ClickEvent*, int64_t id) {
    for (int i = 0; i < len(gScript.args); i++) {
        auto& arg = gScript.args[i];
        if (arg.id != id) continue;
        gp::InputBlur(arg.input, cx->app, cx->win);
        delete arg.input;
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

void PDFScriptView::OnCancel(PDFScriptView*, gp::Ctx*, const gp::ClickEvent*) {
    ClosePDFScriptDialog();
}

gp::El* PDFScriptDialogBuild(MainWindow* win, gp::Ctx* cx) {
    if (!gScript.visible || gScript.win != win || (gScript.tw && !DlgWindowIsHost(cx))) return nullptr;
    if (!gScriptView.IsValid()) gScriptView = gp::EntityNewState<PDFScriptView>(cx->app);
    if (gScript.examples.PollChanged(cx->app)) {
        gp::InputSetValue(gScript.script, ToGpui(PDFScriptSource(gScript.examples.sel)));
    }
    auto* body = gp::Div(cx->a)->FlexCol()->Gap(6);
    auto* rows = gp::Div(cx->a)->FlexCol()->Gap(4)->W(gp::kFill);
    for (int i = 0; i < len(gScript.args); i++) {
        auto& arg = gScript.args[i];
        auto* row = gp::Div(cx->a)->FlexRow()->ItemsCenter()->Gap(6)->W(gp::kFill);
        row->Child(gp::Div(cx->a)->W(52)->Child(gp::TextEl(cx->a, GpuiDup(cx->a, fmt("%s %d", Tr("Arg"), i + 1)))));
        row->Child(gpc::Input::New(cx, ToGpui(fmt("pdf-arg-%d", arg.id)), arg.input)->W(gp::kFill)->IntoEl());
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
    body->Child(gScript.examples.Build(cx, StrL("pdf-script-example"), 680));
    body->Child(gpc::Textarea::New(cx, GStrL("pdf-script-source"), gScript.script)->H(300)->SoftWrap(false)->IntoEl());
    if (len(FromGpui(gp::InputValue(gScript.output))) > 0) {
        body->Child(gpc::Textarea::New(cx, GStrL("pdf-script-output"), gScript.output)->H(140)->Readonly()->IntoEl());
    }
    auto* buttons = gp::Div(cx->a)->FlexRow()->JustifyEnd()->Gap(7);
    buttons->Child(gpc::Button::New(cx, GStrL("pdf-script-cancel"))
                       ->Label(ToGpui(Tr("Cancel")))
                       ->OnClick(gp::ListenTo(gScriptView, &PDFScriptView::OnCancel))
                       ->IntoEl());
    buttons->Child(gpc::Button::New(cx, GStrL("pdf-script-help"))
                       ->Label(ToGpui(Tr("Help")))
                       ->OnClick(gp::ListenTo(gScriptView, &PDFScriptView::OnHelp))
                       ->IntoEl());
    buttons->Child(gpc::Button::New(cx, GStrL("pdf-script-run"))
                       ->Label(ToGpui(gScript.running ? Tr("Running...") : Tr("Run Script")))
                       ->Disabled(gScript.running)
                       ->OnClick(gp::ListenTo(gScriptView, &PDFScriptView::OnRun))
                       ->IntoEl());
    auto* dlg = gpc::Dialog::New(cx)
                    ->Open(true)
                    ->Title(ToGpui(ScriptTitle()))
                    ->Body(body)
                    ->Footer(buttons)
                    ->W(720)
                    ->OnClose(gp::ListenTo(gScriptView, &PDFScriptView::OnCancel));
    if (gScript.wantFocus) {
        gp::InputFocus(gScript.script, cx->app, cx->win);
        gScript.wantFocus = cx->win->input != gScript.script;
    }
    return DlgIntoEl(cx, dlg);
}
