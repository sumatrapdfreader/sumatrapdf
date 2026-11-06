/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Pixmap.h"
#include "base/GuessFileType.h"
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
#include "gui/VirtHost.h"
#include "Settings.h"
#include "DisplayMode.h"
#include "AppSettings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "EngineAll.h"
#include "DisplayModel.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "Theme.h"
#include "SumatraConfig.h"
#include "SumatraPDF.h"
#include "SumatraDialogs.h"
#include "SvgIcons.h"
#include "Translations.h"
#include "DarkMode.h"
#include "DocumentProperties.h"
#include "SidebarPanel.h"
#include "AppHelpersCommon.h"
#include "shared/PdfToolsCommon.h"
#include "SignatureUI.h"

constexpr int kSigBarDy = 32;
constexpr int kSigBarIconDx = 18;
constexpr int kSigBarPadDx = 10;
constexpr int kSigRowDy = 46;
constexpr int kSigRowIconDx = 18;
constexpr int kSigRowPadDx = 8;
constexpr int kSigMarkDx = 20;
constexpr int kSigDialogDx = 620;
constexpr int kSigDialogDy = 460;
constexpr int kSigStatusFontPx = 16;

static const Color kColValid = MkRgb(0x1a, 0x7f, 0x37);
static const Color kColProblem = MkRgb(0xb5, 0x80, 0x00);
static const Color kColInvalid = MkRgb(0xc6, 0x28, 0x28);
static const Color kColBarProblemBg = MkRgb(0xff, 0xf3, 0xb0);
static const Color kColBarInvalidBg = MkRgb(0xff, 0xd6, 0xd6);
static const Color kColBarWarnText = MkRgb(0x20, 0x20, 0x20);

static const char* VerdictIcon(SigVerdict v) {
    switch (v) {
        case SigVerdict::Invalid:
            return gIconSigInvalid;
        case SigVerdict::Unknown:
            return gIconSigProblem;
        default:
            return gIconSigValid;
    }
}

static Color VerdictColor(SigVerdict v) {
    switch (v) {
        case SigVerdict::Invalid:
            return kColInvalid;
        case SigVerdict::Unknown:
            return kColProblem;
        default:
            return kColValid;
    }
}

static Pixmap* VerdictPixmap(SigVerdict v, int dx) {
    return GetCachedPixmapForSvg(Str(VerdictIcon(v)), dx, dx, VerdictColor(v));
}

static PdfSigResult* TabSignatures(WindowTab* tab) {
    return tab ? tab->signatures : nullptr;
}

static PdfSigResult* CurrentSignatures(MainWindow* win) {
    return win ? TabSignatures(win->CurrentTab()) : nullptr;
}

bool TabHasSignatures(WindowTab* tab) {
    PdfSigResult* res = TabSignatures(tab);
    return res && len(res->entries) > 0;
}

static SigVerdict OverallVerdict(const PdfSigSummary& sum) {
    if (sum.anyInvalid) {
        return SigVerdict::Invalid;
    }
    if (sum.anyUnknown) {
        return SigVerdict::Unknown;
    }
    return sum.nQualified > 0 ? SigVerdict::ValidQualified : SigVerdict::Valid;
}

static Str OverallMessage(const PdfSigSummary& sum) {
    if (sum.anyInvalid) {
        return Tr("At least one signature is invalid.");
    }
    if (sum.anyUnknown) {
        return Tr("At least one signature has problems.");
    }
    if (sum.nQualified > 0) {
        return Tr("Signed with a qualified signature. All signatures are valid.");
    }
    return Tr("Signed and all signatures are valid.");
}

static void GoToSignature(MainWindow* win, const PdfSigEntry* e) {
    if (win->ctrl && e->pageNo > 0 && e->pageNo <= win->ctrl->PageCount()) {
        win->ctrl->GoToPage(e->pageNo, true);
    }
}

//--- the sidebar view

struct SigRowsModel : ListBoxModel {
    MainWindow* win = nullptr;

    int ItemsCount() override {
        PdfSigResult* res = CurrentSignatures(win);
        return res ? len(res->entries) : 0;
    }
    Str Item(int i) override {
        PdfSigResult* res = CurrentSignatures(win);
        return res ? res->entries[i]->signer : Str();
    }
};

struct SignaturesCtrl : VirtListBox {
    MainWindow* win = nullptr;
    SigRowsModel* rows = nullptr;

    void DrawRow(DrawItemEvent* ev);
    void OnSelected();
    void OnOpen();
};

void SignaturesCtrl::DrawRow(DrawItemEvent* ev) {
    PdfSigResult* res = CurrentSignatures(win);
    if (!res || ev->itemIndex < 0 || ev->itemIndex >= len(res->entries)) {
        return;
    }
    const PdfSigEntry* e = res->entries[ev->itemIndex];
    Gfx* gfx = ev->gfx;
    Rect r = ev->itemRect;
    if (ev->selected) {
        gfx->FillRect(r, GetColor(kColListSel));
    }
    int pad = DpiScaleByDpi(dpi, kSigRowPadDx);
    int iconDx = DpiScaleByDpi(dpi, kSigRowIconDx);
    Rect rcIcon = {r.x + pad, r.y + ((r.dy - iconDx) / 2), iconDx, iconDx};
    gfx->DrawPixmap(VerdictPixmap(e->verdict, iconDx), rcIcon);

    int textX = rcIcon.Right() + pad;
    Rect rcText = {textX, r.y, r.Right() - pad - textX, r.dy / 2};
    Rect rcStatus = {textX, r.y + (r.dy / 2), rcText.dx, r.dy / 2};
    Color col = GetColor(kColListText);
    gfx->DrawText(len(e->signer) > 0 ? e->signer : Tr("Unknown signer"), rcText, gfxTextEllipsis | gfxTextVCenter, font,
                  col);
    gfx->DrawText(e->status, rcStatus, gfxTextEllipsis | gfxTextVCenter, font, AccentColor(col, 40));
}

void SignaturesCtrl::OnSelected() {
    PdfSigResult* res = CurrentSignatures(win);
    int i = GetCurrentSelection();
    if (res && i >= 0 && i < len(res->entries)) {
        GoToSignature(win, res->entries[i]);
    }
}

void SignaturesCtrl::OnOpen() {
    int i = GetCurrentSelection();
    if (i >= 0) {
        ShowSignatureDialog(win, i);
    }
}

void CreateSignaturesCtrl(MainWindow* win) {
    auto* c = new SignaturesCtrl();
    c->win = win;
    c->rows = new SigRowsModel();
    c->rows->win = win;
    c->SetModel(c->rows);
    c->onDrawItem = MkMethod1<SignaturesCtrl, VirtListBox::DrawItemEvent*, &SignaturesCtrl::DrawRow>(c);
    c->onSelectionChanged = MkMethod0<SignaturesCtrl, &SignaturesCtrl::OnSelected>(c);
    c->onDoubleClick = MkMethod0<SignaturesCtrl, &SignaturesCtrl::OnOpen>(c);
    c->SetIsVisible(false);
    win->signaturesCtrl = c;
    SignaturesCtrlColorsAndDpi(win, DpiGetForHwnd(win->hwndFrame));
}

VirtCtrl* SignaturesView(MainWindow* win) {
    return win->signaturesCtrl;
}

void DeleteSignaturesCtrl(MainWindow* win) {
    delete win->signaturesCtrl;
    win->signaturesCtrl = nullptr;
}

void SignaturesCtrlColorsAndDpi(MainWindow* win, int dpi) {
    SignaturesCtrl* c = win->signaturesCtrl;
    if (!c) {
        return;
    }
    c->font = GetAppFont();
    c->dpi = dpi;
    c->itemDy = DpiScaleByDpi(dpi, kSigRowDy);
    c->SetColor(kColListText, ThemeWindowTextColor());
    c->SetColor(kColListBg, ThemeControlBackgroundColor());
    c->Invalidate();
}

// the list follows the current tab
void UpdateSignaturesCtrl(MainWindow* win) {
    SignaturesCtrl* c = win->signaturesCtrl;
    if (!c) {
        return;
    }
    c->SetCurrentSelection(-1);
    c->SetModel(c->rows);
    c->Invalidate();
}

// beside an icon, so without the dialog caption's insets
static VirtText* NewSigCaption(Str s, PlatformFont* font, Color col) {
    VirtText* t = NewDlgCaption(s, font, col);
    t->padding = Insets{};
    return t;
}

//--- the bar over the document

// a host per window; the tree is rebuilt whenever what it says changes
struct SigBar {
    VirtHost* host = nullptr;
    const char* builtMsg = nullptr;
    int builtDpi = 0;
    Color builtBg = 0;
    Color builtText = 0;
};

static void OnSigBarClose(MainWindow* win) {
    if (!IsMainWindowValid(win)) {
        return;
    }
    if (PdfSigResult* res = CurrentSignatures(win)) {
        res->barDismissed = true;
    }
    ScheduleUiUpdate(win, kUiForceRelayout | kUiRelayout);
}

static void OnSigBarPanel(MainWindow* win) {
    if (IsMainWindowValid(win)) {
        ShowSidebarView(win, SidebarView::Signatures);
    }
}

// posted: both change what the bar's tree holds, and a click comes from inside it
static void OnSigBarCloseClick(MainWindow* win, VirtMouseEvent*) {
    PostForWindow(win, OnSigBarClose, "SigBarClose");
}

static void OnSigBarPanelClick(MainWindow* win, VirtMouseEvent*) {
    PostForWindow(win, OnSigBarPanel, "SigBarPanel");
}

bool SigBarWanted(MainWindow* win) {
    PdfSigResult* res = CurrentSignatures(win);
    if (!res || res->sum.nSigned == 0 || res->barDismissed || !gSettings->showSignatureStatus) {
        return false;
    }
    return !win->isFullScreen && win->presentation == PM_DISABLED;
}

int SigBarDy(MainWindow* win) {
    return DpiScaleByDpi(DpiGetForHwnd(win->hwndFrame), kSigBarDy);
}

HWND SigBarEnsure(MainWindow* win) {
    if (win->sigBar) {
        return win->sigBar->host->native;
    }
    VirtHost::CreateArgs args;
    args.parent = win->hwndFrame;
    args.className = WStrL(L"SumatraSigBarHost");
    args.initialSize = {100, SigBarDy(win)};
    args.bgColor = ThemeControlBackgroundColor();
    args.isRtl = IsUIRtl();
    args.visible = false;
    args.noActivate = true;
    args.clipSiblings = true;
    args.userData = win;
    VirtHost* host = VirtHost::Create(args);
    if (!host) {
        return nullptr;
    }
    win->sigBar = new SigBar();
    win->sigBar->host = host;
    SigBarUpdate(win);
    return host->native;
}

void SigBarSetVisible(MainWindow* win, bool visible) {
    if (win->sigBar) {
        win->sigBar->host->Show(visible);
    }
}

void DeleteSigBar(MainWindow* win) {
    if (win->sigBar) {
        delete win->sigBar->host;
        delete win->sigBar;
        win->sigBar = nullptr;
    }
}

void SigBarUpdate(MainWindow* win) {
    PdfSigResult* res = CurrentSignatures(win);
    if (!win->sigBar || !res) {
        return;
    }
    VirtHost* host = win->sigBar->host;
    HWND hwnd = host->native;
    int dpi = DpiGetForHwnd(win->hwndFrame);
    SigVerdict verdict = OverallVerdict(res->sum);
    Color bg = ThemeControlBackgroundColor();
    Color txt = ThemeWindowTextColor();
    if (verdict == SigVerdict::Unknown || verdict == SigVerdict::Invalid) {
        bg = verdict == SigVerdict::Invalid ? kColBarInvalidBg : kColBarProblemBg;
        txt = kColBarWarnText;
    }
    Str msg = OverallMessage(res->sum);
    SigBar* bar = win->sigBar;
    if (bar->builtMsg == msg.s && bar->builtDpi == dpi && bar->builtBg == bg && bar->builtText == txt) {
        return;
    }
    bar->builtMsg = msg.s;
    bar->builtDpi = dpi;
    bar->builtBg = bg;
    bar->builtText = txt;
    host->bgColor = bg;

    PlatformFont* font = DlgUiFont(kToolDlgTextFontPx);
    int iconDx = DpiScaleByDpi(dpi, kSigBarIconDx);
    int pad = DpiScaleByDpi(dpi, kSigBarPadDx);

    auto* row = new HBox();
    row->alignCross = CrossAxisAlign::CrossCenter;
    row->gap = pad;
    auto* icon = new VirtIconButton();
    icon->pixmap = VerdictPixmap(verdict, iconDx);
    row->AddChild(icon);
    row->AddChild(NewSigCaption(msg, font, txt), 1);
    VirtButton* panel = NewDlgButton(hwnd, Tr("Signature Panel"), font, DlgButtonStyle::Ghost, false);
    panel->onClick = MkFunc1(OnSigBarPanelClick, win);
    row->AddChild(panel);
    auto* close = new VirtCloseButton();
    int closeDx = DpiScaleByDpi(dpi, kSigBarIconDx);
    close->idealSize = {closeDx, closeDx};
    close->SetColor(kColCloseCircle, bg);
    close->onClick = MkFunc1(OnSigBarCloseClick, win);
    row->AddChild(close);
    host->SetLayout(new Padding(row, Insets{0, pad, 0, pad}));
    host->Invalidate();
}

//--- the Signature Properties dialog

struct SignatureWnd : WindowBase {
    MainWindow* win = nullptr;
    Str certDer;
    Edit* edit = nullptr;

    ~SignatureWnd() override { str::Free(certDer); }
    bool Create(MainWindow*, const PdfSigEntry*);
    void OnShowCert(VirtMouseEvent*);
    void OnCloseClick(VirtMouseEvent*);
};

static SignatureWnd* gSignatureWnd = nullptr;

static void OnSignatureWndClose(WindowBase::CloseEvent*) {
    if (gSignatureWnd) {
        gSignatureWnd->ScheduleDelete();
    }
}

static void OnSignatureWndDestroy(WindowBase::DestroyEvent*) {
    if (gSignatureWnd) {
        gSignatureWnd->ScheduleDelete();
    }
}

static void ClearSignatureWnd() {
    gSignatureWnd = nullptr;
}

void SignatureWnd::OnShowCert(VirtMouseEvent*) {
    ViewCertDer(hwnd, certDer);
}

void SignatureWnd::OnCloseClick(VirtMouseEvent*) {
    ScheduleDelete();
}

// the lines of `all` that aren't in `validity`, without the signature's header
static void AppendSignerInfo(str::Builder& out, Str all, Str validity) {
    StrVec lines;
    Split(&lines, all, StrL("\n"), true);
    for (int i = 1; i < len(lines); i++) {
        Str line = lines[i];
        if (str::StartsWith(line, StrL("  Status:")) || str::Contains(validity, line)) {
            continue;
        }
        str::TrimWsBoth(line);
        if (str::EndsWith(line, StrL(":"))) {
            continue;
        }
        out.Append(line);
        out.Append(StrL("\r\n"));
    }
}

static void AppendCrLf(str::Builder& out, Str text) {
    StrVec lines;
    Split(&lines, text, StrL("\n"), true);
    for (Str line : lines) {
        str::TrimWsBoth(line);
        out.Append(line);
        out.Append(StrL("\r\n"));
    }
}

bool SignatureWnd::Create(MainWindow* w, const PdfSigEntry* e) {
    win = w;
    certDer = str::Dup(e->certDer);
    Color colTxt = ThemeWindowTextColor();
    Color colBg = ThemeWindowControlBackgroundColor();
    PlatformFont* fontText = DlgUiFont(kToolDlgTextFontPx);
    SetFont(fontText);
    bool isRtl = IsUIRtl();
    {
        CreateCustomArgs args;
        args.owner = win->hwndFrame;
        args.title = Tr("Signature Properties");
        args.visible = false;
        args.style = WS_POPUPWINDOW | WS_CAPTION;
        args.font = fontText;
        args.isRtl = isRtl;
        args.bgColor = colBg;
        args.icon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(GetAppIconID()));
        CreateCustom(args);
    }
    if (!hwnd) {
        return false;
    }
    SetColors(colTxt, colBg);

    auto* vbox = new VBox();
    vbox->alignCross = CrossAxisAlign::Stretch;
    vbox->gap = DpiScale(6);

    // the verdict first, in its color
    auto* head = new HBox();
    head->alignCross = CrossAxisAlign::CrossCenter;
    head->gap = DpiScale(kSigBarPadDx);
    auto* icon = new VirtIconButton();
    icon->pixmap = VerdictPixmap(e->verdict, DpiScale(kSigBarIconDx + 6));
    head->AddChild(icon);
    head->AddChild(NewSigCaption(e->status, DlgUiFont(kSigStatusFontPx), VerdictColor(e->verdict)), 1);
    vbox->AddChild(new Padding(head, DpiScaledInsets(12, kToolDlgPadDx, 0, kToolDlgPadDx)));

    str::Builder text;
    text.Append(Tr("Validity Summary"));
    text.Append(StrL("\r\n"));
    AppendCrLf(text, e->validity);
    text.Append(StrL("\r\n"));
    text.Append(Tr("Signer Info"));
    text.Append(StrL("\r\n"));
    AppendSignerInfo(text, e->details, e->validity);

    Edit::CreateArgs editArgs;
    editArgs.parent = hwnd;
    editArgs.font = fontText;
    editArgs.isMultiLine = true;
    editArgs.isRtl = isRtl;
    edit = new Edit();
    edit->SetColors(colTxt, colBg);
    edit->Create(editArgs);
    SendMessageW(edit->hwnd, EM_SETREADONLY, TRUE, 0);
    edit->SetText(ToStr(text));
    vbox->AddChild(new Padding(edit, DpiScaledInsets(6, kToolDlgPadDx, 6, kToolDlgPadDx)), 1);

    VirtButton* cert = nullptr;
    if (len(certDer) > 0) {
        cert = NewDlgButton(hwnd, Tr("Show Signer's Certificate..."), fontText, DlgButtonStyle::Outline, false);
        cert->onClick = MkMethod1<SignatureWnd, VirtMouseEvent*, &SignatureWnd::OnShowCert>(this);
    }
    VirtButton* close = NewDlgButton(hwnd, Tr("Close"), fontText, DlgButtonStyle::Filled, true);
    close->onClick = MkMethod1<SignatureWnd, VirtMouseEvent*, &SignatureWnd::OnCloseClick>(this);
    vbox->AddChild(NewDlgFooter(nullptr, close, cert));
    layout = vbox;

    LayoutAndSizeToContent(layout, DpiScale(kSigDialogDx), DpiScale(kSigDialogDy), hwnd);
    DoLayout(HwndClientRect(hwnd).Size());
    HwndCenterDialog(hwnd, win->hwndFrame);
    UpdateTheme();
    SetIsVisible(true);
    return true;
}

// not modal: it's useful next to the document it's about
void ShowSignatureDialog(MainWindow* win, int entryIdx) {
    PdfSigResult* res = CurrentSignatures(win);
    if (!res || entryIdx < 0 || entryIdx >= len(res->entries)) {
        return;
    }
    if (gSignatureWnd) {
        gSignatureWnd->ScheduleDelete();
        gSignatureWnd = nullptr;
    }
    auto* wnd = new SignatureWnd();
    wnd->closeOnEsc = true;
    wnd->onBeforeDelete = MkFunc0Void(ClearSignatureWnd);
    wnd->onClose = MkFunc1Void<WindowBase::CloseEvent*>(OnSignatureWndClose);
    wnd->onDestroy = MkFunc1Void<WindowBase::DestroyEvent*>(OnSignatureWndDestroy);
    gSignatureWnd = wnd;
    if (!wnd->Create(win, res->entries[entryIdx])) {
        gSignatureWnd = nullptr;
        delete wnd;
    }
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
void PaintSignatureMarks(MainWindow* win, Gfx* gfx) {
    PdfSigResult* res = CurrentSignatures(win);
    DisplayModel* dm = win->AsFixed();
    if (!res || !dm || !MarksEnabled(win)) {
        return;
    }
    int dx = DpiScaleByDpi(DpiGetForHwnd(win->hwndCanvas), kSigMarkDx);
    for (const PdfSigEntry* e : res->entries) {
        Rect r = SignatureScreenRect(dm, e);
        if (r.IsEmpty()) {
            continue;
        }
        Rect rc = {r.x + 2, r.y + 2, dx, dx};
        gfx->FillEllipse(rc, MkRgb(0xff, 0xff, 0xff));
        gfx->DrawPixmap(VerdictPixmap(e->verdict, dx), rc);
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

bool SigMarksOnSetCursor(MainWindow* win) {
    if (SignatureAtPoint(win, HwndGetCursorPos(win->hwndCanvas)) < 0) {
        return false;
    }
    SetCursorCached(IDC_HAND);
    return true;
}

//--- what a test can see

// action: "panel" presses the bar's button, "close" its ✕, "row <n>" selects a row
TempStr SignatureUiStateTemp(MainWindow* win, Str action) {
    if (str::Eq(action, StrL("panel"))) {
        OnSigBarPanel(win);
    } else if (str::Eq(action, StrL("close"))) {
        OnSigBarClose(win);
    } else if (str::StartsWith(action, StrL("row "))) {
        int i = ParseInt(Str(action.s + 4, len(action) - 4));
        win->signaturesCtrl->SetCurrentSelection(i);
        win->signaturesCtrl->OnSelected();
    } else if (str::StartsWith(action, StrL("open "))) {
        ShowSignatureDialog(win, ParseInt(Str(action.s + 5, len(action) - 5)));
    }
    str::Builder out;
    PdfSigResult* res = CurrentSignatures(win);
    bool barVisible = win->sigBar && win->sigBar->host->IsVisible();
    Str msg = res ? OverallMessage(res->sum) : Str();
    out.Append(fmt("bar=%d %s\n", barVisible ? 1 : 0, msg));
    out.Append(fmt("rows=%d\n", win->signaturesCtrl ? win->signaturesCtrl->ItemsCount() : 0));
    bool panelShown = SidebarPanelShowing(win, SidebarView::Signatures) != nullptr;
    out.Append(fmt("panel=%d\n", panelShown ? 1 : 0));
    bool listFocused = win->signaturesCtrl && win->signaturesCtrl->HasFlag(vwfFocused);
    out.Append(fmt("listFocused=%d\n", listFocused ? 1 : 0));
    out.Append(fmt("page=%d\n", win->ctrl ? win->ctrl->CurrentPageNo() : 0));
    Rect mark;
    if (res && win->AsFixed()) {
        for (const PdfSigEntry* e : res->entries) {
            mark = SignatureScreenRect(win->AsFixed(), e);
            if (!mark.IsEmpty()) {
                break;
            }
        }
    }
    out.Append(fmt("mark=%d,%d,%d,%d\n", mark.x, mark.y, mark.dx, mark.dy));
    out.Append(fmt("dialog=%d\n", gSignatureWnd ? 1 : 0));
    if (gSignatureWnd && gSignatureWnd->edit) {
        out.Append(gSignatureWnd->edit->GetTextTemp());
    }
    return ToStrTemp(out);
}
