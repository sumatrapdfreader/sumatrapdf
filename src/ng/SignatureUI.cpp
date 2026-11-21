/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: orig's SignatureUI.cpp draws the bar in a VirtHost child window, the
// dialog in a WindowBase with a read-only Edit and the marks with Gfx. Here
// the bar is a row of the frame's gpui tree, the dialog a modeless DlgWindow
// and the marks are painted on the canvas. What both apps share is in
// SignatureUICommon.cpp.

#include "gui/GpuiBridge.h"
#include "base/UITask.h"

#include "gui/UIModels.h"

#include "Settings.h"
#include "DisplayMode.h"
#include "AppSettings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "base/GuessFileType.h"
#include "EngineAll.h"
#include "DisplayModel.h"
#include "MainWindow.h"
#include "AIChatTypes.h"
#include "WindowTab.h"
#include "Theme.h"
#include "SumatraConfig.h"
#include "SumatraPDF.h"
#include "Translations.h"
#include "gui/AppShell.h"
#include "gui/DialogWidgets.h"
#include "gui/DocCanvas.h"
#include "gui/ToolWindow.h"
#include "gui/Sidebar.h"
#include "SumatraDialogs.h"
#include "shared/PdfToolsCommon.h"
#include "AnnotEditToolbarCommon.h"
#include "SignatureUICommon.h"
#include "SignatureUI.h"

#include "SumatraLog.h"

// the dialog's verdict line, over the text and the footer
constexpr float kSigDlgHeadDy = 36;
constexpr float kSigDlgEditPadDy = 6;

// the current tab's signatures were checked: the bar, the panel and the marks
// follow, and the bar takes a row from the canvas
void OnTabSignaturesChecked(MainWindow* win) {
    WindowTab* tab = win->CurrentTab();
    if (tab && tab->sidebarContent == SidebarContent::Signatures) {
        SetSidebarVisibility(win, tab->showToc, gSettings->showFavorites);
    }
    AppShellSyncCanvasSize(win);
    AppShellInvalidate(win);
}

//--- the bar over the document

int SigBarDy(MainWindow* win) {
    return SigBarWanted(win) ? kSigBarDy : 0;
}

static void OnSigBarClose(MainWindow* win) {
    if (!IsMainWindowValid(win)) {
        return;
    }
    if (PdfSigResult* res = CurrentSignatures(win)) {
        res->barDismissed = true;
    }
    AppShellSyncCanvasSize(win);
    AppShellInvalidate(win);
}

static void OnSigBarPanel(MainWindow* win) {
    if (IsMainWindowValid(win)) {
        SidebarShowSignatures(win);
    }
}

// the listeners carry the window
struct SigBarView {
    static void OnPanel(SigBarView*, gp::Ctx* cx, const gp::ClickEvent*, int64_t win);
    static void OnClose(SigBarView*, gp::Ctx* cx, const gp::ClickEvent*, int64_t win);
};

static gp::Entity<SigBarView> gSigBarView;

// posted: both change the tree the click came from
void SigBarView::OnPanel(SigBarView*, gp::Ctx*, const gp::ClickEvent*, int64_t win) {
    PostForWindow((MainWindow*)(intptr_t)win, OnSigBarPanel, "SigBarPanel");
}

void SigBarView::OnClose(SigBarView*, gp::Ctx*, const gp::ClickEvent*, int64_t win) {
    PostForWindow((MainWindow*)(intptr_t)win, OnSigBarClose, "SigBarClose");
}

//  [icon] what the signatures amount to           [Signature Panel] [x]
gp::El* SigBarBuild(MainWindow* win, gp::Ctx* cx) {
    PdfSigResult* res = CurrentSignatures(win);
    if (!res || !SigBarWanted(win)) {
        return nullptr;
    }
    if (!gSigBarView.IsValid()) {
        gSigBarView = gp::EntityNewState<SigBarView>(cx->app);
    }
    SigVerdict verdict = OverallSigVerdict(res->sum);
    Color bg, txt;
    SigBarColors(verdict, &bg, &txt);
    gp::El* row = gp::Div(cx->a)
                      ->FlexRow()
                      ->ItemsCenter()
                      ->W(gp::kFill)
                      ->H((float)kSigBarDy)
                      ->Shrink0()
                      ->PadX((float)kSigBarPadDx)
                      ->Gap((float)kSigBarPadDx)
                      ->Bg(ToGpui(bg));
    row->Child(gpc::Icon::New(cx, gp::IconName::None)
                   ->Data(ToGpui(Str(SigVerdictIcon(verdict))))
                   ->Size((float)kSigBarIconDx)
                   ->Color(ToGpui(SigVerdictColor(verdict)))
                   ->IntoEl());
    row->Child(gp::TextEl(cx->a, GpuiDup(cx->a, OverallSigMessage(res->sum)))
                   ->Font(kToolDlgTextFontPx)
                   ->Fg(ToGpui(txt))
                   ->Flex1()
                   ->MinW(0)
                   ->Truncate());
    row->Child(gpc::Button::New(cx, GStrL("sigbar-panel"))
                   ->Label(ToGpui(Tr("Signature Panel")))
                   ->Ghost()
                   ->WithSize(gp::UiSize::Small)
                   ->OnClick(gp::ListenTo(gSigBarView, &SigBarView::OnPanel, (int64_t)(intptr_t)win))
                   ->IntoEl());
    row->Child(gpc::Button::New(cx, GStrL("sigbar-close"))
                   ->Icon(gp::IconName::Close)
                   ->Ghost()
                   ->Compact()
                   ->WithSize(gp::UiSize::XSmall)
                   ->Size((float)kSigBarIconDx)
                   ->Tooltip(ToGpui(Tr("Close")))
                   ->OnClick(gp::ListenTo(gSigBarView, &SigBarView::OnClose, (int64_t)(intptr_t)win))
                   ->IntoEl());
    return row;
}

//--- the Signature Properties dialog

struct SignatureDlg {
    MainWindow* win = nullptr;
    bool visible = false;
    SigVerdict verdict = SigVerdict::Unknown;
    Str status;
    Str certDer;
    // the text, for tests (the input keeps its own copy)
    Str text;
    gp::InputState* edit = nullptr;
};

static SignatureDlg gSigDlg;
// orig's window (DlgWindowOpen): modeless and owned; null: a dialog in the frame
static ToolWindow* gSigDlgTw = nullptr;

struct SignatureView {
    static void OnShowCert(SignatureView*, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnClose(SignatureView*, gp::Ctx* cx, const gp::ClickEvent*);
};

static gp::Entity<SignatureView> gSignatureView;

bool IsSignatureDialogVisible() {
    return gSigDlg.visible && !gSigDlgTw;
}

static Str SignatureDialogTitle() {
    return Tr("Signature Properties");
}

void CloseSignatureDialog() {
    if (!gSigDlg.visible) {
        return;
    }
    gSigDlg.visible = false;
    MainWindow* win = gSigDlg.win;
    if (gSigDlgTw) {
        DlgWindowClose(&gSigDlgTw);
    } else if (IsMainWindowValid(win) && win->gpuiWin && gSigDlg.edit) {
        InputBlurWhereFocused(gSigDlg.edit, win->gpuiWin);
    }
    delete gSigDlg.edit;
    gSigDlg.edit = nullptr;
    str::FreePtr(&gSigDlg.status);
    str::FreePtr(&gSigDlg.certDer);
    str::FreePtr(&gSigDlg.text);
    AppShellInvalidate(IsMainWindowValid(win) ? win : nullptr);
}

void SignatureView::OnShowCert(SignatureView*, gp::Ctx* cx, const gp::ClickEvent*) {
#if OS_WIN
    ViewCertDer((HWND)gp::PlatWindowHandle(cx->win), gSigDlg.certDer);
#endif
    gp::Notify(cx);
}

void SignatureView::OnClose(SignatureView*, gp::Ctx* cx, const gp::ClickEvent*) {
    CloseSignatureDialog();
    gp::Notify(cx);
}

// not modal: it's useful next to the document it's about
void ShowSignatureDialog(MainWindow* win, int entryIdx) {
    PdfSigResult* res = CurrentSignatures(win);
    if (!res || entryIdx < 0 || entryIdx >= len(res->entries)) {
        return;
    }
    CloseSignatureDialog();
    const PdfSigEntry* e = res->entries[entryIdx];
    gSigDlg.win = win;
    gSigDlg.verdict = e->verdict;
    gSigDlg.status = str::Dup(e->status);
    gSigDlg.certDer = str::Dup(e->certDer);
    gSigDlg.text = str::Dup(SignatureDialogTextTemp(e, StrL("\n")));
    auto* s = new gp::InputState();
    s->focus = gp::FocusHandleNew(win->gpuiWin ? win->gpuiWin->app : nullptr);
    s->kind = gp::InputKind::Textarea;
    s->mode.kind = gp::LayoutModeKind::PlainText;
    gp::InputSetValue(s, ToGpui(gSigDlg.text));
    gSigDlg.edit = s;
    gSigDlg.visible = true;

    DlgWindowSpec spec;
    spec.name = "signature";
    spec.title = SignatureDialogTitle;
    spec.modal = false;
    spec.owned = true;
    spec.build = SignatureDialogBuild;
    spec.close = CloseSignatureDialog;
    spec.clientDx = (float)kSigDialogDx;
    gSigDlgTw = DlgWindowOpen(spec, win);
    AppShellInvalidate(win);
}

//   (icon) Valid qualified electronic signature.
//   +--------------------------------------------+
//   | Validity Summary ...                        |
//   +--------------------------------------------+
//   [Show Signer's Certificate...]         [Close]
gp::El* SignatureDialogBuild(MainWindow* win, gp::Ctx* cx) {
    if (!gSigDlg.visible || gSigDlg.win != win) {
        return nullptr;
    }
    if (gSigDlgTw && !DlgWindowIsHost(cx)) {
        return nullptr;
    }
    if (!gSignatureView.IsValid()) {
        gSignatureView = gp::EntityNewState<SignatureView>(cx->app);
    }
    gp::Listener onClose = gp::ListenTo(gSignatureView, &SignatureView::OnClose);
    DlgSetDefault(cx, onClose, true);

    // the verdict first, in its color
    Color col = SigVerdictColor(gSigDlg.verdict);
    gp::El* head = gp::Div(cx->a)
                       ->FlexRow()
                       ->ItemsCenter()
                       ->W(gp::kFill)
                       ->H(kSigDlgHeadDy)
                       ->Shrink0()
                       ->PadT(12)
                       ->PadX(kToolDlgPadDx)
                       ->Gap((float)kSigBarPadDx);
    head->Child(gpc::Icon::New(cx, gp::IconName::None)
                    ->Data(ToGpui(Str(SigVerdictIcon(gSigDlg.verdict))))
                    ->Size((float)(kSigBarIconDx + 6))
                    ->Color(ToGpui(col))
                    ->IntoEl());
    head->Child(gp::TextEl(cx->a, GpuiDup(cx->a, gSigDlg.status))
                    ->Font((float)kSigStatusFontPx)
                    ->Fg(ToGpui(col))
                    ->Flex1()
                    ->MinW(0)
                    ->Truncate());

    float footerDy = (2 * kToolDlgFootPadDy) + kDlgBtnDy;
    float editDy = (float)kSigDialogDy - kSigDlgHeadDy - footerDy - (2 * kSigDlgEditPadDy);
    gp::El* edit = gpc::Textarea::New(cx, GStrL("signature-text"), gSigDlg.edit)->H(editDy)->Readonly()->IntoEl();
    gp::El* text = gp::Div(cx->a)->W(gp::kFill)->Shrink0()->PadX(kToolDlgPadDx)->PadY(kSigDlgEditPadDy)->Child(edit);

    gp::El* cert = nullptr;
#if OS_WIN
    if (len(gSigDlg.certDer) > 0) {
        cert = gpc::Button::New(cx, GStrL("signature-cert"))
                   ->Label(ToGpui(Tr("Show Signer's Certificate...")))
                   ->Outline()
                   ->WithSize(gp::UiSize::Small)
                   ->OnClick(gp::ListenTo(gSignatureView, &SignatureView::OnShowCert))
                   ->IntoEl();
    }
#endif
    gp::El* col1 = gp::Div(cx->a)->FlexCol()->W(gp::kFill);
    col1->Child(head);
    col1->Child(text);
    col1->Child(DlgFooter(cx, Tr("Close"), onClose, gp::Listener{}, cert));
    return DlgToolContent(cx, col1, SignatureDialogTitle(), (float)kSigDialogDx, onClose);
}

//--- the mark on a visible signature

static bool MarksEnabled(MainWindow* win) {
    return win->AsFixed() && !win->pdfAnnotationsToolbarEnabled;
}

static Rect SignatureScreenRect(DisplayModel* dm, const PdfSigEntry* e) {
    if (e->rect.IsEmpty() || e->pageNo <= 0 || !dm->ValidPageNo(e->pageNo) || !dm->PageVisible(e->pageNo)) {
        return {};
    }
    return dm->CvtToScreen(e->pageNo, e->rect);
}

static int SignatureAtPoint(MainWindow* win, Point pt) {
    PdfSigResult* res = CurrentSignatures(win);
    DisplayModel* dm = win->AsFixed();
    if (!res || !dm || !MarksEnabled(win)) {
        return -1;
    }
    for (int i = 0; i < len(res->entries); i++) {
        if (SignatureScreenRect(dm, res->entries[i]).Contains(pt)) {
            return i;
        }
    }
    return -1;
}

// a visible signature looks signed whatever its state: this says what it is
void PaintSignatureMarks(MainWindow* win, gp::PaintCtx* ctx) {
    PdfSigResult* res = CurrentSignatures(win);
    DisplayModel* dm = win->AsFixed();
    if (!res || !dm || !MarksEnabled(win)) {
        return;
    }
    int dx = (int)((float)kSigMarkDx / CanvasScale(win));
    for (const PdfSigEntry* e : res->entries) {
        Rect r = SignatureScreenRect(dm, e);
        if (r.IsEmpty()) {
            continue;
        }
        Rect rc = {r.x + 2, r.y + 2, dx, dx};
        CanvasDrawEllipse(ctx, rc, MkRgb(0xff, 0xff, 0xff), 0);
        CanvasDrawSvg(ctx, Str(SigVerdictIcon(e->verdict)), rc, SigVerdictColor(e->verdict));
    }
}

struct SigMarkClick {
    MainWindow* win = nullptr;
    int idx = 0;
};

static void OnSigMarkClick(SigMarkClick* c) {
    if (IsMainWindowValid(c->win)) {
        ShowSignatureDialog(c->win, c->idx);
    }
    delete c;
}

bool SigMarksOnLeftDown(MainWindow* win, int x, int y) {
    int idx = SignatureAtPoint(win, Point{x, y});
    if (idx < 0) {
        return false;
    }
    auto* c = new SigMarkClick{win, idx};
    uitask::Post(MkFunc0<SigMarkClick>(OnSigMarkClick, c), "SigMarkClick");
    return true;
}

bool SigMarksOnSetCursor(MainWindow* win, Point pt) {
    return SignatureAtPoint(win, pt) >= 0;
}

//--- what a test can see

// action: "panel" presses the bar's button, "close" its x, "row <n>" selects
// a row, "open <n>" opens a row's dialog. The mark is in frame pixels: ng
// draws the page in the frame
TempStr SignatureUiStateTemp(MainWindow* win, Str action) {
    if (str::Eq(action, StrL("panel"))) {
        OnSigBarPanel(win);
    } else if (str::Eq(action, StrL("close"))) {
        OnSigBarClose(win);
    } else if (str::StartsWith(action, StrL("row "))) {
        SidebarSelectSignature(win, ParseInt(Str(action.s + 4, len(action) - 4)));
    } else if (str::StartsWith(action, StrL("open "))) {
        ShowSignatureDialog(win, ParseInt(Str(action.s + 5, len(action) - 5)));
    }
    str::Builder out;
    PdfSigResult* res = CurrentSignatures(win);
    bool barVisible = SigBarWanted(win);
    Str msg = res ? OverallSigMessage(res->sum) : Str();
    out.Append(fmt("bar=%d %s\n", barVisible ? 1 : 0, msg));
    out.Append(fmt("rows=%d\n", res ? len(res->entries) : 0));
    bool panelShown = SidebarContentVisible(win, SidebarContent::Signatures);
    out.Append(fmt("panel=%d\n", panelShown ? 1 : 0));
    out.Append(fmt("listFocused=%d\n", SidebarSignaturesFocused(win) ? 1 : 0));
    out.Append(fmt("page=%d\n", win->ctrl ? win->ctrl->CurrentPageNo() : 0));
    Rect mark;
    if (res && win->AsFixed()) {
        for (const PdfSigEntry* e : res->entries) {
            mark = SignatureScreenRect(win->AsFixed(), e);
            if (!mark.IsEmpty()) {
                mark = CanvasToFramePx(win, mark);
                break;
            }
        }
    }
    out.Append(fmt("mark=%d,%d,%d,%d\n", mark.x, mark.y, mark.dx, mark.dy));
    out.Append(fmt("dialog=%d\n", gSigDlg.visible ? 1 : 0));
    if (gSigDlg.visible) {
        out.Append(gSigDlg.text);
    }
    return ToStrTemp(out);
}
