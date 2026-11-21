/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
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
#include "SumatraPDF.h"
#include "SvgIcons.h"
#include "Translations.h"
#include "EutlTrust.h"
#include "SignatureUICommon.h"

static const Color kColValid = MkRgb(0x1a, 0x7f, 0x37);
static const Color kColProblem = MkRgb(0xb5, 0x80, 0x00);
static const Color kColInvalid = MkRgb(0xc6, 0x28, 0x28);
static const Color kColBarProblemBg = MkRgb(0xff, 0xf3, 0xb0);
static const Color kColBarInvalidBg = MkRgb(0xff, 0xd6, 0xd6);
static const Color kColBarWarnText = MkRgb(0x20, 0x20, 0x20);

PdfSigResult* TabSignatures(WindowTab* tab) {
    return tab ? tab->signatures : nullptr;
}

PdfSigResult* CurrentSignatures(MainWindow* win) {
    return win ? TabSignatures(win->CurrentTab()) : nullptr;
}

bool TabHasSignatures(WindowTab* tab) {
    PdfSigResult* res = TabSignatures(tab);
    return res && len(res->entries) > 0;
}

SigVerdict OverallSigVerdict(const PdfSigSummary& sum) {
    if (sum.anyInvalid) {
        return SigVerdict::Invalid;
    }
    if (sum.anyUnknown) {
        return SigVerdict::Unknown;
    }
    return sum.nQualified > 0 ? SigVerdict::ValidQualified : SigVerdict::Valid;
}

Str OverallSigMessage(const PdfSigSummary& sum) {
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

const char* SigVerdictIcon(SigVerdict v) {
    switch (v) {
        case SigVerdict::Invalid:
            return gIconSigInvalid;
        case SigVerdict::Unknown:
            return gIconSigProblem;
        default:
            return gIconSigValid;
    }
}

Color SigVerdictColor(SigVerdict v) {
    switch (v) {
        case SigVerdict::Invalid:
            return kColInvalid;
        case SigVerdict::Unknown:
            return kColProblem;
        default:
            return kColValid;
    }
}

// a bar with problems stands out; a valid one looks like the rest of the window
void SigBarColors(SigVerdict v, Color* bg, Color* txt) {
    *bg = ThemeControlBackgroundColor();
    *txt = ThemeWindowTextColor();
    if (v == SigVerdict::Unknown || v == SigVerdict::Invalid) {
        *bg = v == SigVerdict::Invalid ? kColBarInvalidBg : kColBarProblemBg;
        *txt = kColBarWarnText;
    }
}

bool SigBarWanted(MainWindow* win) {
    PdfSigResult* res = CurrentSignatures(win);
    if (!res || res->sum.nSigned == 0 || res->barDismissed || !gSettings->showSignatureStatus) {
        return false;
    }
    return !win->IsFullScreenOrPresentation();
}

void GoToSignature(MainWindow* win, const PdfSigEntry* e) {
    if (win->ctrl && e->pageNo > 0 && e->pageNo <= win->ctrl->PageCount()) {
        win->ctrl->GoToPage(e->pageNo, true);
    }
}

// the lines of `all` that aren't in `validity`, without the signature's header
static void AppendSignerInfo(str::Builder& out, Str all, Str validity, Str nl) {
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
        out.Append(nl);
    }
}

static void AppendLines(str::Builder& out, Str text, Str nl) {
    StrVec lines;
    Split(&lines, text, StrL("\n"), true);
    for (Str line : lines) {
        str::TrimWsBoth(line);
        out.Append(line);
        out.Append(nl);
    }
}

// the Signature Properties dialog's text; nl ends each line
TempStr SignatureDialogTextTemp(const PdfSigEntry* e, Str nl) {
    str::Builder text;
    text.Append(Tr("Validity Summary"));
    text.Append(nl);
    AppendLines(text, e->validity, nl);
    text.Append(nl);
    text.Append(Tr("Signer Info"));
    text.Append(nl);
    AppendSignerInfo(text, e->details, e->validity, nl);
    return ToStrTemp(text);
}

#if OS_WIN
struct SigStatusJob {
    WindowTab* tab = nullptr;
    EngineBase* engine = nullptr;
    // only PDF signatures are checked against the EU trusted list
    bool needsEutl = false;
    // OfdTrustedCertificates, copied: the check runs on a thread
    StrVec trustedCerts;
    PdfSigResult* res = nullptr;
};

static void OnSigStatusDone(SigStatusJob* job) {
    WindowTab* tab = job->tab;
    EngineBase* engine = job->engine;
    PdfSigResult* res = job->res;
    delete job;
    defer {
        engine->Release();
    };
    if (!IsWindowTabValid(tab) || !tab->AsFixed() || tab->AsFixed()->GetEngine() != engine) {
        delete res;
        return;
    }
    // a reload: the bar stays closed if it was
    if (tab->signatures) {
        res->barDismissed = tab->signatures->barDismissed;
    }
    delete tab->signatures;
    tab->signatures = res;

    MainWindow* win = tab->win;
    if (IsCurrentTab(win, tab)) {
        OnTabSignaturesChecked(win);
    }
}

static void SigStatusThread(SigStatusJob* job) {
    if (job->needsEutl && EutlNeedsUpdate()) {
        EutlUpdate(nullptr);
    }
    Vec<Str> trusted;
    for (int i = 0; i < len(job->trustedCerts); i++) {
        VecAppend(trusted, job->trustedCerts.At(i));
    }
    job->res = EngineMupdfCheckSignatures(job->engine, &trusted);
    uitask::Post(MkFunc0<SigStatusJob>(OnSigStatusDone, job), "SigStatusDone");
}
#endif

// checks a signed document's signatures, for the bar, the sidebar view and the
// marks. On a thread: it hashes the file and may update the EU list.
void CheckTabSignatures(WindowTab* tab) {
#if OS_WIN
    if (gPluginMode || !tab || !tab->AsFixed()) {
        return;
    }
    EngineBase* engine = tab->AsFixed()->GetEngine();
    bool isPdf = EngineMupdfIsPdf(engine);
    bool hasSignatures = isPdf ? EngineMupdfCountSignedSignatures(engine) > 0 : EngineMupdfHasOfdSignatures(engine);
    if (!hasSignatures) {
        delete tab->signatures;
        tab->signatures = nullptr;
        return;
    }
    EutlRegisterLookup();
    auto* job = new SigStatusJob;
    job->tab = tab;
    job->engine = engine;
    job->needsEutl = isPdf;
    if (gSettings->ofdTrustedCertificates) {
        for (Str path : *gSettings->ofdTrustedCertificates) {
            job->trustedCerts.Append(path);
        }
    }
    engine->AddRef();
    RunAsync(MkFunc0<SigStatusJob>(SigStatusThread, job), StrL("SigStatus"));
#else
    (void)tab;
#endif
}
