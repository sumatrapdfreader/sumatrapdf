/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/File.h"
#include "base/Win.h"
#include "gui/Dpi.h"
#include "base/UITask.h"

#include "gui/UIModels.h"
#include "gui/Layout.h"
#include "gui/win/WinGui.h"
#include "gui/PlatformFont.h"
#include "gui/Gfx.h"
#include "gui/VirtCtrl.h"

#include "Settings.h"
#include "AppSettings.h"
#include "DocProperties.h"
#include "DocController.h"
#include "EngineBase.h"
#include "base/GuessFileType.h"
#include "EngineAll.h"
#include "DisplayModel.h"
#include "AppTools.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "Commands.h"
#include "Translations.h"
#include "SumatraConfig.h"
#include "Print.h"
#include "Theme.h"
#include "DarkMode.h"
#include "EutlTrust.h"
#include "SumatraDialogs.h"
#include "shared/PdfToolsCommon.h"

#include <wincrypt.h>
#include "DocumentProperties.h"
#include "DocumentPropertiesCommon.h"

// The properties as selectable monospaced text on the dialog's background,
// then the footer: the certificate actions as quiet buttons on the left, the
// one filled Copy To Clipboard on the right
constexpr int kPropsPadDy = 14;

struct PropertiesWnd : WindowBase {
    HWND hwndParent = nullptr;
    Edit* editProps = nullptr;
    VirtButton* btnCopyToClipboard = nullptr;
    VirtButton* btnViewCert = nullptr;
    VirtButton* btnUpdateEutl = nullptr;
    PlatformFont* propsFont = nullptr;
    HBox* footer = nullptr;
    Color colTxt = kColorUnset;
    Color colBg = kColorUnset;
    str::Builder propsText;
    Point initialPos;
    PdfSigCert* certs = nullptr;
    HWND parentHwnd = nullptr;
    // kept apart so a rebuilt text can put it back
    str::Builder fontsText;

    ~PropertiesWnd() override;
    bool Create(HWND parent);
    void UpdateTheme() override;
    void ApplyDarkMode() override;
    void SetPropsText(Str text);
    void SizeToContent();
    void CopyToClipboard(VirtMouseEvent* ev = nullptr);
    void ViewCertificate(VirtMouseEvent* ev = nullptr);
    void UpdateEutl(VirtMouseEvent* ev = nullptr);
    void OnCommand(WindowBase::CommandEvent* ev);
};

static Vec<PropertiesWnd*> gPropertiesWindows;

PropertiesWnd* FindPropertyWindowByHwnd(HWND hwnd) {
    for (PropertiesWnd* w : gPropertiesWindows) {
        if (w->hwnd == hwnd) {
            return w;
        }
        if (w->hwndParent == hwnd) {
            return w;
        }
    }
    return nullptr;
}

// Which action buttons the open properties dialog has.
TempStr PropertiesDialogButtonsTemp(int* exitCodeOut) {
    str::Builder out;
    auto finish = [&](Str msg, int code) -> TempStr {
        if (exitCodeOut) {
            *exitCodeOut = code;
        }
        out.Append(msg);
        return ToStrTemp(out);
    };
    if (len(gPropertiesWindows) == 0) {
        return finish(StrL("NOTREADY no-properties-window"), 2);
    }
    PropertiesWnd* w = gPropertiesWindows[0];
    return finish(fmt("copy=%d viewCert=%d updateEutl=%d", w->btnCopyToClipboard ? 1 : 0, w->btnViewCert ? 1 : 0,
                      w->btnUpdateEutl ? 1 : 0),
                  0);
}

// True for the properties dialog and its children (the read-only edit),
// but not the owner frame. Used so Home/End keep navigating the document
// while Properties stays open (issue #5971).
bool IsHwndInPropertiesWindow(HWND hwnd) {
    if (!hwnd) {
        return false;
    }
    for (PropertiesWnd* w : gPropertiesWindows) {
        if (w->hwnd == hwnd || (w->hwnd && IsChild(w->hwnd, hwnd))) {
            return true;
        }
    }
    return false;
}

static void SavePropertiesWindowPos(PropertiesWnd* w, HWND hwnd);

void DeletePropertiesWindow(HWND hwndParent) {
    PropertiesWnd* w = FindPropertyWindowByHwnd(hwndParent);
    if (!w) {
        return;
    }
    // Unregister first so a second closer cannot find this instance and schedule
    // another delete (CloseDocumentInCurrentTab then DeleteMainWindow).
    VecRemove(gPropertiesWindows, w);
    if (w->hwnd && IsWindow(w->hwnd)) {
        SavePropertiesWindowPos(w, w->hwnd);
        // Destroy HWND now (sync). Do not use Close()/PostMessage: that left the
        // object findable until WM_CLOSE ran and allowed a double ScheduleDelete.
        w->Destroy();
    }
    w->ScheduleDelete();
}

void PropertiesWnd::CopyToClipboard(VirtMouseEvent*) {
    CopyTextToClipboard(ToStr(propsText));
}

PropertiesWnd::~PropertiesWnd() {
    FreePdfSigCerts(certs);
    certs = nullptr;
}

// CryptUIDlgViewContext lives in cryptui.dll, loaded only when a certificate is viewed
void ViewCertDer(HWND parent, Str der) {
    if (len(der) == 0) {
        return;
    }
    PCCERT_CONTEXT cert =
        CertCreateCertificateContext(X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, (const BYTE*)der.s, (DWORD)len(der));
    if (!cert) {
        return;
    }
    using Fn = BOOL(WINAPI*)(DWORD, const void*, HWND, LPCWSTR, DWORD, void*);
    HMODULE h = LoadLibraryW(L"cryptui.dll");
    Fn fn = h ? (Fn)GetProcAddress(h, "CryptUIDlgViewContext") : nullptr;
    if (fn) {
        fn(CERT_STORE_CERTIFICATE_CONTEXT, cert, parent, L"Certificate", 0, nullptr);
    }
    if (h) {
        FreeLibrary(h);
    }
    CertFreeCertificateContext(cert);
}

void PropertiesWnd::ViewCertificate(VirtMouseEvent*) {
    if (!certs) {
        return;
    }
    if (!certs->next) {
        ViewCertDer(hwnd, certs->der);
        return;
    }
    HMENU menu = CreatePopupMenu();
    UINT_PTR id = 1;
    for (PdfSigCert* c = certs; c; c = c->next) {
        AppendMenuW(menu, MF_STRING, id, CWStrTemp(c->label));
        id++;
    }
    POINT pt{};
    GetCursorPos(&pt);
    int chosen = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, hwnd, nullptr);
    DestroyMenu(menu);
    if (chosen >= 1) {
        PdfSigCert* c = certs;
        for (int n = 1; c && n < chosen; n++) {
            c = c->next;
        }
        if (c) {
            ViewCertDer(hwnd, c->der);
        }
    }
}

// the signatures' text depends on the list, the fonts part doesn't
static void RebuildPropsText(PropertiesWnd* w) {
    MainWindow* win = FindMainWindowByHwnd(w->parentHwnd);
    if (!win || !win->ctrl) {
        return;
    }
    str::Builder text;
    GetPropsText(win->ctrl, text);
    AppendCertsText(text, w->certs);
    AlignPropertiesText(text);
    Str props = ToStr(w->propsText);
    int fonts = len(w->fontsText) > 0 ? str::IndexOf(props, ToStr(w->fontsText)) : -1;
    EndWithSingleNewline(text);
    if (fonts >= 0) {
        text.Append(ToStr(w->fontsText));
    }
    w->propsText.Reset();
    w->propsText.Append(ToStr(text));
    w->SetPropsText(ToStr(w->propsText));
}

void OnEutlUpdateDone(EutlUpdateJob* job) {
    if (job->refreshed) {
        for (PropertiesWnd* w : gPropertiesWindows) {
            if (w->certs) {
                RebuildPropsText(w);
            }
        }
    }
    if (!job->silent && job->hwnd && IsWindow(job->hwnd)) {
        MessageBoxW(job->hwnd, CWStrTemp(job->msg), L"EU Trusted List",
                    MB_OK | (job->ok ? MB_ICONINFORMATION : MB_ICONWARNING));
    }
    str::Free(job->msg);
    delete job;
}

void PropertiesWnd::UpdateEutl(VirtMouseEvent*) {
    auto* job = new EutlUpdateJob;
    job->hwnd = hwnd;
    auto fn = MkFunc0<EutlUpdateJob>(EutlUpdateThread, job);
    RunAsync(fn, StrL("EutlUpdate"));
}

void PropertiesWnd::SetPropsText(Str text) {
    if (!editProps) {
        return;
    }
    str::Builder crlfText;
    for (int i = 0; i < text.len; i++) {
        char c = text.s[i];
        if (c == '\n' && (i == 0 || text.s[i - 1] != '\r')) {
            crlfText.AppendChar('\r');
        }
        crlfText.AppendChar(c);
    }
    editProps->SetText(ToStr(crlfText));
    SendMessageW(editProps->hwnd, EM_SETSEL, 0, 0);
}

// as big as the text, up to 80% of the work area; the scrollbar only when
// the text doesn't fit
void PropertiesWnd::SizeToContent() {
    if (!editProps) {
        return;
    }
    int maxLineDx = 0;
    int nLines = 0;
    Str text = ToStr(propsText);
    for (int off = 0; off < text.len;) {
        Str rest = Str(text.s + off, text.len - off);
        int nl = str::IndexOfChar(rest, '\n');
        int lineLen = nl >= 0 ? nl : rest.len;
        Size size = PlatformFontMeasureText(propsFont, Str(rest.s, lineLen));
        maxLineDx = std::max(size.dx, maxLineDx);
        nLines++;
        off += lineLen + (nl >= 0 ? 1 : 0);
    }
    // a bit of slack so the longest lines don't touch the right edge
    maxLineDx += 4 * PlatformFontMeasureText(propsFont, StrL("x")).dx;

    int padDx = DpiScale(kToolDlgPadDx);
    int padDy = DpiScale(kPropsPadDy);
    int sbDx = DpiGetSystemMetrics(SM_CXVSCROLL);
    int footerDx = footer->MinIntrinsicWidth(0);
    int footerDy = footer->MinIntrinsicHeight(footerDx) + (2 * DpiScale(kToolDlgFootPadDy));
    int textDx = maxLineDx + editProps->GetLeftTextMargin() + sbDx;
    int wantedDx = std::max(textDx, footerDx) + (2 * padDx);

    Rect wr = HwndWindowRect(hwnd);
    Rect cr = HwndClientRect(hwnd);
    Rect work = GetWorkAreaRect(wr, hwnd);
    int maxDx = (work.dx * 80 / 100) - (wr.dx - cr.dx);
    int maxDy = (work.dy * 80 / 100) - (wr.dy - cr.dy);
    auto sizeForLines = [&](int lines) {
        int wantedDy = ((lines + 1) * PlatformFontLineHeight(propsFont)) + (2 * padDy) + footerDy;
        ShowScrollBar(editProps->hwnd, SB_VERT, wantedDy > maxDy);
        HwndResizeClientSize(hwnd, std::min(wantedDx, maxDx), std::min(wantedDy, maxDy));
        DoLayout();
    };
    sizeForLines(nLines);

    // a value wider than the window (an EPUB description) wraps into more lines
    int wrappedLines = (int)SendMessageW(editProps->hwnd, EM_GETLINECOUNT, 0, 0);
    if (wrappedLines > nLines) {
        sizeForLines(wrappedLines);
    }
}

void PropertiesWnd::ApplyDarkMode() {
    DarkModeApplyToWindowAndEraseBg(hwnd);
}

void PropertiesWnd::UpdateTheme() {
    WindowBase::UpdateTheme();
    // Re-apply monospaced font after darkmode child theming (may reset font).
    if (editProps && propsFont) {
        editProps->SetFont(propsFont);
    }
}

void PropertiesWnd::OnCommand(WindowBase::CommandEvent* ev) {
    auto cmd = LOWORD(ev->wparam);
    if (cmd == CmdCopySelection) {
        CopyToClipboard();
        ev->didHandle = true;
    }
}

static void SavePropertiesWindowPos(PropertiesWnd* w, HWND hwnd) {
    if (!w || !hwnd || !IsWindow(hwnd)) {
        return;
    }
    Rect rc = HwndWindowRect(hwnd);
    Point pos = {rc.x, rc.y};
    if (pos != w->initialPos) {
        // Only stash the point. SaveSettings() walks every tab and can re-enter
        // IsDocLoaded while CloseDocumentInCurrentTab has already cleared win->ctrl.
        gSettings->propWinPos = pos;
    }
}

// WM_CLOSE must clean up here: WindowBase::Destroy() clears hwnd and drops the WindowBase from
// the hwnd map before DestroyWindow(), so WM_DESTROY never reaches onDestroy.
static void OnPropertiesClose(WindowBase::CloseEvent* ev) {
    PropertiesWnd* w = (PropertiesWnd*)ev->e->self;
    if (!w || w->deleteScheduled) {
        return;
    }
    SavePropertiesWindowPos(w, w->hwnd);
    VecRemove(gPropertiesWindows, w);
    w->ScheduleDelete();
}

static void OnPropertiesDestroy(WindowBase::DestroyEvent* ev) {
    PropertiesWnd* w = (PropertiesWnd*)ev->e->self;
    // Fallback if the window was destroyed without WM_CLOSE (or Close already
    // scheduled the deferred free — then deleteScheduled is set and we no-op).
    if (!w || w->deleteScheduled) {
        return;
    }
    if (VecFind(gPropertiesWindows, w) >= 0) {
        SavePropertiesWindowPos(w, ev->e->hwnd);
        VecRemove(gPropertiesWindows, w);
    }
    w->ScheduleDelete();
}

bool PropertiesWnd::Create(HWND parent) {
    hwndParent = parent;
    bool isRtl = IsUIRtl();
    colTxt = ThemeWindowTextColor();
    colBg = ThemeWindowControlBackgroundColor();

    {
        CreateCustomArgs args;
        args.title = Tr("Document Properties");
        args.visible = false;
        args.style = WS_OVERLAPPEDWINDOW;
        args.font = DlgUiFont(kToolDlgTextFontPx);
        args.isRtl = isRtl;
        args.icon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(GetAppIconID()));
        args.bgColor = colBg;
        CreateCustom(args);
    }
    if (!hwnd) {
        return false;
    }

    SetColors(colTxt, colBg);
    HDC hdc = GetDC(hwnd);
    propsFont = HdcCreateSimpleFont(hdc, StrL("Consolas"), 14);
    ReleaseDC(hwnd, hdc);
    PlatformFont* fontText = DlgUiFont(kToolDlgTextFontPx);

    auto* vbox = new VBox();
    vbox->alignMain = MainAxisAlign::MainStart;
    vbox->alignCross = CrossAxisAlign::Stretch;

    {
        Edit::CreateArgs args;
        args.parent = hwnd;
        args.font = propsFont;
        args.isMultiLine = true;
        args.isRtl = isRtl;
        editProps = new Edit();
        editProps->SetColors(colTxt, colBg);
        editProps->Create(args);
        SendMessageW(editProps->hwnd, EM_SETREADONLY, TRUE, 0);
        // multi-line edit default can still cap text; font lists may be large
        SendMessageW(editProps->hwnd, EM_SETLIMITTEXT, 0, 0);
        DWORD tabStop = 16;
        SendMessageW(editProps->hwnd, EM_SETTABSTOPS, 1, (LPARAM)&tabStop);
        Insets pad = DpiScaledInsets(kPropsPadDy, 0, kPropsPadDy, kToolDlgPadDx);
        vbox->AddChild(new Padding(editProps, pad), 1);
    }

    {
        footer = new HBox();
        footer->alignCross = CrossAxisAlign::CrossCenter;
        footer->rtl = isRtl;
        footer->gap = DpiScale(kToolDlgGapDx);
        if (certs) {
            btnViewCert = NewDlgButton(hwnd, Tr("View Certificate..."), fontText, DlgButtonStyle::Ghost, false);
            btnViewCert->onClick = MkMethod1<PropertiesWnd, VirtMouseEvent*, &PropertiesWnd::ViewCertificate>(this);
            footer->AddChild(btnViewCert);
            btnUpdateEutl = NewDlgButton(hwnd, Tr("Update EU Trusted List"), fontText, DlgButtonStyle::Ghost, false);
            btnUpdateEutl->onClick = MkMethod1<PropertiesWnd, VirtMouseEvent*, &PropertiesWnd::UpdateEutl>(this);
            footer->AddChild(btnUpdateEutl);
        }
        footer->AddChild(new Spacer(0, 0), 1);
        btnCopyToClipboard = NewDlgButton(hwnd, Tr("Copy To Clipboard"), fontText, DlgButtonStyle::Filled, true);
        btnCopyToClipboard->onClick = MkMethod1<PropertiesWnd, VirtMouseEvent*, &PropertiesWnd::CopyToClipboard>(this);
        footer->AddChild(btnCopyToClipboard);
        vbox->AddChild(new Padding(footer, DpiScaledInsets(kToolDlgFootPadDy, kToolDlgPadDx)));
    }
    layout = vbox;

    SetPropsText(ToStr(propsText));
    SizeToContent();
    UpdateTheme();
    SetIsVisible(true);
    return true;
}

void OnGetFontsFinished(GetFontsResult* result) {
    PropertiesWnd* w = FindPropertyWindowByHwnd(result->hwnd);
    if (w) {
        Str marker = Tr("Getting font information...");
        Str props = ToStr(w->propsText);
        int pos = str::IndexOf(props, marker);
        if (pos >= 0) {
            w->propsText.RemoveAt(pos, len(w->propsText) - pos);
        }
        // fontsText starts with "\n" to separate it with a single empty line
        EndWithSingleNewline(w->propsText);
        w->propsText.Append(ToStr(result->fontsText));
        w->fontsText.Reset();
        w->fontsText.Append(ToStr(result->fontsText));
        w->SetPropsText(ToStr(w->propsText));
        w->SizeToContent();
    }
    delete result;
}

void ShowProperties(HWND parent, DocController* ctrl) {
    PropertiesWnd* w = FindPropertyWindowByHwnd(parent);
    if (w) {
        SetActiveWindow(w->hwnd);
        return;
    }

    if (!ctrl) {
        return;
    }

    auto* wnd = new PropertiesWnd();
    wnd->parentHwnd = parent;
    VecAppend(gPropertiesWindows, wnd);
    DisplayModel* dm = ctrl->AsFixed();
    EngineBase* engine = dm ? dm->GetEngine() : nullptr;
    if (EngineMupdfIsPdf(engine)) {
        EutlRegisterLookup();
        wnd->certs = EngineMupdfGetSignatureCerts(engine);
    }
    if (wnd->certs && !gForTesting && HasPermission(Perm::InternetAccess)) {
        // both only refresh what the text says: it's there already
        auto* job = new EutlUpdateJob;
        job->silent = true;
        job->update = EutlNeedsUpdate();
        job->engine = engine;
        engine->AddRef();
        RunAsync(MkFunc0<EutlUpdateJob>(EutlUpdateThread, job), StrL("SigOnlineChecks"));
    }
    GetPropsText(ctrl, wnd->propsText);
    AppendCertsText(wnd->propsText, wnd->certs);
    AlignPropertiesText(wnd->propsText);
    EndWithSingleNewline(wnd->propsText);
    wnd->propsText.Append(StrL("\n"));
    wnd->propsText.Append(Tr("Getting font information..."));

    wnd->closeOnEsc = true;
    wnd->onClose = MkFunc1Void<WindowBase::CloseEvent*>(OnPropertiesClose);
    wnd->onDestroy = MkFunc1Void<WindowBase::DestroyEvent*>(OnPropertiesDestroy);
    wnd->onCommand = MkMethod1<PropertiesWnd, WindowBase::CommandEvent*, &PropertiesWnd::OnCommand>(wnd);
    if (!wnd->Create(parent)) {
        VecRemove(gPropertiesWindows, wnd);
        delete wnd;
        return;
    }

    Point savedPos = gSettings->propWinPos;
    if (!savedPos.IsEmpty()) {
        SetWindowPos(wnd->hwnd, nullptr, savedPos.x, savedPos.y, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
        wnd->DoLayout();
    } else {
        HwndCenterDialog(wnd->hwnd, parent);
    }
    HwndEnsureOnScreen(wnd->hwnd);
    {
        Rect rc = HwndWindowRect(wnd->hwnd);
        wnd->initialPos = {rc.x, rc.y};
    }

    if (!dm || !dm->engine) {
        auto* result = new GetFontsResult;
        result->hwnd = wnd->hwnd;
        OnGetFontsFinished(result);
        return;
    }
    auto* data = new GetFontsData;
    data->hwnd = wnd->hwnd;
    data->engine = dm->engine;
    data->engine->AddRef();
    auto fn = MkFunc0<GetFontsData>(GetFontsThread, data);
    RunAsync(fn, StrL("GetFontsThread"));
}
