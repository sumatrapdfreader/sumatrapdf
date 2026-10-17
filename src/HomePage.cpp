/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/AutoWin.h"
#include "base/File.h"
#include "base/Pixmap.h"
#include "base/Win.h"
#include "gui/Dpi.h"

#include "gui/UIModels.h"
#include "gui/Layout.h"
#include "gui/win/WinGui.h"
#include "gui/win/WebView.h"
#include "gui/PlatformFont.h"
#include "gui/Gfx.h"
#include "gui/GuiColors.h"
#include "gui/VirtCtrl.h"
#include "gui/VirtHost.h"

#include "Settings.h"
#include "DocController.h"
#include "SumatraConfig.h"
#include "FileHistory.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "Commands.h"
#include "Accelerators.h"
#include "CommandPalette.h"
#include "FilterHighlightDraw.h"
#include "FileThumbnails.h"
#include "Menu.h"
#include "Translations.h"
#include "Version.h"
#include "Theme.h"
#include "AppSettings.h"
#include "AppTools.h"
#include "DarkMode.h"
#include "SvgIcons.h"
#include "PagePosition.h"
#include "HomePage.h"
#include "HomePageCommon.h"

// how the shared tip code (TipText.cpp) opens a url link
static void OpenTipUrl(Str url) {
    // documentation links open in the embedded manual browser
    if (!MaybeLaunchDocumentation(url)) {
        SumatraLaunchBrowser(url);
    }
}

// what the shared tip code (TipText.cpp) knows about our commands: it names
// them in (Key/Cmd...) and in link targets, but knows nothing about the command
// table itself
struct SumatraCommandsContext : CommandsContext {
    TempStr GetCommandShortcutTemp(Str cmdName) override {
        int cmdId = GetCommandIdByName(cmdName);
        if (cmdId <= 0) {
            return {}; // not a command: the markup stays literal text
        }
        TempStr accel = AppendAccelKeyToMenuStringTemp(StrL(""), cmdId);
        if (len(accel) == 0 || !*accel.s) {
            return str::DupTemp(cmdName); // a command, but unbound
        }
        // AppendAccelKeyToMenuStringTemp prepends 	, skip it
        if (accel.s[0] == '	') {
            return Str(accel.s + 1);
        }
        return accel;
    }

    // `cmd` can carry arguments (e.g. "CmdFixDefaultApp .pdf"); those go through
    // CreateCommandFromDefinition so FrameOnCommand sees a CustomCommand with args
    void ExecuteCommand(HWND hwnd, Str cmd) override {
        CustomCommand* custom = CreateCommandFromDefinition(cmd);
        if (custom) {
            HwndSendCommand(hwnd, custom->id);
            return;
        }
        int cmdId = GetCommandIdByName(cmd);
        if (cmdId > 0) {
            HwndSendCommand(hwnd, cmdId);
        }
    }
};

static SumatraCommandsContext gSumatraCommandsContext;

struct TipHookInstaller {
    TipHookInstaller() {
        gTipOpenUrl = OpenTipUrl;
        gCommandsContext = &gSumatraCommandsContext;
    }
};
static TipHookInstaller gTipHookInstaller;

static Str sumatraPromos = StrL(R"promos(Try [Edna](https://edna.arslexis.io): a note taking web app for power users.
Try [MarkLexis](https://marklexis.arslexis.io): a bookmarking web application.
Try [Sumatra File Manager](https://sumatrafilemanager.com): file manager with super powers.
Try [Sumatra File Manager Online](https://online.sumatrafilemanager.com): file manager for S3, Dropbox and more.
)promos");

static Str promoFromServer;

static void EnsureTipsParsed() {
    if (gTipsParsed) {
        return;
    }
    CollectTipsFromString(sumatraTips, &gTipLines);
    CollectTipsFromString(sumatraPromos, &gPromoLines);
    gTipsParsed = true;
    PickRandomTipOrPromo();
}

static void ClearHomeLayoutCache(MainWindow*);

void FreeHomePageTips() {
    if (gTipsParsed) {
        gTipLines.Reset();
        gPromoLines.Reset();
        gTipsParsed = false;
    }
    str::Free(promoFromServer);
    HomePageInvalidateLayoutCache();
}

// the About box: the brand over the version, the link rows, a note, then a
// hairline over the copy link and the other software
constexpr int kAboutBoxDx = 380;
constexpr int kAboutPadTop = 28;
constexpr int kAboutPadSide = 32;
constexpr int kAboutPadBottom = 24;
constexpr int kAboutVersionGap = 2;
constexpr int kAboutGroupGap = 22;
constexpr int kAboutRowGap = 7;
constexpr int kAboutColGap = 20;
constexpr int kAboutFootGap = 18;
constexpr int kAboutFootRowGap = 10;
constexpr int kAboutSoftwareGap = 4;
constexpr int kAboutNoteMarkDx = 10;
constexpr int kAboutNoteMarkDy = 3;
constexpr int kAboutNoteGap = 8;
constexpr int kAboutTextFontSize = 13;
constexpr int kAboutMetaFontSize = 12;

constexpr int kInnerPadding = 8;
// the home page brand: five stripes in the logo colors and the name
static const Str kHomeBrandFont = StrL("Segoe UI Semibold");
constexpr int kHomeBrandFontSize = 15;
constexpr int kHomeBrandStripeDx = 3;
constexpr int kHomeBrandStripeDy = 16;
constexpr int kHomeBrandStripeGap = 2;
constexpr int kHomeBrandGap = 10;

constexpr int kLayoutLtr = 0;

static ATOM gAtomAbout;
static HWND gHwndAbout;
static VirtRoot* gAboutRoot = nullptr;
static Tooltip* gAboutTooltip = nullptr;
static Str gClickedURL;

// one row of the About screen's two-column table
struct AboutRow {
    Str leftTxt;
    Str rightTxt;
    Str url;
};

static AboutRow gAboutRows[] = {
    {StrL("manual"), StrL("SumatraPDF manual"), StrL("https://www.sumatrapdfreader.org/docs/SumatraPDF-documentation")},
    {StrL("version history"), StrL("What's new"), StrL("https://www.sumatrapdfreader.org/docs/Version-history")},
    {StrL("website"), StrL("SumatraPDF website"), Str(kWebsiteURL)},
    {StrL("forums"), StrL("SumatraPDF forums"), StrL("https://github.com/sumatrapdfreader/sumatrapdf/discussions")},
    {StrL("licenses"), StrL("Various Open Source"),
     StrL("https://github.com/sumatrapdfreader/sumatrapdf/blob/master/AUTHORS")},
#ifdef GIT_COMMIT_ID_STR
    {StrL("last change"), StrL("git commit " GIT_COMMIT_ID_STR),
     StrL("https://github.com/sumatrapdfreader/sumatrapdf/commit/" GIT_COMMIT_ID_STR)},
#endif
    {StrL("more software by me"), StrL("Sumatra File Manager"), StrL("https://sumatrafilemanager.com")},
    {{}, StrL("Sumatra File Manager Online"), StrL("https://online.sumatrafilemanager.com")},
    {{}, {}, {}}};

// the rows from kAboutSoftwareRow on are the other software, in the footer
constexpr int kAboutSoftwareEnd = dimofi(gAboutRows) - 1;
constexpr int kAboutSoftwareRow = kAboutSoftwareEnd - 2;

// the line under the rows that says this isn't a release build
static Str AboutNoteText() {
#if defined(PRE_RELEASE_VER)
    return StrL("pre-release build");
#elif IS_DEBUG
    return StrL("debug build, for testing only");
#else
    return {};
#endif
}

// The About box, the same in the About window, the home page's dropdown and
// the About page: the brand over the version line, label / link rows, the
// build note, then a hairline over the copy link and the other software.
// Rows with a url are VirtLink (hit-testing, hand cursor, tooltip)
static Kind kindAboutCtrl = "aboutCtrl";

struct SumatraLogo;

struct AboutCtrl : VirtCtrl {
    SumatraLogo* logo = nullptr;
    VirtText* version = nullptr;
    Vec<VirtText*> labels;
    Vec<VirtText*> values;
    VirtText* note = nullptr;
    // copies version / OS / machine info for bug reports
    VirtLink* copyLink = nullptr;
    VirtText* softwareHeader = nullptr;
    Vec<VirtLink*> softwareLinks;
    // "Show frequently read", bottom right of the About page (not the window)
    VirtLink* showFreqRead = nullptr;
    // the dropdown has a hairline around it; the window and the page don't
    bool withBorder = false;

    // geometry, computed by UpdateLayout()
    Rect aboutRect;
    int hairlineY = 0;
    Rect noteMark;

    AboutCtrl();
    void Sync();
    void UpdateLayout(Rect clientRc);
    void Paint(VirtPaintCtx&) override;
};

static void OpenAboutUrl(VirtMouseEvent* ev) {
    auto* link = (VirtLink*)ev->target;
    OpenTipUrl(link->target);
}

void SetPromoString(Str s) {
    if (len(s) == 0) return;
    str::ReplaceWithCopy(&promoFromServer, s);
}

static TempStr GetAppVersionTemp() {
    TempStr s = str::DupTemp(StrL("v" CURR_VERSION_STRA));
    if (IsProcess64()) {
        s = str::JoinTemp(s, StrL(" 64-bit"));
    } else {
        s = str::JoinTemp(s, StrL(" 32-bit"));
    }
    if (gIsDebugBuild) {
        s = str::JoinTemp(s, StrL(" (dbg)"));
    }
    return s;
}

// "3.7 64-bit, built Oct 14 2026 01:46"
static TempStr AboutVersionLineTemp() {
    Str bits = IsProcess64() ? StrL("64-bit") : StrL("32-bit");
    Str built = StrL(__DATE__ " " __TIME__);
    // without the seconds
    built.len = std::max(0, len(built) - 3);
    return fmt("%s %s, built %s", StrL(CURR_VERSION_STRA), bits, built);
}

constexpr Color kCol1 = MkRgb(196, 64, 50);
constexpr Color kCol2 = MkRgb(227, 107, 35);
constexpr Color kCol3 = MkRgb(93, 160, 40);
constexpr Color kCol4 = MkRgb(69, 132, 190);
constexpr Color kCol5 = MkRgb(112, 115, 207);

static Kind kindSumatraLogo = "sumatraLogo";

// the brand: the five logo colors as stripes before the app name in the
// window's text color
struct SumatraLogo : VirtCtrl {
    PlatformFont* font = nullptr; // not owned

    SumatraLogo();
    Size GetIdealSize() override;
    void Paint(VirtPaintCtx&) override;
};

SumatraLogo::SumatraLogo() {
    kind = kindSumatraLogo;
    flags |= vwfNoHitTest;
}

static int HomeBrandStripesDx() {
    return (5 * DpiScale(kHomeBrandStripeDx)) + (4 * DpiScale(kHomeBrandStripeGap));
}

static PlatformFont* HomeBrandFont() {
    return GetUserGuiFont(kHomeBrandFont, DpiScale(kHomeBrandFontSize));
}

Size SumatraLogo::GetIdealSize() {
    Size sz = PlatformFontMeasureText(font, StrL(kAppName));
    sz.dx += HomeBrandStripesDx() + DpiScale(kHomeBrandGap);
    sz.dy = std::max(sz.dy, DpiScale(kHomeBrandStripeDy));
    return sz;
}

void SumatraLogo::Paint(VirtPaintCtx& ctx) {
    static Color cols[] = {kCol1, kCol2, kCol3, kCol4, kCol5};
    Size txtSize = PlatformFontMeasureText(font, StrL(kAppName));
    Rect r = ctx.bounds;
    int stripeDx = DpiScale(kHomeBrandStripeDx);
    int stripeDy = DpiScale(kHomeBrandStripeDy);
    int x = r.x;
    int y = r.y + ((r.dy - stripeDy) / 2);
    for (int i = 0; i < 5; i++) {
        ctx.gfx->FillRect({x, y, stripeDx, stripeDy}, cols[i]);
        x += stripeDx + DpiScale(kHomeBrandStripeGap);
    }
    x = r.x + HomeBrandStripesDx() + DpiScale(kHomeBrandGap);
    Rect rcName(x, r.y + ((r.dy - txtSize.dy) / 2), txtSize.dx, txtSize.dy);
    ctx.gfx->DrawText(StrL(kAppName), rcName, 0, font, ThemeWindowTextColor());
}

// the About screen's virtual controls for one HWND. Positions come from
// AboutCtrl::UpdateLayout(), so the root must not run a layout of its own
static AboutCtrl* EnsureAboutCtrl(VirtRoot** rootPtr, HWND hwnd, Rect clientRc) {
    VirtRoot* root = *rootPtr;
    if (!root) {
        root = new VirtRoot(hwnd);
        *rootPtr = root;
    }
    if (!IsVirtCtrlOfKind(root->owned, kindAboutCtrl)) {
        root->SetChild(new AboutCtrl());
    }
    root->bounds = clientRc;
    root->needsLayout = false;
    auto* about = (AboutCtrl*)root->owned;
    about->SetBounds(clientRc);
    return about;
}

static PlatformFont* HomePageFont(int size);
static void OnCopyProgramInfo(VirtMouseEvent*);

AboutCtrl::AboutCtrl() {
    kind = kindAboutCtrl;
    flags |= vwfNoHitTest;
    logo = new SumatraLogo();
    AddChild(logo);
    version = new VirtText({});
    AddChild(version);
    note = new VirtText({});
    AddChild(note);
    copyLink = new VirtLink(Tr("Copy program and machine info"));
    copyLink->name = StrL("aboutCopy");
    copyLink->withUnderline = true;
    copyLink->underlineOffsetY = -3;
    copyLink->onClick = MkFunc1Void(OnCopyProgramInfo);
    AddChild(copyLink);
    AboutRow& software = gAboutRows[kAboutSoftwareRow];
    softwareHeader = new VirtText(software.leftTxt);
    AddChild(softwareHeader);
    for (int i = kAboutSoftwareRow; i < kAboutSoftwareEnd; i++) {
        auto* link = new VirtLink(gAboutRows[i].rightTxt);
        link->SetTarget(gAboutRows[i].url);
        link->SetTooltip(gAboutRows[i].url);
        link->withUnderline = true;
        link->underlineOffsetY = -3;
        link->onClick = MkFunc1Void(OpenAboutUrl);
        AddChild(link);
        VecAppend(softwareLinks, link);
    }
}

// the hairline over the footer, the note's mark and the dropdown's frame
void AboutCtrl::Paint(VirtPaintCtx& ctx) {
    Rect r = aboutRect;
    if (r.IsEmpty()) {
        return;
    }
    ctx.gfx->FillRect(r, ThemeMainWindowBackgroundColor());
    if (withBorder) {
        ctx.gfx->DrawRect(r, ThemeEdgeColor());
    }
    int padSide = DpiScale(kAboutPadSide);
    ctx.gfx->FillRect({r.x + padSide, hairlineY, r.dx - (2 * padSide), 1}, ThemeEdgeColor());
    if (!noteMark.IsEmpty()) {
        ctx.gfx->FillRect(noteMark, ThemeAccentColor());
    }
}

// build the rows once, then keep text, fonts and colors in step with the theme
// and the DPI. Sizing happens in UpdateLayout(), which measures what we set here
void AboutCtrl::Sync() {
    int n = kAboutSoftwareRow;
    bool canAccessDisk = CanAccessDisk();
    if (len(labels) == 0) {
        for (int i = 0; i < n; i++) {
            AboutRow* el = &gAboutRows[i];
            auto* label = new VirtText(el->leftTxt);
            label->align = VirtTextAlign::Right;
            AddChild(label);
            VecAppend(labels, label);
            auto* link = new VirtLink(TrimGitTemp(el->rightTxt));
            link->SetTarget(el->url);
            link->SetTooltip(el->url);
            link->underlineOffsetY = -3;
            link->onClick = MkFunc1Void(OpenAboutUrl);
            AddChild(link);
            VecAppend(values, link);
        }
    }

    logo->font = HomeBrandFont();
    PlatformFont* fontText = HomePageFont(kAboutTextFontSize);
    PlatformFont* fontMeta = HomePageFont(kAboutMetaFontSize);
    Color colText = ThemeWindowTextColor();
    Color colMuted = ThemeWindowTextDisabledColor();

    version->SetText(AboutVersionLineTemp());
    version->font = fontMeta;
    version->SetColor(kColText, colMuted);

    // a link only when the url can be opened: without disk access it is text
    auto syncLink = [&](VirtText* link) {
        link->font = fontText;
        link->SetColor(kColText, colText);
        link->underlineColor = colMuted;
        link->withUnderline = canAccessDisk;
        link->SetFlag(vwfNoHitTest, !canAccessDisk);
    };
    for (int i = 0; i < n; i++) {
        labels[i]->font = fontText;
        labels[i]->SetColor(kColText, colMuted);
        syncLink(values[i]);
    }

    Str noteTxt = AboutNoteText();
    note->SetText(noteTxt);
    note->font = fontMeta;
    note->SetColor(kColText, colMuted);
    note->SetVisibility(len(noteTxt) > 0 ? Visibility::Visible : Visibility::Collapse);

    copyLink->font = fontText;
    copyLink->SetColor(kColText, colText);
    copyLink->underlineColor = colMuted;
    softwareHeader->font = fontMeta;
    softwareHeader->SetColor(kColText, colMuted);
    for (VirtLink* link : softwareLinks) {
        syncLink(link);
    }
}

// the box is kAboutBoxDx wide (more when a row needs it) and as tall as its
// content; it is centered in clientRc
void AboutCtrl::UpdateLayout(Rect clientRc) {
    int padTop = DpiScale(kAboutPadTop);
    int padSide = DpiScale(kAboutPadSide);
    int padBottom = DpiScale(kAboutPadBottom);
    int groupGap = DpiScale(kAboutGroupGap);
    int rowGap = DpiScale(kAboutRowGap);
    int colGap = DpiScale(kAboutColGap);

    Size logoSz = logo->GetIdealSize();
    version->sz = {};
    Size verSz = version->GetIdealSize(true);
    int n = len(labels);
    int labelDx = 0;
    int valueDx = 0;
    int rowsDy = 0;
    for (int i = 0; i < n; i++) {
        labels[i]->sz = {};
        values[i]->sz = {};
        Size l = labels[i]->GetIdealSize(true);
        Size v = values[i]->GetIdealSize(true);
        labelDx = std::max(labelDx, l.dx);
        valueDx = std::max(valueDx, v.dx);
        rowsDy += std::max(l.dy, v.dy) + (i > 0 ? rowGap : 0);
    }
    bool showNote = note->GetVisibility() != Visibility::Collapse;
    note->sz = {};
    Size noteSz = showNote ? note->GetIdealSize(true) : Size{};
    copyLink->sz = {};
    softwareHeader->sz = {};
    Size copySz = copyLink->GetIdealSize(true);
    Size hdrSz = softwareHeader->GetIdealSize(true);
    int linksDy = 0;
    for (VirtLink* link : softwareLinks) {
        link->sz = {};
        linksDy += DpiScale(kAboutSoftwareGap) + link->GetIdealSize(true).dy;
    }

    Rect r;
    r.dx = std::max(DpiScale(kAboutBoxDx), labelDx + colGap + valueDx + (2 * padSide));
    r.dy = padTop + logoSz.dy + DpiScale(kAboutVersionGap) + verSz.dy + groupGap + rowsDy;
    if (showNote) {
        r.dy += groupGap + noteSz.dy;
    }
    r.dy += groupGap + 1 + DpiScale(kAboutFootGap) + copySz.dy + DpiScale(kAboutFootRowGap) + hdrSz.dy + linksDy +
            padBottom;
    r.x = clientRc.x + ((clientRc.dx - r.dx) / 2);
    r.y = clientRc.y + ((clientRc.dy - r.dy) / 2);
    aboutRect = r;

    int x = r.x + padSide;
    int innerDx = r.dx - (2 * padSide);
    int y = r.y + padTop;
    logo->SetBounds({x, y, logoSz.dx, logoSz.dy});
    y += logoSz.dy + DpiScale(kAboutVersionGap);
    version->SetBounds({x, y, verSz.dx, verSz.dy});
    y += verSz.dy + groupGap;

    int valueX = x + labelDx + colGap;
    for (int i = 0; i < n; i++) {
        Size l = labels[i]->GetIdealSize(true);
        Size v = values[i]->GetIdealSize(true);
        int rowDy = std::max(l.dy, v.dy);
        labels[i]->SetBounds({x, y + ((rowDy - l.dy) / 2), labelDx, l.dy});
        values[i]->SetBounds({valueX, y + ((rowDy - v.dy) / 2), v.dx, v.dy});
        y += rowDy + rowGap;
    }
    y -= rowGap;

    noteMark = {};
    if (showNote) {
        y += groupGap;
        int markDx = DpiScale(kAboutNoteMarkDx);
        int lineDx = markDx + DpiScale(kAboutNoteGap) + noteSz.dx;
        int lineX = x + ((innerDx - lineDx) / 2);
        noteMark = {lineX, y + ((noteSz.dy - DpiScale(kAboutNoteMarkDy)) / 2), markDx, DpiScale(kAboutNoteMarkDy)};
        note->SetBounds({lineX + markDx + DpiScale(kAboutNoteGap), y, noteSz.dx, noteSz.dy});
        y += noteSz.dy;
    }

    y += groupGap;
    hairlineY = y;
    y += 1 + DpiScale(kAboutFootGap);
    copyLink->SetBounds({x + ((innerDx - copySz.dx) / 2), y, copySz.dx, copySz.dy});
    y += copySz.dy + DpiScale(kAboutFootRowGap);
    softwareHeader->SetBounds({x + ((innerDx - hdrSz.dx) / 2), y, hdrSz.dx, hdrSz.dy});
    y += hdrSz.dy;
    for (VirtLink* link : softwareLinks) {
        Size linkSz = link->GetIdealSize(true);
        y += DpiScale(kAboutSoftwareGap);
        link->SetBounds({x + ((innerDx - linkSz.dx) / 2), y, linkSz.dx, linkSz.dy});
        y += linkSz.dy;
    }
}

// Version, OS, WebView2, memory and similar facts for a bug report.
static void AppendBugReportInfo(str::Builder& s) {
    s.Append(fmt("SumatraPDF %s\n", GetAppVersionTemp()));
    s.Append(fmt("Built on: %s %s\n", StrL(__DATE__), StrL(__TIME__)));
    if (gitCommidId) {
        s.Append(fmt("Git: %s\n", gitCommidId));
    }
    Str exeType = IsDllBuild() ? StrL("dll") : StrL("static");
    Str instType = IsRunningInPortableMode() ? StrL("portable") : StrL("installed");
    s.Append(fmt("Type: %s, %s\n", exeType, instType));
    if (gIsPreReleaseBuild) {
        s.Append(StrL("Pre-release: yes\n"));
    }
    if (gIsAsanBuild) {
        s.Append(StrL("ASan: yes\n"));
    }

    OSVERSIONINFOEX ver{};
    if (GetOsVersion(ver)) {
        TempStr os = OsNameFromVerTemp(ver);
        int buildNumber = (int)ver.dwBuildNumber & 0xFFFF;
        Str arch = StrL("64-bit");
        if (IsProcess32()) {
            arch = IsRunningInWow64() ? StrL("32-bit (Wow64)") : StrL("32-bit");
        }
        s.Append(fmt("OS: Windows %s, build %d, %s\n", os, buildNumber, arch));
    }
    if (IsOs64()) {
        s.Append(StrL("OS architecture: 64-bit\n"));
    } else {
        s.Append(StrL("OS architecture: 32-bit\n"));
    }

    TempStr wv = GetWebView2VersionTemp();
    if (len(wv) == 0) {
        s.Append(StrL("WebView2: not installed\n"));
    } else {
        s.Append(fmt("WebView2: %s\n", wv));
    }

    MEMORYSTATUSEX ms{};
    ms.dwLength = sizeof(ms);
    if (GlobalMemoryStatusEx(&ms)) {
        float physMemGB = (float)ms.ullTotalPhys / (float)(1024 * 1024 * 1024);
        s.Append(fmt("Physical memory: %.2f GB (%d%% in use)\n", physMemGB, (int)ms.dwMemoryLoad));
    }

    SYSTEM_INFO si{};
    GetSystemInfo(&si);
    s.Append(fmt("Processors: %d\n", (int)si.dwNumberOfProcessors));
    TempStr cpuName = ReadRegStrTemp(HKEY_LOCAL_MACHINE, StrL(R"(HARDWARE\DESCRIPTION\System\CentralProcessor\0)"),
                                     StrL("ProcessorNameString"));
    if (cpuName) {
        s.Append(fmt("Processor: %s\n", cpuName));
    }

    int screenDx = GetSystemMetrics(SM_CXSCREEN);
    int screenDy = GetSystemMetrics(SM_CYSCREEN);
    int dpi = DpiGet();
    s.Append(fmt("Screen: %dx%d, DPI %d (%d%%)\n", screenDx, screenDy, dpi, MulDiv(dpi, 100, 96)));

    char country[32] = {}, lang[32]{};
    GetLocaleInfoA(LOCALE_USER_DEFAULT, LOCALE_SISO3166CTRYNAME, country, dimof(country) - 1);
    GetLocaleInfoA(LOCALE_USER_DEFAULT, LOCALE_SISO639LANGNAME, lang, dimof(lang) - 1);
    s.Append(fmt("Locale: %s-%s\n", Str(lang), Str(country)));

    Str theme = ThemeGetNameAt(ThemeGetCurrentIndex());
    if (theme) {
        s.Append(fmt("Theme: %s\n", theme));
    }
}

static void CopyAboutInfoToClipboard() {
    str::Builder info;
    info.Reserve(1024);
    AppendBugReportInfo(info);
    CopyTextToClipboard(ToStr(info));
}

static void OnCopyProgramInfo(VirtMouseEvent*) {
    CopyAboutInfoToClipboard();
}

// prepares the About tree for hwnd and computes its geometry
static AboutCtrl* UpdateAboutLayout(VirtRoot** rootPtr, HWND hwnd, Rect clientRc, bool hover = false) {
    DpiSetFromHwnd(hwnd);
    AboutCtrl* about = EnsureAboutCtrl(rootPtr, hwnd, clientRc);
    about->withBorder = hover;
    about->Sync();
    about->UpdateLayout(clientRc);
    return about;
}

static void DrawAbout(Gfx* gfx, VirtRoot* root, Rect clientRc) {
    gfx->FillRect(clientRc, ThemeMainWindowBackgroundColor());
    root->Paint(gfx, clientRc);
}

static void OnPaintAbout(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    SetLayout(hdc, kLayoutLtr);
    Rect clientRc = HwndClientRect(hwnd);
    UpdateAboutLayout(&gAboutRoot, hwnd, clientRc);
    Gfx* gfx = GfxCreate(hdc);
    DrawAbout(gfx, gAboutRoot, clientRc);
    delete gfx;
    EndPaint(hwnd, &ps);
}

static void CreateInfotipForLink(Str tooltip, const Rect& rc) {
    if (gAboutTooltip != nullptr) {
        return;
    }

    Tooltip::CreateArgs args;
    args.parent = gHwndAbout;
    args.font = GetAppFont();
    args.isRtl = IsUIRtl();

    gAboutTooltip = new Tooltip();
    gAboutTooltip->Create(args);
    gAboutTooltip->SetSingle(tooltip, rc, false);
}

static void DeleteInfotip() {
    if (gAboutTooltip == nullptr) {
        return;
    }
    // gAboutTooltip->Hide();
    delete gAboutTooltip;
    gAboutTooltip = nullptr;
}

static LRESULT CALLBACK WndProcAbout(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    Point pt;

    // the links are VirtLinks: let the tree hit-test, click and set the
    // cursor. Its GetTooltipTemp() drives this window's own Tooltip
    if (gAboutRoot && gAboutRoot->owned) {
        LRESULT res = 0;
        switch (msg) {
            case WM_MOUSEMOVE:
            case WM_MOUSELEAVE:
            case WM_LBUTTONDOWN:
            case WM_LBUTTONUP:
                if (gAboutRoot->OnMessage(msg, wp, lp, res)) {
                    return res;
                }
                break;
            case WM_SETCURSOR: {
                pt = HwndGetCursorPos(hwnd);
                Point ptLocal{0, 0};
                ILayout* el = ElementFromPoint(gAboutRoot, pt, &ptLocal);
                VirtCtrl* w = el ? el->AsVirtCtrl() : nullptr;
                if (w && w->OnSetCursor(ptLocal)) {
                    TempStr tip = w->GetTooltipTemp(ptLocal);
                    if (tip && *tip.s) {
                        Rect r = w->BoundsInWindow();
                        CreateInfotipForLink(tip, r);
                    }
                    return TRUE;
                }
                DeleteInfotip();
                return DefWindowProc(hwnd, msg, wp, lp);
            }
        }
    }

    switch (msg) {
        case WM_CREATE:
            ReportIf(gHwndAbout);
            DarkModeApplyToTitleBar(hwnd);
            break;

        case WM_ERASEBKGND:
            // do nothing, helps to avoid flicker
            return TRUE;

        case WM_PAINT:
            OnPaintAbout(hwnd);
            break;

        case WM_SETCURSOR:
            DeleteInfotip();
            return DefWindowProc(hwnd, msg, wp, lp);

        case WM_CHAR:
            if (VK_ESCAPE == wp) {
                DestroyWindow(hwnd);
            }
            break;

        case WM_KEYDOWN:
            if ('C' == wp && IsCtrlPressed()) {
                CopyAboutInfoToClipboard();
            }
            break;

        case WM_COMMAND:
            if (CmdCopySelection == LOWORD(wp)) {
                CopyAboutInfoToClipboard();
            }
            break;

        case WM_DESTROY:
            DeleteInfotip();
            delete gAboutRoot;
            gAboutRoot = nullptr;
            ReportIf(!gHwndAbout);
            gHwndAbout = nullptr;
            break;

        default:
            return DefWindowProc(hwnd, msg, wp, lp);
    }
    return 0;
}

constexpr const WCHAR* kAboutClassName = L"SUMATRA_PDF_ABOUT";

void ShowAboutWindow(MainWindow* win) {
    if (gHwndAbout) {
        SetActiveWindow(gHwndAbout);
        return;
    }

    if (!gAtomAbout) {
        WNDCLASSEX wcex;
        FillWndClassEx(wcex, kAboutClassName, WndProcAbout);
        HMODULE h = GetModuleHandleW(nullptr);
        wcex.hIcon = LoadIcon(h, MAKEINTRESOURCE(GetAppIconID()));
        gAtomAbout = RegisterClassEx(&wcex);
        ReportIf(!gAtomAbout);
    }

    WCHAR* title = CWStrTemp(Tr("About SumatraPDF"));
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU;
    int x = CW_USEDEFAULT;
    int y = CW_USEDEFAULT;
    int dx = CW_USEDEFAULT;
    int dy = CW_USEDEFAULT;
    HINSTANCE h = GetModuleHandleW(nullptr);
    gHwndAbout = CreateWindowExW(0, kAboutClassName, title, style, x, y, dx, dy, nullptr, nullptr, h, nullptr);
    if (!gHwndAbout) {
        return;
    }

    HwndSetRtl(gHwndAbout, IsUIRtl());

    // the client area is the box
    AboutCtrl* about = UpdateAboutLayout(&gAboutRoot, gHwndAbout, HwndClientRect(gHwndAbout));
    dx = about->aboutRect.dx;
    dy = about->aboutRect.dy;

    // resize the new window to just match these dimensions
    Rect wRc = HwndWindowRect(gHwndAbout);
    Rect cRc = HwndClientRect(gHwndAbout);
    wRc.dx += dx - cRc.dx;
    wRc.dy += dy - cRc.dy;
    MoveWindow(gHwndAbout, wRc.x, wRc.y, wRc.dx, wRc.dy, FALSE);

    HwndPositionInCenterOf(gHwndAbout, win->hwndFrame);
    ShowWindow(gHwndAbout, SW_SHOW);
}

static void ShowFrequentlyRead(VirtMouseEvent* ev) {
    auto* win = (MainWindow*)ev->target->userData;
    gSettings->showStartPage = true;
    HomePageRelayout(win);
    win->RedrawAll(true);
}

void DrawAboutPage(MainWindow* win, Gfx* gfx) {
    HWND hwnd = win->hwndCanvas;
    Rect clientRc = HwndClientRect(hwnd);
    AboutCtrl* about = UpdateAboutLayout(&win->homeRoot, hwnd, clientRc);

    bool showLink = HasPermission(Perm::SavePreferences | Perm::DiskAccess) && SettingsRememberOpenedFiles();
    if (showLink && !about->showFreqRead) {
        auto* link = new VirtLink(Tr("Show frequently read"));
        link->withUnderline = true;
        link->isRtl = IsUIRtl();
        link->userData = (uintptr_t)win;
        link->onClick = MkFunc1Void(ShowFrequentlyRead);
        about->showFreqRead = link;
        about->AddChild(link);
    }
    if (about->showFreqRead) {
        VirtLink* link = about->showFreqRead;
        link->visibility = showLink ? Visibility::Visible : Visibility::Collapse;
        link->font = HomePageFont(kAboutTextFontSize);
        link->SetColor(kColText, ThemeWindowTextColor());
        link->underlineColor = ThemeWindowTextDisabledColor();
        link->sz = {0, 0}; // re-measure: the font may have changed with the DPI
        Size txtSize = link->GetIdealSize(true);
        Rect r = {0, 0, txtSize.dx, txtSize.dy};
        PositionRB(clientRc, r);
        MoveXY(r, -DpiScale(kInnerPadding), -DpiScale(kInnerPadding));
        link->SetBounds(r);
    }
    DrawAbout(gfx, win->homeRoot, clientRc);
}

/* alternate static page to display when no document is loaded */

constexpr int kThumbsSeparatorDy = 2;
constexpr int kThumbsBorderDx = 1;
#define kThumbsMarginLeft DpiScale(40)
#define kThumbsMarginRight DpiScale(40)
#define kThumbsMarginTop DpiScale(50)
#define kThumbsMarginBottom DpiScale(40)
#define kThumbsSpaceBetweenX DpiScale(24)
#define kThumbsSpaceBetweenY DpiScale(60)
// caption below each thumbnail: the file name over the page count
#define kThumbsCaptionGapY DpiScale(10)
#define kThumbsCaptionDy DpiScale(34)
#define kThumbsCaptionLineDy DpiScale(18)
// the reading-progress line along the bottom edge of a thumbnail
#define kThumbsProgressDy DpiScale(3)
// the selection ring sits this far outside the page
#define kThumbsSelectionGap DpiScale(4)
#define kThumbsBottomBoxDy DpiScale(50)
#define kHomeListThumbDx DpiScale(30)
#define kHomeListThumbDy DpiScale(40)
#define kHomeListRowDy DpiScale(46)
#define kHomeListRowGapDx DpiScale(8)

// ThumbnailLayout::fileSize cache: AppendBlanks zero-fills, so set
// kSizeNotFetched after each AppendBlanks (default member init never runs).
constexpr i64 kSizeNotFetched = -2;
constexpr i64 kSizeFetchFail = -1;

struct ThumbnailLayout {
    Rect rcCell; // the grid slot; rcPage is the thumbnail inside it
    Rect rcPage;
    Size szThumb;
    Rect rcText;
    Rect rcListRow;
    Rect rcListThumb;
    Rect rcListFileName;
    Rect rcListPath;
    Rect rcListSize;
    Rect rcListProgress;
    Rect rcListRemove;
    Rect rcListPin;
    FileState* fs = nullptr; // info needed to draw the thumbnail
    // Cached file::GetSize() so we don't hit the disk on every paint.
    // AppendBlanks zero-fills, so set to kSizeNotFetched after AppendBlanks.
    i64 fileSize = kSizeNotFetched;
    // false until MeasureHomeListRowText() has split rcListFileName into name +
    // directory. Also relies on AppendBlanks zero-fill, so false must mean "not
    // measured yet"
    bool listTextMeasured = false;
};

static TempStr FileSizeForHomeListTemp(i64 size);

// true if r overlaps the visible thumbs band (optionally with a small margin)
static bool IsHomeThumbOnScreen(const Rect& r, const Rect& thumbsArea, int marginY = 0) {
    if (r.IsEmpty() || thumbsArea.IsEmpty()) {
        return false;
    }
    Rect band = thumbsArea;
    if (marginY > 0) {
        band.y -= marginY;
        band.dy += 2 * marginY;
    }
    return !r.Intersect(band).IsEmpty();
}

// HomePageViewMode setting ("thumbnails" or "list")
bool HomePageIsListView() {
    return gSettings && str::EqI(gSettings->homePageViewMode, StrL("list"));
}

void SetHomePageListView(bool listView) {
    Str mode = listView ? StrL("list") : StrL("thumbnails");
    str::ReplaceWithCopy(&gSettings->homePageViewMode, mode);
}

struct HomePageLayout {
    // args in
    Gfx* gfx = nullptr;
    Rect rc;
    MainWindow* win = nullptr;

    Rect rcIconOpen;
    Rect rcIconListView;
    Rect rcIconThumbnailView;
    Rect rcLogo;

    VirtText* freqRead = nullptr;
    VirtText* openDoc = nullptr;
    VirtText* hideShowFreqRead = nullptr;
    Vec<ThumbnailLayout> thumbnails; // info for each thumbnail
    int totalContentDy = 0;          // total height of all thumbnail rows
    int thumbsVisibleDy = 0;         // visible height for thumbnails area
    Rect rcThumbsArea;               // clip rect for thumbnails

    // search filter
    StrVec filterWords;
    Vec<u8> highlighted;
    Rect rcSearchBorder; // border rect drawn around the edit control

    // tip layout
    Rect rcTip;     // background rect for tip area
    Rect rcTipText; // where the markup goes inside the band
    bool hasTip = false;

    ~HomePageLayout();
};

// freqRead / openDoc are borrowed from the persistent chrome tree (owned by
// win->homeRoot), not created per layout
HomePageLayout::~HomePageLayout() = default;

// --- home page chrome as a VirtCtrl tree ---
// The chrome (header, view-mode buttons, "Open a document..." link, logo row)
// lives for as long as the window, so hover / pressed state survives the
// repaints that scrolling and filtering cause. Geometry still comes from
// LayoutHomePage(): HomePageSyncChrome() just feeds it into the tree. The
// [palette] logo [help] row is an HBox of virt controls.

// Leaf home-page controls: no MainWindow*. Wire onClick / hwndForCmds when
// building the chrome so the same VirtCtrl types stay reusable.

struct HomeViewIconCtrl : VirtCtrl {
    Pixmap* pixmap = nullptr; // not owned, from GetCachedPixmapForSvg()
    // true for the "show as list" button, false for "show as thumbnails"
    bool listView = false;

    HomeViewIconCtrl();
    void Paint(VirtPaintCtx&) override;
};

struct HomeOpenDocCtrl : VirtCtrl {
    Pixmap* pixmap = nullptr; // not owned, from GetCachedPixmapForSvg()
    VirtText* text = nullptr; // child
    // icon position, relative to our bounds
    Rect rcIconLocal;

    HomeOpenDocCtrl();
    void Paint(VirtPaintCtx&) override;
};

// the brand along the top of the home page. An HBox sizes and places the virt
// controls; they are also our VirtCtrl children so paint / hit-test / delete
// stay on this tree.
struct HomeLogoRow : VirtCtrl {
    HBox* box = nullptr;

    HomeLogoRow();
    ~HomeLogoRow() override;

    void AddItem(VirtCtrl*);
    Size GetIdealSize() override;
    void SetBounds(Rect) override;
};

struct HomeEntryCtrl;

// the pin icon of a list-view row. It is drawn by DrawHomeListRow, so this is a
// hit target only (the row's ✕ is a VirtCloseButton, which draws itself)
struct HomeListIconCtrl : VirtCtrl {
    bool isPin = true;

    HomeListIconCtrl();
    void OnGetTooltip(VirtTooltipEvent*);
};

// one file entry (a thumbnail or a list row): hit-testing, hover, clicks and
// painting of the row / thumbnail content
struct HomeEntryCtrl : VirtCtrl {
    Str filePath; // owned
    int idx = 0;
    VirtCloseButton* closeBtn = nullptr;
    VirtCloseButton* removeBtn = nullptr;
    HomeListIconCtrl* pinBtn = nullptr;

    HomeEntryCtrl();
    ~HomeEntryCtrl() override;
    void Paint(VirtPaintCtx&) override;
};

// page-level list: still knows the MainWindow so it can wire entry actions and
// keep keyboard selection in sync
struct HomeEntriesCtrl : VirtCtrl {
    MainWindow* win = nullptr;
    // entry the mouse is on, -1 for none. Drives the ✕ button and the keyboard
    // selection, which follows the mouse
    int activeIdx = -1;
    Point lastHoverPt{-1, -1};
    const StrVec* filterWords = nullptr;
    Vec<u8>* highlighted = nullptr;

    HomeEntriesCtrl();
    void OnMouseMove(VirtMouseEvent*);

    HomeEntryCtrl* EntryAt(int idx);
    HomeEntryCtrl* EntryForCtrl(VirtCtrl*);
    void SetEntryCount(int n);
    void SetActiveEntry(int idx);
    void UpdateCloseBtnVisibility();
};

// the tip band at the bottom. The markup is its VirtRichText child, which draws
// itself and runs its own links; double-clicking the band anywhere else picks another
// tip
struct HomeTipCtrl : VirtCtrl {
    // for link commands inside the tip markup (like VirtRichText)
    HWND hwndForCmds = nullptr;
    // onDoubleClick (VirtCtrl): double-click outside a link picks another tip
    VirtRichText* rich = nullptr; // owned, as our only child
    Str richFor;                  // owned, the markup `rich` was parsed from

    ~HomeTipCtrl() override;
    void SetTipLine(Str line, PlatformFont* font);
    void Sync(const Rect& rcTip, const Rect& rcText);
    void Paint(VirtPaintCtx&) override;
};

// paints the border and background around the home search edit (the edit
// itself is a real HWND on top). Decoration only, never a click target
struct HomeSearchBorderCtrl : VirtCtrl {
    HomeSearchBorderCtrl();
    void Paint(VirtPaintCtx&) override;
};

static Kind kindHomeChromeCtrl = "homeChromeCtrl";

struct HomeChromeCtrl : VirtCtrl {
    HomeTipCtrl* tip = nullptr;
    HomeSearchBorderCtrl* searchBorder = nullptr;
    HomeEntriesCtrl* entries = nullptr;
    VirtText* hdr = nullptr;
    HomeLogoRow* logoRow = nullptr;
    SumatraLogo* logo = nullptr;
    HomeViewIconCtrl* thumbView = nullptr;
    HomeViewIconCtrl* listView = nullptr;
    HomeOpenDocCtrl* openDoc = nullptr;
    // chrome-less About dropdown under the logo; shown after the tooltip delay
    VirtHost* aboutHover = nullptr;

    ~HomeChromeCtrl() override;
};

static HomeChromeCtrl* EnsureHomeChrome(MainWindow* win);
static HomeChromeCtrl* HomeChrome(MainWindow* win);
static HomeEntriesCtrl* HomeEntries(MainWindow* win);
static void HideHomeAboutHover(MainWindow* win);
static void ShowHomeAboutHover(MainWindow* win);
static void HomePageSyncChrome(HomePageLayout& l);
static Rect HomeSelectionOutlineRect(const ThumbnailLayout& t);
static Rect HomeOutlinePaintClip(const Rect& thumbsArea, const Rect& searchBorder, const Rect& tip, bool hasTip);

static int HomePageIconSize() {
    int sz = DpiScale(gSettings->toolbarSize);
    if (sz < 1) {
        sz = DpiScale(16);
    }
    return RoundUp(sz, 4);
}

constexpr int kThumbsMiddleMargin = 32;
// draw a gray separator line between list-view rows
static bool gShowListSeparatorLine = false;
constexpr int kSearchEditDy = 28;
constexpr int kSearchThumbnailsGapY = 28;
constexpr int kHomeHeaderTopY = 24;
constexpr int kHomeSearchMaxDx = 340;

static PlatformFont* HomePageFont(int size) {
    return GetUserGuiFont(StrL("Segoe UI"), DpiScale(size));
}

static void HomeSelectFromSearchReturnCol(MainWindow* win);
static void HomePageShowSelectionTooltip(MainWindow* win);

struct HomeSearchEdit : Edit {
    MainWindow* win = nullptr;

    void WndProc(ControlBase::WndProcEvent* ev) {
        if (ev->msg == WM_KEYDOWN && ev->wparam == VK_DOWN) {
            // down from the search box moves into the file list (issue #1136),
            // restoring the column we left from when going up
            if (win) {
                HomeSelectFromSearchReturnCol(win);
                HwndSetFocus(win->hwndCanvas);
                HwndInvalidate(win->hwndCanvas);
                HomePageShowSelectionTooltip(win);
            }
            ev->result = 0;
            ev->didHandle = true;
            return;
        }
        if (ev->msg == WM_KEYDOWN && ev->wparam == VK_ESCAPE) {
            SetText(StrL(""));
            if (win) {
                HwndSetFocus(win->hwndCanvas);
                win->RedrawAll(true);
            }
            ev->result = 0;
            ev->didHandle = true;
            return;
        }
        if (ev->msg == WM_MOUSEWHEEL) {
            // the home page scrolls, not the one-line edit
            ev->result = SendMessageW(GetParent(ev->hwnd), ev->msg, ev->wparam, ev->lparam);
            ev->didHandle = true;
            return;
        }
        Edit::WndProc(ev);
    }
};

// Cue banner when the search field is empty: "Search N files (Ctrl + F)".
static void UpdateHomeSearchCueBanner(MainWindow* win) {
    if (!win || !win->homeSearch) {
        return;
    }
    // Tr returns Str; pass .s into type-safe fmt for the format string.
    TempStr cue = fmt(Tr("Search %d files (Ctrl + F)").s, CountHomePageFiles());
    EditSetCueText(win->homeSearch, cue);
}

static void HomeSearchTextChanged(MainWindow* win) {
    win->homePageScrollY = 0;
    // the filter changed the list, so select its first entry (#1136)
    HomePageSelectFirst(win);
    HomePageRelayout(win);
    HwndInvalidate(win->hwndCanvas);
}

// the keyboard selection outline is hidden while the search box has the focus
static void HomeSearchFocusChanged(MainWindow* win) {
    HwndInvalidate(win->hwndCanvas);
}

static void PlaceHomeSearchEdit(MainWindow* win, const Rect& rcSearchBorder) {
    if (!win || !win->homeSearchLayout || rcSearchBorder.IsEmpty()) {
        return;
    }
    int searchEditDy = DpiScale(kSearchEditDy);
    Rect rcEdit = {rcSearchBorder.x + 1, rcSearchBorder.y + 1, rcSearchBorder.dx - 2, searchEditDy};
    LayoutToSize(win->homeSearchLayout, rcEdit.Size());
    win->homeSearchLayout->SetBounds(rcEdit);
}

static void EnsureHomeSearchCreated(MainWindow* win) {
    if (win->homeSearch) {
        UpdateHomeSearchCueBanner(win);
        return;
    }
    HWND parent = win->hwndCanvas;
    PlatformFont* font = HomePageFont(14);

    Edit::CreateArgs args;
    args.parent = parent;
    args.font = font;
    // the home page draws the box around it, so the edit has no border of its own
    auto* e = new HomeSearchEdit();
    e->win = win;
    e->Create(args);
    // Edit::Create wired Edit::WndProc; re-route to HomeSearchEdit for Esc/Down/wheel
    e->onWndProc = MkMethod1<HomeSearchEdit, ControlBase::WndProcEvent*, &HomeSearchEdit::WndProc>(e);
    e->SetColors(ThemeWindowTextColor(), ThemeControlBackgroundColor());
    e->onTextChanged = MkFunc0(HomeSearchTextChanged, win);
    e->onFocus = MkFunc0(HomeSearchFocusChanged, win);
    e->onKillFocus = MkFunc0(HomeSearchFocusChanged, win);
    win->homeSearch = e;
    UpdateHomeSearchCueBanner(win);
    // add left/right padding so text doesn't overlap the border
    int margin = DpiScale(6);
    EditSetMargins(e, margin, margin);
    // restore the query from before the edit control was destroyed
    // (e.g. by switching to a document tab and back)
    if (len(win->homeSearchQuery) > 0) {
        e->SetText(win->homeSearchQuery);
    }
    // the box is kSearchEditDy tall but the edit is only as tall as its text,
    // so a one-child HBox centers it in there instead of us doing that by hand
    auto* box = new HBox();
    box->alignCross = CrossAxisAlign::CrossCenter;
    box->AddChild(e, 1);
    win->homeSearchLayout = box;
    e->SetIsVisible(false);
}

void HomePageHideSearch(MainWindow* win) {
    if (win && win->homeSearch) {
        win->homeSearch->SetIsVisible(false);
    }
}

void HomePageDestroySearch(MainWindow* win) {
    if (!win || !win->homeSearch) {
        return;
    }
    TempStr query = win->homeSearch->GetTextTemp();
    str::ReplaceWithCopy(&win->homeSearchQuery, query);
    // destroying the edit's window pumps messages, and the canvas answers most
    // of them by calling us again (see WndProcCanvas), so drop our pointers
    // before deleting - otherwise the re-entered call deletes the tree twice
    ILayout* layout = win->homeSearchLayout;
    win->homeSearchLayout = nullptr;
    win->homeSearch = nullptr;
    // the layout owns the edit
    delete layout;
}

// after a theme change; the edit paints itself from these (see
// Edit::OnMessageReflect and the reflection in WndProcCanvas)
void HomePageUpdateSearchColors(MainWindow* win) {
    if (win->homeSearch) {
        win->homeSearch->SetColors(ThemeWindowTextColor(), ThemeControlBackgroundColor());
    }
}

// The search edit survives while the frame moves between monitors. Its HFONT
// and text margins do not follow WM_DPICHANGED automatically, and the page's
// cached rectangles were measured at the previous DPI.
void HomePageOnDpiChanged(MainWindow* win, int dpi) {
    HideHomeAboutHover(win);
    ClearHomeLayoutCache(win);
    if (!win || dpi <= 0) {
        return;
    }
    if (win->homeSearch) {
        int fontSize = DpiScaleByDpi(dpi, 14);
        win->homeSearch->SetFont(GetUserGuiFont(StrL("MS Shell Dlg"), fontSize));
        int margin = DpiScaleByDpi(dpi, 6);
        EditSetMargins(win->homeSearch, margin, margin);
    }
    HomePageRelayout(win);
    if (win->hwndCanvas) {
        HwndInvalidate(win->hwndCanvas, true);
    }
}

void PickAnotherRandomPromotion() {
    PickAnotherRandomTip();
}

// --- scroll-friendly layout cache: full LayoutHomePage only when content/size/
// filter changes; pure scrollY changes just offset stored thumb rects ---
struct HomePageLayoutCache {
    bool valid = false;
    int dpi = 0;
    Rect canvasRc;
    int scrollY = 0;
    int nFiles = 0;
    bool listView = false;
    bool sortByFreq = false;
    bool showTips = false;
    bool isRtl = false;
    int tipIdx = -1;
    bool tipIsPromo = false;
    Str filterText; // owned

    Rect rcThumbsArea;
    Rect rcSearchBorder;
    Rect rcIconOpen;
    Rect rcIconListView;
    Rect rcIconThumbnailView;
    Rect rcLogo;
    Rect rcTip;
    Rect rcFreqRead;
    Rect rcOpenDoc;
    int totalContentDy = 0;
    int thumbsVisibleDy = 0;
    Rect rcTipText;
    bool hasTip = false;
    Vec<ThumbnailLayout> thumbs;
    StrVec filterWords;
    Vec<u8> highlighted;
};

// The layout belongs to the window: its home chrome entries are laid out and
// painted from these rects, so a second window must not overwrite them.
static HomePageLayoutCache& HomeLayout(MainWindow* win) {
    if (!win->homeLayout) {
        win->homeLayout = new HomePageLayoutCache();
    }
    return *win->homeLayout;
}

static void ClearHomeLayoutCache(MainWindow* win) {
    if (!win || !win->homeLayout) {
        return;
    }
    auto& c = *win->homeLayout;
    c.valid = false;
    str::Free(c.filterText);
    c.filterText = {};
    VecReset(c.thumbs);
    c.filterWords.Reset();
    VecReset(c.highlighted);
    c.hasTip = false;
    c.nFiles = 0;
    c.scrollY = 0;
}

// The cache holds raw FileState* (ThumbnailLayout::fs) owned by gSettings.
// Reloading settings frees and rebuilds those, so the cache has to be dropped
// first or hover / selection reads freed memory (crash 8c34d7eda). It is
// rebuilt on the next HomePageRelayout.
// must be called before the FileState objects the cache points at are freed
void HomePageInvalidateLayoutCache() {
    for (MainWindow* win : gWindows) {
        ClearHomeLayoutCache(win);
    }
}

void HomePageFocusSearch(MainWindow* win) {
    EnsureHomeSearchCreated(win);
    HomePageRelayout(win);
    if (win->homeSearch) {
        win->homeSearch->SetIsVisible(true);
        EditSetFocus(win->homeSearch);
    }
}

static void OffsetThumbnailLayouts(Vec<ThumbnailLayout>& thumbs, int dy) {
    if (dy == 0) {
        return;
    }
    for (ThumbnailLayout& t : thumbs) {
        t.rcCell.y += dy;
        t.rcPage.y += dy;
        t.rcText.y += dy;
        t.rcListRow.y += dy;
        t.rcListThumb.y += dy;
        t.rcListFileName.y += dy;
        t.rcListPath.y += dy;
        t.rcListSize.y += dy;
        t.rcListRemove.y += dy;
        t.rcListPin.y += dy;
    }
}

static TempStr HomeSearchQueryTemp(MainWindow* win) {
    if (win->homeSearch) {
        return win->homeSearch->GetTextTemp();
    }
    // edit is created after paint; keep filtering with the remembered query
    return win->homeSearchQuery;
}

static bool HomeLayoutCacheMatches(MainWindow* win, const Rect& rc, Str filterText) {
    auto& c = HomeLayout(win);
    if (!c.valid) {
        return false;
    }
    if (c.dpi != DpiGet()) {
        return false;
    }
    if (c.canvasRc != rc) {
        return false;
    }
    if (c.listView != HomePageIsListView()) {
        return false;
    }
    if (c.sortByFreq != (gSettings && gSettings->homePageSortByFrequentlyRead)) {
        return false;
    }
    if (c.showTips != (gSettings && gSettings->showTips)) {
        return false;
    }
    if (c.isRtl != IsUIRtl()) {
        return false;
    }
    if (c.tipIdx != gSelectedTipIdx || c.tipIsPromo != gSelectedIsPromo) {
        return false;
    }
    if (!str::Eq(c.filterText, filterText)) {
        return false;
    }
    // pin/remove/reorder changes FileState pointers or order → invalidate
    // (nFiles alone is not enough: pin does not change count)
    return true;
}

// true if cached thumb FileState* sequence still matches the current file list
static bool HomeLayoutCacheFilesMatch(MainWindow* win, const Vec<FileState*>& files) {
    auto& c = HomeLayout(win);
    if (len(files) != c.nFiles || len(c.thumbs) != c.nFiles) {
        return false;
    }
    for (int i = 0; i < c.nFiles; i++) {
        if (c.thumbs[i].fs != files[i]) {
            return false;
        }
    }
    return true;
}

static void CollectHomePageFiles(MainWindow* win, Vec<FileState*>& fileStates, StrVec& filterWords) {
    Vec<FileState*> allFileStates;
    if (gSettings->homePageSortByFrequentlyRead) {
        FileHistoryGetFrequencyOrder(allFileStates);
    } else {
        FileHistoryGetRecentlyOpenedOrder(allFileStates);
    }

    TempStr searchQuery = HomeSearchQueryTemp(win);
    bool hasFilter = searchQuery && searchQuery.s[0];
    if (hasFilter) {
        SplitFilterToWords(searchQuery, filterWords);
    }
    for (int i = 0; i < len(allFileStates); i++) {
        FileState* fs = allFileStates[i];
        if (len(fs->filePath) == 0) {
            continue;
        }
        if (hasFilter) {
            TempStr baseName = path::GetBaseNameTemp(fs->filePath);
            if (!FilterMatches(baseName, filterWords)) {
                continue;
            }
        }
        VecAppend(fileStates, fs);
    }
}

static void SaveHomeLayoutCache(const HomePageLayout& l, Str filterText, int scrollY) {
    auto& c = HomeLayout(l.win);
    c.valid = true;
    c.dpi = DpiGet();
    c.canvasRc = l.rc;
    c.scrollY = scrollY;
    c.nFiles = len(l.thumbnails);
    c.listView = HomePageIsListView();
    c.sortByFreq = gSettings && gSettings->homePageSortByFrequentlyRead;
    c.showTips = gSettings && gSettings->showTips;
    c.isRtl = IsUIRtl();
    c.tipIdx = gSelectedTipIdx;
    c.tipIsPromo = gSelectedIsPromo;
    str::ReplaceWithCopy(&c.filterText, filterText);
    c.rcThumbsArea = l.rcThumbsArea;
    c.rcSearchBorder = l.rcSearchBorder;
    c.rcIconOpen = l.rcIconOpen;
    c.rcIconListView = l.rcIconListView;
    c.rcIconThumbnailView = l.rcIconThumbnailView;
    c.rcLogo = l.rcLogo;
    c.rcTip = l.rcTip;
    c.rcFreqRead = l.freqRead ? l.freqRead->lastBounds : Rect{};
    c.rcOpenDoc = l.openDoc ? l.openDoc->lastBounds : Rect{};
    c.totalContentDy = l.totalContentDy;
    c.thumbsVisibleDy = l.thumbsVisibleDy;
    c.rcTipText = l.rcTipText;
    c.hasTip = l.hasTip;
    c.thumbs = l.thumbnails;
    c.filterWords = l.filterWords;
    c.highlighted = l.highlighted;
}

// rebuild chrome VirtText + copy cached geometry into l (no full layout)
static void ApplyHomeLayoutCache(HomePageLayout& l, int scrollY) {
    auto* win = l.win;
    auto& c = HomeLayout(win);
    bool isRtl = IsUIRtl();

    // clamp scroll using cached content height
    int maxScrollY = std::max(0, c.totalContentDy - c.thumbsVisibleDy);
    if (scrollY > maxScrollY) {
        scrollY = maxScrollY;
        win->homePageScrollY = scrollY;
    }
    if (scrollY < 0) {
        scrollY = 0;
        win->homePageScrollY = 0;
    }

    int dy = c.scrollY - scrollY; // content moves opposite scroll direction
    OffsetThumbnailLayouts(c.thumbs, dy);
    c.scrollY = scrollY;

    l.rcThumbsArea = c.rcThumbsArea;
    l.rcSearchBorder = c.rcSearchBorder;
    l.rcIconOpen = c.rcIconOpen;
    l.rcIconListView = c.rcIconListView;
    l.rcIconThumbnailView = c.rcIconThumbnailView;
    l.rcLogo = c.rcLogo;
    l.rcTip = c.rcTip;
    l.totalContentDy = c.totalContentDy;
    l.thumbsVisibleDy = c.thumbsVisibleDy;
    l.rcTipText = c.rcTipText;
    l.hasTip = c.hasTip;
    l.thumbnails = c.thumbs;
    l.filterWords = c.filterWords;
    l.highlighted = c.highlighted;

    PlatformFont* hdrFont = HomePageFont(24);
    PlatformFont* fontText = HomePageFont(14);

    Str txt = Tr("Recently Opened");
    if (gSettings->homePageSortByFrequentlyRead) {
        txt = Tr("Frequently Read");
    }
    HomeChromeCtrl* chrome = EnsureHomeChrome(win);
    VirtText* hdr = chrome->hdr;
    hdr->SetText(txt);
    hdr->font = hdrFont;
    hdr->isRtl = isRtl;
    hdr->SetBounds(c.rcFreqRead);
    l.freqRead = hdr;

    VirtText* openDoc = chrome->openDoc->text;
    openDoc->SetText(Tr("Open..."));
    openDoc->font = fontText;
    openDoc->isRtl = isRtl;
    openDoc->withUnderline = false;
    openDoc->SetBounds(c.rcOpenDoc);
    l.openDoc = openDoc;
}

static void LayoutHomePage(HomePageLayout& l) {
    EnsureTipsParsed();

    Vec<FileState*> allFileStates;
    if (gSettings->homePageSortByFrequentlyRead) {
        FileHistoryGetFrequencyOrder(allFileStates);
    } else {
        FileHistoryGetRecentlyOpenedOrder(allFileStates);
    }
    auto rc = l.rc;
    auto* win = l.win;

    TempStr searchQuery = HomeSearchQueryTemp(win);
    bool hasFilter = searchQuery && searchQuery.s[0];
    if (hasFilter) {
        SplitFilterToWords(searchQuery, l.filterWords);
    }
    Vec<FileState*> fileStates;
    for (int i = 0; i < len(allFileStates); i++) {
        FileState* fs = allFileStates[i];
        // a state without a path can't be opened or thumbnailed - don't show it
        if (len(fs->filePath) == 0) {
            continue;
        }
        if (hasFilter) {
            TempStr baseName = path::GetBaseNameTemp(fs->filePath);
            if (!FilterMatches(baseName, l.filterWords)) {
                continue;
            }
        }
        VecAppend(fileStates, fs);
    }

    bool isRtl = IsUIRtl();
    PlatformFont* fontText = HomePageFont(13);

    // --- Pre-compute thumbnail grid x offset so header can align with it ---
    // use unfiltered count so layout stays stable when search filters results
    int nFilesForLayout = len(allFileStates);
    Size frame = ThumbnailFrameSize();
    int colsForLayout =
        (rc.dx - kThumbsMarginLeft - kThumbsMarginRight + kThumbsSpaceBetweenX) / (frame.dx + kThumbsSpaceBetweenX);
    int thumbsColsForLayout = std::max(colsForLayout, 1);
    int thumbsStartX = rc.x + kThumbsMarginLeft +
                       ((rc.dx - (thumbsColsForLayout * frame.dx) - ((thumbsColsForLayout - 1) * kThumbsSpaceBetweenX) -
                         kThumbsMarginLeft - kThumbsMarginRight) /
                        2);
    if (thumbsStartX < DpiScale(kInnerPadding)) {
        thumbsStartX = DpiScale(kInnerPadding);
    } else if (nFilesForLayout == 0) {
        thumbsStartX = kThumbsMarginLeft;
    }
    int thumbsContentWidth = (thumbsColsForLayout * frame.dx) + ((thumbsColsForLayout - 1) * kThumbsSpaceBetweenX);

    // --- Step 1: one header row. The brand ends on the first column's right
    // edge, the Open link and view icons end on the last column's, and the
    // search field sits on the center line between them ---
    Rect rcIconView(0, 0, 0, 0);
    rcIconView.dx = rcIconView.dy = HomePageIconSize();

    HomeChromeCtrl* chrome = EnsureHomeChrome(win);
    // the section title is no longer shown; empty bounds keep it unpainted
    VirtText* hdr = chrome->hdr;
    hdr->SetBounds({});
    l.freqRead = hdr;

    int searchEditDy = DpiScale(kSearchEditDy);
    int searchThumbsGap = DpiScale(kSearchThumbnailsGapY);
    int borderDy = searchEditDy + 2; // 1px border on each side

    chrome->logo->font = HomeBrandFont();
    Size logoRowSize = chrome->logoRow->GetIdealSize();

    int hdrY = DpiScale(kHomeHeaderTopY);
    int iconGap = DpiScale(4);
    int rowDy = std::max({rcIconView.dy, borderDy, logoRowSize.dy});
    // every row item (brand, link, search box, view icons) is centered on the
    // row's vertical centerline
    int centerY = hdrY + (rowDy / 2);
    int viewIconsDx = (2 * rcIconView.dx) + iconGap;

    int logoX = thumbsStartX + frame.dx - logoRowSize.dx;
    if (logoX < thumbsStartX) {
        logoX = thumbsStartX;
    }
    l.rcLogo = {logoX, centerY - (logoRowSize.dy / 2), logoRowSize.dx, logoRowSize.dy};

    /* view-mode icons at the right edge */
    l.rcIconThumbnailView = {thumbsStartX + thumbsContentWidth - viewIconsDx, centerY - (rcIconView.dy / 2),
                             rcIconView.dx, rcIconView.dy};
    l.rcIconListView = {l.rcIconThumbnailView.x + rcIconView.dx + iconGap, l.rcIconThumbnailView.y, rcIconView.dx,
                        rcIconView.dy};

    /* "Open..." link before the icons */
    Rect rcIconOpen(0, 0, 0, 0);
    rcIconOpen.dx = rcIconOpen.dy = HomePageIconSize();

    VirtText* openDoc = chrome->openDoc->text;
    openDoc->SetText(Tr("Open..."));
    openDoc->font = fontText;
    openDoc->isRtl = isRtl;
    openDoc->withUnderline = false;
    Size txtSize = openDoc->GetIdealSize(true);
    int openTextGap = DpiScale(6);
    int openGroupDx = rcIconOpen.dx + openTextGap + txtSize.dx;
    int rowGapX = DpiScale(16);

    rcIconOpen.x = l.rcIconThumbnailView.x - rowGapX - openGroupDx;
    rcIconOpen.y = centerY - (rcIconOpen.dy / 2);
    Rect rcOpenDoc(rcIconOpen.x + rcIconOpen.dx + openTextGap, centerY - (txtSize.dy / 2), txtSize.dx, txtSize.dy);

    // the search box is centered; the brand and the Open group bound its width
    int leftDx = std::max(logoRowSize.dx, frame.dx) + rowGapX;
    int rightDx = thumbsStartX + thumbsContentWidth - rcIconOpen.x + rowGapX;
    int flankDx = std::max(leftDx, rightDx);
    int borderDx = std::min(thumbsContentWidth - (2 * flankDx), DpiScale(kHomeSearchMaxDx));
    borderDx = std::max(borderDx, DpiScale(200));
    int borderX = thumbsStartX + ((thumbsContentWidth - borderDx) / 2);
    l.rcSearchBorder = {borderX, centerY - (borderDy / 2), borderDx, borderDy};

    if (isRtl) {
        auto mirrorX = [&rc](Rect& r) { r.x = rc.dx - r.x - r.dx; };
        mirrorX(l.rcLogo);
        mirrorX(l.rcIconThumbnailView);
        mirrorX(l.rcIconListView);
        mirrorX(rcIconOpen);
        mirrorX(rcOpenDoc);
        mirrorX(l.rcSearchBorder);
    }
    l.rcIconOpen = rcIconOpen;
    openDoc->SetBounds(rcOpenDoc);
    l.openDoc = openDoc;

    int headerBottomY = hdrY + rowDy + searchThumbsGap;

    // --- Step 2: calculate tip area at the bottom (before thumbnails) ---
    int tipHeight = 0;
    PlatformFont* fontTip = HomePageFont(12);
    HomeTipCtrl* tipCtrl = EnsureHomeChrome(l.win)->tip;
    tipCtrl->SetTipLine(SelectedTipLine(), fontTip);
    VirtRichText* tip = tipCtrl->rich;
    if (tip) {
        int tipPadding = DpiScale(8);
        tipHeight = tip->MinIntrinsicHeight(thumbsContentWidth) + (2 * tipPadding);
    }

    // --- Step 3: middle area for thumbnails/list ---
    // content starts directly after headerBottomY (which includes kSearchThumbnailsGapY)
    int thumbsTopY = headerBottomY;
    int thumbsBottomY = rc.dy - tipHeight - kThumbsMiddleMargin;
    int thumbsVisibleDy = std::max(0, thumbsBottomY - thumbsTopY);

    l.rcThumbsArea = {0, thumbsTopY, rc.dx, thumbsVisibleDy};

    int nFiles = len(fileStates);
    bool showList = HomePageIsListView();
    // Leave room above the first row so RoundRect / selection outline top edges
    // aren't clipped by rcThumbsArea (they extend a few px upward).
    int thumbsContentPadTop = showList ? DpiScale(2) : DpiScale(5);
    int thumbsRows = 0;
    int thumbsContentDy = 0;
    if (showList) {
        thumbsRows = nFiles;
        thumbsContentDy = nFiles * kHomeListRowDy;
    } else {
        thumbsRows = (nFiles + thumbsColsForLayout - 1) / thumbsColsForLayout;
        if (thumbsRows > 0) {
            // the last row's caption hangs below its thumbnails (issue #6234)
            thumbsContentDy = (thumbsRows * (frame.dy + kThumbsSpaceBetweenY)) - kThumbsSpaceBetweenY +
                              kThumbsCaptionGapY + kThumbsCaptionDy;
        }
    }
    if (thumbsContentDy > 0) {
        thumbsContentDy += thumbsContentPadTop;
    }

    int scrollY = win->homePageScrollY;
    int maxScrollY = std::max(0, thumbsContentDy - thumbsVisibleDy);
    if (scrollY > maxScrollY) {
        scrollY = maxScrollY;
        win->homePageScrollY = scrollY;
    }
    l.totalContentDy = thumbsContentDy;
    l.thumbsVisibleDy = thumbsVisibleDy;

    Point ptOff(thumbsStartX, thumbsTopY + thumbsContentPadTop - scrollY);

    if (showList) {
        int listX = thumbsStartX;
        if (isRtl) {
            listX = rc.dx - thumbsStartX - thumbsContentWidth;
        }
        int listIconDx = l.rcIconListView.dx;
        int listIconGap = DpiScale(6);
        // fixed size column — never call file::GetSize during layout (disk/network I/O)
        int listSizeDx = DpiScale(56);
        bool showProgress = gSettings && gSettings->showHomePageReadingProgress;
        int listProgressDx = showProgress ? DpiScale(56) : 0;
        int listProgressGap = listProgressDx > 0 ? listIconGap : 0;
        // one-row margin so a quick scroll still has measured name/path splits ready
        int listPrefetchY = kHomeListRowDy;
        for (int row = 0; row < nFiles; row++) {
            ThumbnailLayout& thumb = *VecAppendBlanks(l.thumbnails, 1);
            thumb.fileSize = kSizeNotFetched;
            FileState* fs = fileStates[row];
            thumb.fs = fs;
            Rect rcRow(listX, ptOff.y + (row * kHomeListRowDy), thumbsContentWidth, kHomeListRowDy);
            thumb.rcListRow = rcRow;
            bool onScreen = IsHomeThumbOnScreen(rcRow, l.rcThumbsArea, listPrefetchY);

            Rect rcThumb(rcRow.x, rcRow.y + ((rcRow.dy - kHomeListThumbDy) / 2), kHomeListThumbDx, kHomeListThumbDy);
            Rect rcPin(rcRow.x + rcRow.dx - listIconDx, rcRow.y + ((rcRow.dy - listIconDx) / 2), listIconDx,
                       listIconDx);
            Rect rcRemove(rcPin.x - listIconGap - listIconDx, rcPin.y, listIconDx, listIconDx);
            Rect rcSize(rcRemove.x - listIconGap - listSizeDx, rcRow.y, listSizeDx, rcRow.dy);
            Rect rcProgress(rcSize.x - listProgressGap - listProgressDx, rcRow.y, listProgressDx, rcRow.dy);
            Rect rcFileName(rcThumb.x + rcThumb.dx + kHomeListRowGapDx, rcRow.y,
                            rcProgress.x - (rcThumb.x + rcThumb.dx + kHomeListRowGapDx) - kHomeListRowGapDx, rcRow.dy);
            if (isRtl) {
                rcThumb.x = rcRow.x + rcRow.dx - rcThumb.dx;
                rcPin.x = rcRow.x;
                rcRemove.x = rcPin.x + listIconDx + listIconGap;
                rcSize.x = rcRemove.x + listIconDx + listIconGap;
                rcProgress.x = rcSize.x + rcSize.dx + listProgressGap;
                rcFileName.x = rcProgress.x + rcProgress.dx + kHomeListRowGapDx;
                rcFileName.dx = rcThumb.x - rcFileName.x - kHomeListRowGapDx;
            }
            rcFileName.dx = std::max(rcFileName.dx, 0);
            // rcFileName is the whole name+path span; MeasureHomeListRowText()
            // splits it when the row is first painted. Doing it here would
            // measure text for every history entry on every layout, and doing it
            // only for rows that happen to be on screen *now* left rows scrolled
            // in later without their directory (#5870 follow-up).
            thumb.rcListThumb = rcThumb;
            thumb.rcListPin = rcPin;
            thumb.rcListRemove = rcRemove;
            thumb.rcListSize = rcSize;
            thumb.rcListProgress = rcProgress;
            thumb.rcListFileName = rcFileName;
            // already-cached in-memory thumb size only (no LoadThumbnail / disk)
            if (onScreen && fs->thumbnail) {
                thumb.szThumb = Size(fs->thumbnail->width, fs->thumbnail->height);
            }
        }
    } else {
        int thumbPrefetchY = frame.dy + kThumbsSpaceBetweenY;
        for (int row = 0; row < thumbsRows; row++) {
            for (int col = 0; col < thumbsColsForLayout; col++) {
                if ((row * thumbsColsForLayout) + col >= nFiles) {
                    // no more files to display
                    thumbsRows = col > 0 ? row + 1 : row;
                    break;
                }
                ThumbnailLayout& thumb = *VecAppendBlanks(l.thumbnails, 1);
                thumb.fileSize = kSizeNotFetched;
                FileState* fs = fileStates[(row * thumbsColsForLayout) + col];
                thumb.fs = fs;

                Rect rcCell(ptOff.x + (col * (frame.dx + kThumbsSpaceBetweenX)),
                            ptOff.y + (row * (frame.dy + kThumbsSpaceBetweenY)), frame.dx, frame.dy);
                if (isRtl) {
                    rcCell.x = rc.dx - rcCell.x - rcCell.dx;
                }
                bool onScreen = IsHomeThumbOnScreen(rcCell, l.rcThumbsArea, thumbPrefetchY);
                Rect rcPage = rcCell;
                // only use already-resident thumbnails for aspect adjust — never LoadThumbnail
                // during layout (disk I/O dominated scroll/paint CPU)
                if (onScreen && fs->thumbnail) {
                    Size szThumb(fs->thumbnail->width, fs->thumbnail->height);
                    // a thumbnail smaller than the frame (a whole page, a cover)
                    // sits at the bottom of its cell, centered
                    if (szThumb != frame) {
                        Size drawn = ThumbnailDrawSize(szThumb, frame);
                        rcPage = Rect(rcCell.x + ((rcCell.dx - drawn.dx) / 2), rcCell.y + rcCell.dy - drawn.dy,
                                      drawn.dx, drawn.dy);
                    }
                    thumb.szThumb = szThumb;
                }
                thumb.rcCell = rcCell;
                thumb.rcPage = rcPage;
                thumb.rcText = Rect(rcCell.x, rcCell.y + rcCell.dy + kThumbsCaptionGapY, rcCell.dx, kThumbsCaptionDy);
            }
        }
    }

    // layout tip at the bottom
    if (tip) {
        Rect rcClient = HwndClientRect(win->hwndCanvas);
        int tipPadding = DpiScale(8);

        int tipY = rcClient.dy - tipHeight;
        // background spans full window width
        l.rcTip = {0, tipY, rcClient.dx, tipHeight};
        l.hasTip = true;

        // text area aligned with thumbnails
        int tipStartX = thumbsStartX;
        int tipStartY = tipY + tipPadding;
        l.rcTipText = {tipStartX, tipStartY, thumbsContentWidth, tip->MinIntrinsicHeight(thumbsContentWidth)};
    }
}

static void DrawHomeViewButton(Gfx* gfx, Pixmap* icon, Rect r, bool selected, bool hovered) {
    if (selected || hovered) {
        Color bg = AccentColor(ThemeMainWindowBackgroundColor(), selected ? 28 : 14);
        gfx->FillRoundedRect(r, DpiScale(4), bg);
    }
    if (icon) {
        gfx->DrawPixmap(icon, {r.x, r.y, icon->width, icon->height});
    }
}

static Rect FitRectInRect(Size src, Rect dst) {
    if (src.dx <= 0 || src.dy <= 0 || dst.dx <= 0 || dst.dy <= 0) {
        return dst;
    }
    int dx = dst.dx;
    int dy = src.dy * dx / src.dx;
    if (dy > dst.dy) {
        dy = dst.dy;
        dx = src.dx * dy / src.dy;
    }
    Rect r(dst.x + ((dst.dx - dx) / 2), dst.y + ((dst.dy - dy) / 2), dx, dy);
    return r;
}

static TempStr FileSizeForHomeListTemp(i64 size) {
    if (size < 0) {
        return str::DupTemp(StrL(""));
    }
    return str::FormatSizeShortTemp(size, nullptr);
}

// light blue outline marking the keyboard-selected list row (issue #1136).
// A fixed color: it has to read as "selected" against both the light and the
// dark page background. A thumbnail gets a ring in the text color instead
constexpr Color kHomeSelectionColor = MkRgb(0x4c, 0xa6, 0xff);

static void DrawHomeRoundedOutline(Gfx* gfx, const Rect& r, int radius, Color color, int thickness) {
    Rect line = r;
    for (int i = 0; i < thickness && !line.IsEmpty(); i++) {
        gfx->FillRoundedRect(line, std::max(radius - (2 * i), 1), kColorTransparent, color);
        line.Inflate(-1, -1);
    }
}

static void DrawHomeSelectionOutline(Gfx* gfx, const Rect& r, int radius) {
    int penDx = DpiScale(2);
    Color col = HomePageIsListView() ? kHomeSelectionColor : ThemeWindowTextColor();
    DrawHomeRoundedOutline(gfx, r, radius, col, penDx);
}

// Give the file name the width it needs and put the directory path in what's
// left, right-aligned (mirrored for RTL). Done on first paint of a row, not
// during layout: measuring every history entry made layout (and so scrolling)
// slow, and measuring only the rows visible at layout time meant rows scrolled
// into view later never got a directory.
static void MeasureHomeListRowText(Gfx* gfx, ThumbnailLayout& thumb, PlatformFont* font, bool isRtl) {
    if (thumb.listTextMeasured) {
        return;
    }
    thumb.listTextMeasured = true;

    Rect rcFileName = thumb.rcListFileName;
    TempStr fileName = path::GetBaseNameTemp(thumb.fs->filePath);
    int nameDx = gfx->MeasureText(fileName, font).dx + DpiScale(4);
    int minPathDx = DpiScale(80);
    if (nameDx + kHomeListRowGapDx + minPathDx > rcFileName.dx) {
        // no room for a path, the name gets the whole span
        return;
    }
    int pathDx = rcFileName.dx - nameDx - kHomeListRowGapDx;
    if (isRtl) {
        thumb.rcListPath = Rect(rcFileName.x, rcFileName.y, pathDx, rcFileName.dy);
        rcFileName.x = rcFileName.x + rcFileName.dx - nameDx;
    } else {
        thumb.rcListPath = Rect(rcFileName.x + nameDx + kHomeListRowGapDx, rcFileName.y, pathDx, rcFileName.dy);
    }
    rcFileName.dx = nameDx;
    thumb.rcListFileName = rcFileName;
}

// True when keyboard focus is in the home search box (hide list selection then).
static bool HomeSearchHasFocus(MainWindow* win) {
    return win && win->homeSearch && GetFocus() == win->homeSearch->hwnd;
}

static void DrawHomeListRow(Gfx* gfx, ThumbnailLayout& thumb, const StrVec& filterWords, Vec<u8>& highlighted,
                            PlatformFont* fontText, Color backgroundColor, bool isRtl, bool isSelected) {
    FileState* fs = thumb.fs;
    Rect row = thumb.rcListRow;
    MeasureHomeListRowText(gfx, thumb, fontText, isRtl);
    if (isSelected) {
        DrawHomeSelectionOutline(gfx, HomeSelectionOutlineRect(thumb), 4);
    }

    if (gShowListSeparatorLine) {
        Color lineCol = AccentColor(ThemeMainWindowBackgroundColor(), 30);
        gfx->DrawLine(Rect(row.x, row.y + row.dy - 1, row.dx, 0), lineCol);
    }

    // LoadThumbnail only hits disk the first time; result stays on fs->thumbnail
    Pixmap* thumbImg = LoadThumbnail(fs);
    Rect thumbBox = thumb.rcListThumb;
    if (thumbImg) {
        Size szThumb(thumbImg->width, thumbImg->height);
        Rect thumbDst = FitRectInRect(szThumb, thumbBox);
        gfx->DrawPixmap(thumbImg, thumbDst);
        thumb.szThumb = szThumb;
    }
    Str path = fs->filePath;
    TempStr fileName = path::GetBaseNameTemp(path);
    u32 nameFmt = gfxTextEllipsis | gfxTextVCenter | (isRtl ? gfxTextRight : gfxTextLeft);
    DrawMaybeHighlightedText(gfx, thumb.rcListFileName, fileName, filterWords, highlighted, backgroundColor, isRtl,
                             false, nameFmt, fontText, ThemeWindowTextColor());

    // directory path, right-aligned and muted, in the space the file name doesn't need.
    // Must use DrawTextW: dirPath is UTF-8; DrawTextA treated it as the system ANSI
    // code page and mangled non-ASCII path characters (#5824).
    if (!thumb.rcListPath.IsEmpty()) {
        TempStr dirPath = path::GetDirTemp(path);
        u32 pathFmt = gfxTextVCenter | gfxTextPathEllipsis | (isRtl ? gfxTextLeft : gfxTextRight);
        Rect pathRect = thumb.rcListPath;
        gfx->DrawText(dirPath, pathRect, pathFmt, fontText, ThemeWindowTextDisabledColor());
    }

    // file::GetSize once per row, then cache on ThumbnailLayout (scroll reuses
    // it). kSizeNotFetched = not tried; kSizeFetchFail = GetSize failed; >= 0
    // is a real size (including empty files).
    if (thumb.fileSize == kSizeNotFetched) {
        i64 sz = file::GetSize(path);
        thumb.fileSize = (sz < 0) ? kSizeFetchFail : sz;
    }
    TempStr fileSize = FileSizeForHomeListTemp(thumb.fileSize);
    u32 sizeFmt = gfxTextVCenter | gfxTextEllipsis | (isRtl ? gfxTextLeft : gfxTextRight);
    Rect sizeRect = thumb.rcListSize;
    gfx->DrawText(fileSize, sizeRect, sizeFmt, fontText, ThemeWindowTextColor());

    if (!thumb.rcListProgress.IsEmpty()) {
        TempStr progress = FormatFileStateProgressTemp(fs);
        if (len(progress) > 0) {
            u32 progFmt = gfxTextVCenter | gfxTextEllipsis | (isRtl ? gfxTextLeft : gfxTextRight);
            gfx->DrawText(progress, thumb.rcListProgress, progFmt, fontText, ThemeWindowTextColor());
        }
    }

    if (fs->isPinned) {
        gfx->FillRect(thumb.rcListPin, ThemeControlBackgroundColor());
    }
    {
        int pinDx = thumb.rcListPin.dx > 0 ? thumb.rcListPin.dx : DpiScale(16);
        int pinDy = thumb.rcListPin.dy > 0 ? thumb.rcListPin.dy : pinDx;
        Pixmap* pin = GetCachedPixmapForSvg(Str(gIconPin), pinDx, pinDy);
        if (pin) {
            gfx->DrawPixmap(pin, thumb.rcListPin);
        }
    }
}

// a soft shadow under the page: three translucent layers, each a little
// larger and lower than the last
static void DrawHomeThumbnailShadow(Gfx* gfx, const Rect& page) {
    bool light = IsLightColor(ThemeMainWindowBackgroundColor());
    u8 alphas[] = {light ? (u8)10 : (u8)40, light ? (u8)14 : (u8)50, light ? (u8)26 : (u8)80};
    for (int i = 0; i < 3; i++) {
        int grow = DpiScale(3 - i);
        Rect r(page.x - grow, page.y + DpiScale(i + 1), page.dx + (2 * grow), page.dy + grow);
        gfx->FillRects(&r, 1, kColBlack, alphas[i]);
    }
}

// one thumbnail: the page image with a hairline and a shadow, the caption
// (with search-filter highlights) and the page count under it
static void DrawHomeThumbnail(Gfx* gfx, ThumbnailLayout& thumb, const StrVec& filterWords, Vec<u8>& highlighted,
                              PlatformFont* fontText, Color backgroundColor, bool isRtl) {
    FileState* fs = thumb.fs;
    const Rect& page = thumb.rcPage;
    DrawHomeThumbnailShadow(gfx, page);
    // disk load only first time; stays on fs->thumbnail afterwards
    Pixmap* thumbImg = LoadThumbnail(fs);
    if (thumbImg) {
        Size size(thumbImg->width, thumbImg->height);
        if (size != thumb.szThumb) {
            HomePageThumbnailChanged(fs);
            thumb.szThumb = size;
        }
        gfx->PushClip(page);
        // note: we used to invert bitmaps in dark theme but that doesn't
        // make sense for thumbnails
        gfx->DrawPixmap(thumbImg, page);
        gfx->PopClip();
    } else {
        gfx->FillRect(page, ThemeControlBackgroundColor());
    }
    gfx->DrawRect(page, ThemeEdgeColor());

    float progress = 0;
    TempStr meta = HomeThumbnailMetaTemp(fs, &progress);
    if (progress > 0) {
        int dx = (int)((float)page.dx * progress);
        int x = isRtl ? page.x + page.dx - dx : page.x;
        gfx->FillRect({x, page.y + page.dy - kThumbsProgressDy, dx, kThumbsProgressDy}, ThemeAccentColor());
    }

    Rect rcName = thumb.rcText;
    rcName.dy = kThumbsCaptionLineDy;
    Str path = fs->filePath;
    TempStr fileName = path::GetBaseNameTemp(path);
    u32 fmt = gfxTextEllipsis | (isRtl ? gfxTextRight : gfxTextLeft);
    DrawMaybeHighlightedText(gfx, rcName, fileName, filterWords, highlighted, backgroundColor, isRtl, false, fmt,
                             fontText, ThemeWindowTextColor());
    if (len(meta) > 0) {
        Rect rcMeta(rcName.x, rcName.y + rcName.dy, rcName.dx, thumb.rcText.dy - rcName.dy);
        gfx->DrawText(meta, rcMeta, fmt, HomePageFont(12), ThemeWindowTextDisabledColor());
    }
}

// Slack so the first/last row's rounded outline isn't cut by rcThumbsArea.
// The search field and tip band stay outside this clip (issue #5978).
static Rect HomeOutlinePaintClip(const Rect& thumbsArea, const Rect& searchBorder, const Rect& tip, bool hasTip) {
    if (thumbsArea.IsEmpty()) {
        return {};
    }
    Rect clip = thumbsArea;
    clip.Inflate(0, DpiScale(8));
    if (!searchBorder.IsEmpty() && clip.y < searchBorder.Bottom()) {
        int d = searchBorder.Bottom() - clip.y;
        clip.y += d;
        clip.dy -= d;
    }
    if (hasTip && !tip.IsEmpty() && clip.y + clip.dy > tip.y) {
        clip.dy = tip.y - clip.y;
    }
    if (clip.dx <= 0 || clip.dy <= 0) {
        return {};
    }
    return clip;
}

static TempStr RectCsvTemp(const Rect& r) {
    return fmt("%d,%d,%d,%d", r.x, r.y, r.dx, r.dy);
}

// The home page's keyboard state: which entry the arrows have selected, how
// many entries are currently shown (the search box filters them) and whether
// the search box has the focus. A test driving the home page with keys waits on
// this instead of sleeping after each key, which is what made tests/issue-1136
// flaky: a key posted while focus was still moving went to the wrong window.
// search / outline / outlineFull are canvas rects so a test can check that the
// hover outline does not paint over the search field (issue #5978).
TempStr HomeSelectionResultTemp(int* exitCodeOut) {
    auto finish = [&](int code, TempStr s) -> TempStr {
        if (exitCodeOut) {
            *exitCodeOut = code;
        }
        return s;
    };
    MainWindow* win = len(gWindows) > 0 ? gWindows[0] : nullptr;
    if (!win) {
        return finish(2, str::DupTemp(StrL("NOTREADY no-window")));
    }
    auto& c = HomeLayout(win);
    if (!c.valid) {
        return finish(2, str::DupTemp(StrL("NOTREADY no-layout")));
    }
    int sel = win->homePageSelIdx;
    Str path;
    if (sel >= 0 && sel < len(c.thumbs) && c.thumbs[sel].fs) {
        path = c.thumbs[sel].fs->filePath;
    }
    int searchFocus = HomeSearchHasFocus(win) ? 1 : 0;
    // the search box is created while the home page lays out; until it exists
    // Up from the first row has nowhere to move the focus to
    int searchBox = win->homeSearch ? 1 : 0;
    Rect search = c.rcSearchBorder;
    Rect outlineFull;
    Rect outline;
    bool showSel = !HomePageIsListView() && !searchFocus && sel >= 0 && sel < len(c.thumbs);
    if (showSel) {
        outlineFull = HomeSelectionOutlineRect(c.thumbs[sel]);
        outline = outlineFull.Intersect(HomeOutlinePaintClip(c.rcThumbsArea, c.rcSearchBorder, c.rcTip, c.hasTip));
    }
    // caption rect of the last thumbnail: with the band scrolled to the bottom
    // it must fit inside the thumbs area (issue #6234)
    Rect lastCaption;
    if (!HomePageIsListView() && len(c.thumbs) > 0) {
        lastCaption = c.thumbs[len(c.thumbs) - 1].rcText;
    }
    // the tip shown (promo or tip, index) and its band, for picking another one
    Rect tipRect = c.hasTip ? c.rcTip : Rect{};
    return finish(0, fmt("OK sel=%d entries=%d searchFocus=%d searchBox=%d search=%s outline=%s outlineFull=%s path=%s "
                         "listView=%d listIcon=%s thumbsArea=%s lastCaption=%s tip=%d,%d tipRect=%s",
                         sel, len(c.thumbs), searchFocus, searchBox, RectCsvTemp(search), RectCsvTemp(outline),
                         RectCsvTemp(outlineFull), path, HomePageIsListView() ? 1 : 0, RectCsvTemp(c.rcIconListView),
                         RectCsvTemp(c.rcThumbsArea), RectCsvTemp(lastCaption), gSelectedIsPromo ? 1 : 0,
                         gSelectedTipIdx, RectCsvTemp(tipRect)));
}

// What the home page list drew for each row: the path, the size text as drawn,
// and the size column's rect. Reads the layout cache, so it needs a paint to
// have happened; NOTREADY until then. A test can't sample the size text from
// the pixels instead: the column's position depends on the window width, the
// DPI and the theme, so a fixed sample band lands on the path (identical for
// two renders of the same file) on any other machine. Used by tests/issue-5870.ts.
TempStr HomeListRowsResultTemp(int* exitCodeOut) {
    str::Builder out;
    auto finish = [&](int code) -> TempStr {
        if (exitCodeOut) {
            *exitCodeOut = code;
        }
        return ToStrTemp(out);
    };

    MainWindow* win = len(gWindows) > 0 ? gWindows[0] : nullptr;
    if (!win) {
        out.Append(StrL("NOTREADY no-window\n"));
        return finish(2);
    }
    auto& c = HomeLayout(win);
    if (!c.valid) {
        out.Append(StrL("NOTREADY no-layout\n"));
        return finish(2);
    }
    if (!c.listView) {
        out.Append(StrL("ERROR not-list-view\n"));
        return finish(1);
    }
    out.Append(fmt("OK rows=%d\n", len(c.thumbs)));
    for (int i = 0; i < len(c.thumbs); i++) {
        ThumbnailLayout& t = c.thumbs[i];
        Rect r = t.rcListSize;
        Str path = t.fs ? t.fs->filePath : Str{};
        // fileSize is fetched when a row is first drawn, so an off-screen row
        // still reads kSizeNotFetched and its size text is empty
        TempStr progress;
        if (gSettings && gSettings->showHomePageReadingProgress) {
            progress = FormatFileStateProgressTemp(t.fs);
        }
        if (str::IsNull(progress)) {
            progress = StrL("");
        }
        out.Append(fmt("row=%d size='%s' sizeRect=%d,%d,%d,%d progress='%s' path=%s\n", i,
                       FileSizeForHomeListTemp(t.fileSize), r.x, r.y, r.dx, r.dy, progress, path));
    }
    return finish(0);
}

//--- home page chrome VirtCtrls

HomeViewIconCtrl::HomeViewIconCtrl() {
    cursor = CursorId::Hand;
}

void HomeViewIconCtrl::Paint(VirtPaintCtx& ctx) {
    bool selected = (listView == HomePageIsListView());
    DrawHomeViewButton(ctx.gfx, pixmap, ctx.bounds, selected, HasFlag(vwfHovered));
}

HomeOpenDocCtrl::HomeOpenDocCtrl() {
    cursor = CursorId::Hand;
}

void HomeOpenDocCtrl::Paint(VirtPaintCtx& ctx) {
    if (!pixmap) {
        return;
    }
    Rect r = {ctx.bounds.x + rcIconLocal.x, ctx.bounds.y + rcIconLocal.y, pixmap->width, pixmap->height};
    ctx.gfx->DrawPixmap(pixmap, r);
}

HomeLogoRow::HomeLogoRow() {
    flags |= vwfNoHitTest;
    box = new HBox();
    box->alignMain = MainAxisAlign::MainStart;
    box->alignCross = CrossAxisAlign::CrossCenter;
}

HomeLogoRow::~HomeLogoRow() {
    // the buttons and logo are VirtCtrl children; don't let HBox delete them
    if (box) {
        VecClear(box->children);
        delete box;
        box = nullptr;
    }
}

void HomeLogoRow::AddItem(VirtCtrl* c) {
    AddChild(c);
    box->AddChild(c);
}

Size HomeLogoRow::GetIdealSize() {
    box->gap = DpiScale(10);
    return {box->MinIntrinsicWidth(0), box->MinIntrinsicHeight(0)};
}

void HomeLogoRow::SetBounds(Rect r) {
    VirtCtrl::SetBounds(r);
    box->gap = DpiScale(10);
    box->Layout(Tight(r.Size()));
    box->SetBounds(r);
}

HomeSearchBorderCtrl::HomeSearchBorderCtrl() {
    SetFlag(vwfNoHitTest, true);
}

// border and fill around the search edit; the edit HWND sits inside, so only
// the 1px frame and the padding around it are actually visible
void HomeSearchBorderCtrl::Paint(VirtPaintCtx& ctx) {
    ctx.gfx->FillRoundedRect(ctx.bounds, DpiScale(6), ThemeControlBackgroundColor(), ThemeEdgeColor());
}

//--- tip links

HomeTipCtrl::~HomeTipCtrl() {
    str::Free(richFor);
}

// re-parses when the markup changes; the parse is what the band draws
void HomeTipCtrl::SetTipLine(Str line, PlatformFont* font) {
    if (rich && str::Eq(richFor, line)) {
        rich->font = font;
        rich->hwndForCmds = hwndForCmds;
        return;
    }
    if (rich) {
        RemoveChild(rich, true);
        rich = nullptr;
    }
    str::ReplaceWithCopy(&richFor, line);
    if (len(line) == 0) {
        return;
    }
    rich = ParseTip(line);
    rich->font = font;
    rich->hwndForCmds = hwndForCmds;
    AddChild(rich);
}

// no band of its own: the markup (VirtRichText child) sits on the page
void HomeTipCtrl::Paint(VirtPaintCtx&) {}

// rcTip is the whole band (its background), rcText where the markup goes
void HomeTipCtrl::Sync(const Rect& rcTip, const Rect& rcText) {
    if (!rich || rcTip.IsEmpty()) {
        visibility = Visibility::Collapse;
        return;
    }
    visibility = Visibility::Visible;
    SetBounds(rcTip);
    rich->SetBounds(rcText);
}

//--- file entries

static Rect HomeEntryRect(const ThumbnailLayout& t);

// the ✕ sits in the top-right corner of the thumbnail (top-left in RTL)
static Rect HomeCloseBtnRectForThumb(const Rect& thumb) {
    int sz = DpiScale(18);
    int margin = DpiScale(5);
    int bx = IsUIRtl() ? (thumb.x + margin) : (thumb.x + thumb.dx - sz - margin);
    int by = thumb.y + margin;
    return {bx, by, sz, sz};
}

// the ✕ of a thumbnail / list row: forget the file it belongs to
static void HomeForgetEntryClicked(MainWindow* win, VirtMouseEvent* ev) {
    auto* entry = (HomeEntryCtrl*)ev->target->parent;
    TempStr path = str::DupTemp(entry->filePath);
    if (len(path) > 0) {
        ForgetFileFromFrequentlyRead(win, path);
    }
}

static void HomePinEntryClicked(MainWindow* win, VirtMouseEvent* ev) {
    auto* entry = (HomeEntryCtrl*)ev->target->parent;
    TempStr path = str::DupTemp(entry->filePath);
    if (len(path) == 0) {
        return;
    }
    FileState* fs = FileHistoryFindByPath(path);
    if (!fs) {
        return;
    }
    fs->isPinned = !fs->isPinned;
    ScheduleSaveSettings();
    win->DeleteToolTip();
    HomePageRelayout(win);
    win->RedrawAll(true);
}

static void HomeEntryOpenClicked(MainWindow* win, VirtMouseEvent* ev) {
    auto* entry = (HomeEntryCtrl*)ev->target;
    if (len(entry->filePath) == 0) {
        return;
    }
    LoadArgs args(entry->filePath, win);
    // ctrl forces always opening
    args.activateExisting = !ev->isCtrl;
    args.activateExistingInWindow = true;
    StartLoadDocument(&args);
}

static void HomeViewModeClicked(MainWindow* win, VirtMouseEvent* ev) {
    auto* btn = (HomeViewIconCtrl*)ev->target;
    if (btn->listView == HomePageIsListView()) {
        return;
    }
    SetHomePageListView(btn->listView);
    if (btn->listView) {
        win->DeleteToolTip();
    }
    win->homePageScrollY = 0;
    ScheduleSaveSettings();
    HomePageRelayout(win);
    win->RedrawAll(true);
}

static void HomeOpenDocClicked(MainWindow* win, VirtMouseEvent*) {
    HwndSendCommand(win->hwndFrame, CmdOpenFile);
}

static void HomeTipBandDoubleClicked(MainWindow* win, VirtMouseEvent* ev) {
    ev->didHandle = true;
    PickAnotherRandomPromotion();
    // painting reuses the layout, which holds the parsed tip
    HomePageRelayout(win);
    HwndInvalidate(win->hwndCanvas);
}

HomeListIconCtrl::HomeListIconCtrl() {
    cursor = CursorId::Hand;
    onGetTooltip = MkMethod1<HomeListIconCtrl, VirtTooltipEvent*, &HomeListIconCtrl::OnGetTooltip>(this);
}

void HomeListIconCtrl::OnGetTooltip(VirtTooltipEvent* ev) {
    auto* entry = (HomeEntryCtrl*)parent;
    FileState* fs = FileHistoryFindByPath(entry->filePath);
    bool pinned = fs && fs->isPinned;
    ev->tip = str::DupTemp(pinned ? Tr("Unpin") : Tr("Pin"));
}

HomeEntryCtrl::~HomeEntryCtrl() {
    str::Free(filePath);
}

HomeEntryCtrl::HomeEntryCtrl() {
    cursor = CursorId::Hand;
}

// paints this entry's list row or thumbnail, from our window's layout entry at
// our index (HomePageSyncChrome created us from it)
void HomeEntryCtrl::Paint(VirtPaintCtx& ctx) {
    auto* entries = (HomeEntriesCtrl*)parent;
    if (!entries || !entries->win || !entries->filterWords || !entries->highlighted) {
        return;
    }
    MainWindow* win = entries->win;
    auto& cache = HomeLayout(win);
    if (idx < 0 || idx >= len(cache.thumbs)) {
        return;
    }
    ThumbnailLayout* t = &cache.thumbs[idx];
    Gfx* gfx = ctx.gfx;
    bool isRtl = IsUIRtl();
    PlatformFont* fontText = HomePageFont(13);
    Color backgroundColor = ThemeMainWindowBackgroundColor();
    // no selection chrome while typing in the search box
    bool isSelected = (idx == win->homePageSelIdx) && !HomeSearchHasFocus(win);
    // ctx.clip is the entries band (vwfClipChildren on the parent): an entry
    // scrolled partially out must not paint over the header / tip band
    gfx->PushClip(ctx.clip);
    if (HomePageIsListView()) {
        DrawHomeListRow(gfx, *t, *entries->filterWords, *entries->highlighted, fontText, backgroundColor, isRtl,
                        isSelected);
    } else {
        DrawHomeThumbnail(gfx, *t, *entries->filterWords, *entries->highlighted, fontText, backgroundColor, isRtl);
    }
    gfx->PopClip();
}

HomeEntryCtrl* HomeEntriesCtrl::EntryAt(int idx) {
    return (HomeEntryCtrl*)ChildAt(idx);
}

// the wnd the mouse is on may be one of an entry's buttons
HomeEntryCtrl* HomeEntriesCtrl::EntryForCtrl(VirtCtrl* w) {
    while (w && w != this) {
        if (w->parent == this) {
            return (HomeEntryCtrl*)w;
        }
        w = w->parent;
    }
    return nullptr;
}

void HomeEntriesCtrl::SetEntryCount(int n) {
    while (ChildCount() > n) {
        RemoveChild(children[ChildCount() - 1], true);
    }
    while (ChildCount() < n) {
        auto* e = new HomeEntryCtrl();
        e->idx = ChildCount();
        e->onClick = MkFunc1(HomeEntryOpenClicked, win);

        e->closeBtn = new VirtCloseButton();
        // it sits on the thumbnail, so it needs the circle behind it
        e->closeBtn->withCircle = true;
        e->closeBtn->onClick = MkFunc1(HomeForgetEntryClicked, win);
        e->closeBtn->visibility = Visibility::Collapse;
        e->AddChild(e->closeBtn);

        e->removeBtn = new VirtCloseButton();
        e->removeBtn->SetTooltip(Tr("Remove from Frequently Read"));
        e->removeBtn->onClick = MkFunc1(HomeForgetEntryClicked, win);
        e->removeBtn->visibility = Visibility::Collapse;
        e->AddChild(e->removeBtn);

        e->pinBtn = new HomeListIconCtrl();
        e->pinBtn->onClick = MkFunc1(HomePinEntryClicked, win);
        e->pinBtn->visibility = Visibility::Collapse;
        e->AddChild(e->pinBtn);

        AddChild(e);
    }
    if (activeIdx >= n) {
        activeIdx = -1;
    }
}

// the ✕ shows on the entry the mouse is on, and only in thumbnail view
void HomeEntriesCtrl::UpdateCloseBtnVisibility() {
    bool canShow = CanAccessDisk() && !HomePageIsListView();
    int n = ChildCount();
    for (int i = 0; i < n; i++) {
        HomeEntryCtrl* e = EntryAt(i);
        bool show = canShow && (i == activeIdx);
        e->closeBtn->visibility = show ? Visibility::Visible : Visibility::Collapse;
    }
}

void HomeEntriesCtrl::SetActiveEntry(int idx) {
    if (idx == activeIdx) {
        return;
    }
    activeIdx = idx;
    UpdateCloseBtnVisibility();
    if (idx >= 0 && idx != win->homePageSelIdx) {
        win->homePageSelIdx = idx;
    }
    HwndInvalidate(win->hwndCanvas);
    if (idx >= 0) {
        // the tip is anchored to the active entry, never to the cursor
        HomePageShowSelectionTooltip(win);
    }
}

// mouse events bubble up to us from the entry (or one of its buttons) that was
// hit, so this is where the active entry is tracked
HomeEntriesCtrl::HomeEntriesCtrl() {
    onMouseMove = MkMethod1<HomeEntriesCtrl, VirtMouseEvent*, &HomeEntriesCtrl::OnMouseMove>(this);
}

void HomeEntriesCtrl::OnMouseMove(VirtMouseEvent* ev) {
    // keyboard nav invalidates the canvas and Windows may re-send WM_MOUSEMOVE
    // with the same coordinates: ignore those so the selection doesn't snap
    // back under a stationary cursor
    if (ev->ptWindow == lastHoverPt) {
        return;
    }
    lastHoverPt = ev->ptWindow;
    HomeEntryCtrl* e = EntryForCtrl(ev->hit);
    SetActiveEntry(e ? e->idx : -1);
    return;
}

// TOOLTIPS_CLASS default for TTDT_INITIAL
static int TooltipInitialDelayMs() {
    int ms = (int)GetDoubleClickTime();
    return ms > 0 ? ms : 500;
}

constexpr UINT_PTR kHomeAboutHoverTimerID = 100;
constexpr UINT_PTR kHomeAboutHoverHideTimerID = 102;
// long enough to cross from the title onto the popup. The cursor is read when
// the timer fires, not when the leave message was queued.
constexpr UINT kAboutHoverHideDelayMs = 200;

static HomeChromeCtrl* HomeChrome(MainWindow* win) {
    if (!win || !win->homeRoot) {
        return nullptr;
    }
    if (!IsVirtCtrlOfKind(win->homeRoot->owned, kindHomeChromeCtrl)) {
        return nullptr;
    }
    return (HomeChromeCtrl*)win->homeRoot->owned;
}

static Rect HomeLogoScreenRect(HomeChromeCtrl* chrome) {
    if (!chrome || !chrome->logo) {
        return {};
    }
    HWND hwnd = chrome->GetHwnd();
    if (!hwnd) {
        return {};
    }
    Rect logo = chrome->logo->BoundsInWindow();
    Point origin = HwndClientToScreen(hwnd, Point());
    return {origin.x + logo.x, origin.y + logo.y, logo.dx, logo.dy};
}

static bool CursorOverHomeLogo(HomeChromeCtrl* chrome) {
    return HomeLogoScreenRect(chrome).Contains(GetCursorPosition());
}

static bool CursorOverAboutHover(HomeChromeCtrl* chrome) {
    return chrome && chrome->aboutHover && chrome->aboutHover->IsVisible() &&
           chrome->aboutHover->ScreenRect().Contains(GetCursorPosition());
}

static void CancelHomeAboutHoverTimer(MainWindow* win) {
    if (win && win->hwndCanvas) {
        KillTimer(win->hwndCanvas, kHomeAboutHoverTimerID);
        KillTimer(win->hwndCanvas, kHomeAboutHoverHideTimerID);
    }
}

static void HideHomeAboutHover(MainWindow* win) {
    CancelHomeAboutHoverTimer(win);
    HomeChromeCtrl* chrome = HomeChrome(win);
    if (chrome && chrome->aboutHover) {
        chrome->aboutHover->Show(false);
    }
}

static bool CursorOverAboutOrLogo(MainWindow* win) {
    HomeChromeCtrl* chrome = HomeChrome(win);
    return CursorOverHomeLogo(chrome) || CursorOverAboutHover(chrome);
}

static void CALLBACK HomeAboutHoverHideTimerProc(HWND hwnd, UINT, UINT_PTR id, DWORD) {
    KillTimer(hwnd, id);
    MainWindow* win = FindMainWindowByHwnd(hwnd);
    if (!win || !IsMainWindowValidAndNotClosing(win)) {
        return;
    }
    if (CursorOverAboutOrLogo(win)) {
        return;
    }
    HideHomeAboutHover(win);
}

static void ScheduleHideHomeAboutHover(MainWindow* win) {
    if (!win || !win->hwndCanvas || CursorOverAboutOrLogo(win)) {
        return;
    }
    HomeChromeCtrl* chrome = HomeChrome(win);
    if (!chrome || !chrome->aboutHover || !chrome->aboutHover->IsVisible()) {
        return;
    }
    SetTimer(win->hwndCanvas, kHomeAboutHoverHideTimerID, kAboutHoverHideDelayMs, HomeAboutHoverHideTimerProc);
}

static void OnHomeAboutHoverLeave(MainWindow* win) {
    ScheduleHideHomeAboutHover(win);
}

static void CALLBACK HomeAboutHoverTimerProc(HWND hwnd, UINT, UINT_PTR id, DWORD) {
    KillTimer(hwnd, id);
    MainWindow* win = FindMainWindowByHwnd(hwnd);
    if (!win || !IsMainWindowValidAndNotClosing(win)) {
        return;
    }
    ShowHomeAboutHover(win);
}

static void OnHomeLogoEnter(MainWindow* win) {
    if (!win || !win->hwndCanvas || !win->IsCurrentTabAbout()) {
        return;
    }
    HomeChromeCtrl* chrome = HomeChrome(win);
    if (chrome && chrome->aboutHover && chrome->aboutHover->IsVisible()) {
        return;
    }
    CancelHomeAboutHoverTimer(win);
    SetTimer(win->hwndCanvas, kHomeAboutHoverTimerID, (UINT)TooltipInitialDelayMs(), HomeAboutHoverTimerProc);
}

static void OnHomeLogoLeave(MainWindow* win) {
    ScheduleHideHomeAboutHover(win);
}

// chrome-less About box under the home-page logo; links stay clickable
static void ShowHomeAboutHover(MainWindow* win) {
    if (!win || !IsMainWindowValidAndNotClosing(win) || !win->IsCurrentTabAbout()) {
        return;
    }
    HomeChromeCtrl* chrome = HomeChrome(win);
    if (!chrome || !chrome->logo || !CursorOverHomeLogo(chrome)) {
        return;
    }

    if (!chrome->aboutHover) {
        VirtHost::CreateArgs args;
        args.parent = win->hwndFrame;
        args.className = WStrL(L"SUMATRA_ABOUT_HOVER");
        args.isPopup = true;
        args.noActivate = true;
        args.visible = false;
        args.bgColor = ThemeMainWindowBackgroundColor();
        args.userData = win;
        chrome->aboutHover = VirtHost::Create(args);
        if (!chrome->aboutHover) {
            return;
        }
        chrome->aboutHover->onMouseLeave = MkFunc0(OnHomeAboutHoverLeave, win);
        ULONG_PTR cls = GetClassLongPtrW(chrome->aboutHover->native, GCL_STYLE);
        SetClassLongPtrW(chrome->aboutHover->native, GCL_STYLE, (LONG_PTR)(cls | CS_DROPSHADOW));
    }

    VirtHost* host = chrome->aboutHover;
    host->bgColor = ThemeMainWindowBackgroundColor();
    DpiSetFromHwnd(host->native);

    Rect measureRc{0, 0, 2000, 2000};
    AboutCtrl* about = UpdateAboutLayout(&host->vroot, host->native, measureRc, true);
    Size box = about->aboutRect.Size();
    if (box.IsEmpty()) {
        return;
    }

    Rect logoScreen = HomeLogoScreenRect(chrome);
    Rect pos{logoScreen.x + ((logoScreen.dx - box.dx) / 2), logoScreen.Bottom(), box.dx, box.dy};
    pos = ShiftRectToWorkArea(pos, win->hwndCanvas, true);
    host->SetPos(pos, true);
    UpdateAboutLayout(&host->vroot, host->native, {0, 0, box.dx, box.dy}, true);
    host->Invalidate();
}

HomeChromeCtrl::~HomeChromeCtrl() {
    HWND hwnd = GetHwnd();
    if (hwnd) {
        KillTimer(hwnd, kHomeAboutHoverTimerID);
        KillTimer(hwnd, kHomeAboutHoverHideTimerID);
    }
    delete aboutHover;
    aboutHover = nullptr;
}

// created once per window so that hover / pressed state survives the repaints
// that scrolling and filtering cause
static HomeChromeCtrl* EnsureHomeChrome(MainWindow* win) {
    // the canvas root holds either the home page's chrome or the About page's
    // controls, depending on which one is showing
    if (win->homeRoot && IsVirtCtrlOfKind(win->homeRoot->owned, kindHomeChromeCtrl)) {
        return (HomeChromeCtrl*)win->homeRoot->owned;
    }
    HWND hwnd = win->hwndCanvas;
    if (!win->homeRoot) {
        win->homeRoot = new VirtRoot(hwnd);
    }

    auto* chrome = new HomeChromeCtrl();
    chrome->kind = kindHomeChromeCtrl;
    chrome->flags |= vwfNoHitTest;

    // first, so that the rest of the chrome hit-tests and paints on top of the
    // entries. Below everything else: the tip band sits at the bottom of the page
    chrome->tip = new HomeTipCtrl();
    chrome->tip->hwndForCmds = win->hwndFrame;
    chrome->tip->onDoubleClick = MkFunc1(HomeTipBandDoubleClicked, win);
    chrome->AddChild(chrome->tip);

    chrome->searchBorder = new HomeSearchBorderCtrl();
    chrome->AddChild(chrome->searchBorder);

    chrome->entries = new HomeEntriesCtrl();
    chrome->entries->win = win;
    // hit-testable so that moving into the gaps between thumbnails still
    // reaches OnMouseMove() and clears the active entry
    chrome->entries->flags |= vwfClipChildren;
    chrome->AddChild(chrome->entries);

    chrome->thumbView = new HomeViewIconCtrl();
    chrome->thumbView->listView = false;
    chrome->thumbView->SetTooltip(Tr("Show as thumbnails"));
    chrome->thumbView->onClick = MkFunc1(HomeViewModeClicked, win);
    chrome->AddChild(chrome->thumbView);

    chrome->listView = new HomeViewIconCtrl();
    chrome->listView->listView = true;
    chrome->listView->SetTooltip(Tr("Show as list"));
    chrome->listView->onClick = MkFunc1(HomeViewModeClicked, win);
    chrome->AddChild(chrome->listView);

    chrome->hdr = new VirtText(StrL(""));
    chrome->AddChild(chrome->hdr);

    // the brand at the top left. It is hit-testable so hovering it can open
    // the About dropdown.
    chrome->logoRow = new HomeLogoRow();
    chrome->logo = new SumatraLogo();
    chrome->logo->name = StrL("homeLogo");
    chrome->logo->SetFlag(vwfNoHitTest, false);
    chrome->logo->onMouseEnter = MkFunc0(OnHomeLogoEnter, win);
    chrome->logo->onMouseLeave = MkFunc0(OnHomeLogoLeave, win);
    chrome->logoRow->AddItem(chrome->logo);
    chrome->AddChild(chrome->logoRow);

    chrome->openDoc = new HomeOpenDocCtrl();
    chrome->openDoc->text = new VirtText(StrL(""));
    chrome->openDoc->AddChild(chrome->openDoc->text);
    chrome->openDoc->onClick = MkFunc1(HomeOpenDocClicked, win);
    chrome->AddChild(chrome->openDoc);

    win->homeRoot->SetChild(chrome);
    return chrome;
}

void HomePageDestroyChrome(MainWindow* win) {
    ClearHomeLayoutCache(win);
    delete win->homeLayout;
    win->homeLayout = nullptr;
    CancelHomeAboutHoverTimer(win);
    delete win->homeRoot;
    win->homeRoot = nullptr;
}

// gives the home page's virtual controls first shot at the canvas messages.
// Returns true when the event was consumed and the caller should stop
bool HomePageOnCanvasMessage(MainWindow* win, UINT msg, WPARAM wp, LPARAM lp, LRESULT& res) {
    VirtRoot* root = win->homeRoot;
    if (!root || !root->owned) {
        return false;
    }
    // Hover feedback (highlight, ✕ button, tooltips) must stay quiet while
    // another window is in front. The mouse still moves over the home page when
    // e.g. the command palette or the theme window is up, and putting a tooltip
    // over them steals activation: the palette closes on kill-focus and the
    // theme window ends up behind the main window. Clicks are exempt: clicking
    // a background window is meant to activate it.
    bool isHoverMsg = (msg == WM_MOUSEMOVE) || (msg == WM_SETCURSOR);
    if (isHoverMsg && GetForegroundWindow() != win->hwndFrame) {
        // thumbnail hover / tooltips stay quiet so they don't steal activation
        // from e.g. the command palette. The About dropdown is WS_EX_NOACTIVATE,
        // so hovering the logo still shows it.
        if (msg == WM_MOUSEMOVE) {
            Point pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            ILayout* el = ElementFromPoint(root, pt, nullptr);
            VirtCtrl* w = el ? el->AsVirtCtrl() : nullptr;
            if (w && IsVirtCtrlOfKind(w, kindSumatraLogo)) {
                LRESULT ignored = 0;
                root->OnMessage(msg, wp, lp, ignored);
            } else {
                ScheduleHideHomeAboutHover(win);
            }
        }
        return false;
    }
    if (msg != WM_SETCURSOR) {
        bool didHandle = root->OnMessage(msg, wp, lp, res);
        // moving outside the entries band (or off the canvas) drops the active
        // entry, so the ✕ button goes away
        if (msg == WM_MOUSEMOVE && !root->hovered) {
            HomeEntriesCtrl* entries = HomeEntries(win);
            if (entries) {
                entries->SetActiveEntry(-1);
            }
        }
        // not on the title: the popup closes once the cursor has settled off it
        if (msg == WM_MOUSEMOVE && !IsVirtCtrlOfKind(root->hovered, kindSumatraLogo)) {
            ScheduleHideHomeAboutHover(win);
        }
        return didHandle;
    }
    Point pt = HwndGetCursorPos(win->hwndCanvas);
    Point ptLocal{0, 0};
    ILayout* el = ElementFromPoint(root, pt, &ptLocal);
    VirtCtrl* w = el ? el->AsVirtCtrl() : nullptr;
    if (!w || !w->OnSetCursor(ptLocal)) {
        return false;
    }
    // no tooltip of its own means "leave the tooltip alone", not "hide it": a
    // thumbnail entry's tip is put up by HomePageShowSelectionTooltip() and would
    // otherwise be torn down by the WM_SETCURSOR that follows the hover
    TempStr tip = w->GetTooltipTemp(ptLocal);
    if (tip && *tip.s) {
        Rect r = w->BoundsInWindow();
        win->ShowToolTip(tip, r);
    } else if (HomePageIsListView()) {
        HomeEntriesCtrl* entries = HomeEntries(win);
        if (entries && entries->EntryForCtrl(w)) {
            win->DeleteToolTip();
        }
    }
    res = TRUE;
    return true;
}

// feeds the geometry LayoutHomePage() computed into the persistent chrome tree
static void HomePageSyncChrome(HomePageLayout& l) {
    MainWindow* win = l.win;
    HomeChromeCtrl* chrome = EnsureHomeChrome(win);
    VirtRoot* root = win->homeRoot;
    // the chrome positions its children itself, so don't let the root re-layout
    root->bounds = l.rc;
    root->needsLayout = false;
    chrome->SetBounds(l.rc);

    chrome->tip->Sync(l.rcTip, l.rcTipText);

    chrome->searchBorder->visibility = l.rcSearchBorder.IsEmpty() ? Visibility::Collapse : Visibility::Visible;
    chrome->searchBorder->SetBounds(l.rcSearchBorder);

    // file entries: clipped to the thumbnails band, like the static links were
    HomeEntriesCtrl* entries = chrome->entries;
    entries->SetBounds(l.rcThumbsArea);
    auto& cache = HomeLayout(win);
    entries->filterWords = &cache.filterWords;
    entries->highlighted = &cache.highlighted;
    int nEntries = len(cache.thumbs);
    entries->SetEntryCount(nEntries);
    bool listView = HomePageIsListView();
    for (int i = 0; i < nEntries; i++) {
        ThumbnailLayout& t = cache.thumbs[i];
        HomeEntryCtrl* e = entries->EntryAt(i);
        e->idx = i;
        Str path = t.fs ? t.fs->filePath : Str{};
        if (!str::Eq(e->filePath, path)) {
            str::ReplaceWithCopy(&e->filePath, path);
        }
        Rect rc = HomeEntryRect(t);
        e->visibility = rc.IsEmpty() ? Visibility::Collapse : Visibility::Visible;
        e->SetBounds(rc);
        if (listView) {
            e->removeBtn->visibility = Visibility::Visible;
            e->removeBtn->SetBounds(t.rcListRemove);
            e->pinBtn->visibility = Visibility::Visible;
            e->pinBtn->SetBounds(t.rcListPin);
        } else {
            e->removeBtn->visibility = Visibility::Collapse;
            e->pinBtn->visibility = Visibility::Collapse;
            // relative to the on-screen part of the entry, so the ✕ stays
            // visible on a thumbnail scrolled half-way out of the band
            e->closeBtn->SetBounds(HomeCloseBtnRectForThumb(rc.Intersect(l.rcThumbsArea)));
        }
    }
    entries->UpdateCloseBtnVisibility();

    Size iconSize = l.rcIconThumbnailView.Size();
    chrome->thumbView->pixmap = GetCachedPixmapForSvg(Str(gIconHomeThumbnails), iconSize.dx, iconSize.dy);
    chrome->thumbView->SetBounds(l.rcIconThumbnailView);
    chrome->listView->pixmap = GetCachedPixmapForSvg(Str(gIconHomeList), iconSize.dx, iconSize.dy);
    chrome->listView->SetBounds(l.rcIconListView);

    // re-apply: the bounds were set before the parent was positioned
    chrome->hdr->SetBounds(l.freqRead->lastBounds);

    // font also set here so the cached-layout path (ApplyHomeLayoutCache)
    // repaints the brand without a full relayout
    chrome->logo->font = HomeBrandFont();
    chrome->logoRow->SetBounds(l.rcLogo);
    if (chrome->aboutHover && chrome->aboutHover->IsVisible()) {
        Size sz = chrome->aboutHover->ScreenRect().Size();
        Rect logoScreen = HomeLogoScreenRect(chrome);
        Rect want{logoScreen.x + ((logoScreen.dx - sz.dx) / 2), logoScreen.Bottom(), sz.dx, sz.dy};
        want = ShiftRectToWorkArea(want, l.win->hwndCanvas, true);
        if (want != chrome->aboutHover->ScreenRect()) {
            chrome->aboutHover->SetPos(want, true);
        }
    }

    // one click target covering the icon and the link text
    Rect rcOpen = l.rcIconOpen.Union(l.openDoc->lastBounds);
    rcOpen.Inflate(10, 10);
    HomeOpenDocCtrl* od = chrome->openDoc;
    od->pixmap = GetCachedPixmapForSvg(Str(gIconFileOpen), l.rcIconOpen.dx, l.rcIconOpen.dy);
    od->SetBounds(rcOpen);
    od->rcIconLocal = {l.rcIconOpen.x - rcOpen.x, l.rcIconOpen.y - rcOpen.y, l.rcIconOpen.dx, l.rcIconOpen.dy};
    od->text->SetColor(kColText, ThemeWindowTextColor());
    // re-apply now that the parent moved: bounds are relative to it
    od->text->SetBounds(l.openDoc->lastBounds);
}

static void DrawHomePageLayout(HomePageLayout& l) {
    Gfx* gfx = l.gfx;
    auto* win = l.win;

    gfx->FillRect(l.rc, ThemeMainWindowBackgroundColor());

    int nThumbs = len(l.thumbnails);
    // keep the keyboard selection inside the (possibly filtered) list
    if (win->homePageSelIdx >= nThumbs) {
        win->homePageSelIdx = nThumbs - 1;
    }

    // the chrome tree paints everything else: search border, file entries
    // (thumbnails / list rows), tip band, header (palette / logo / help),
    // view buttons, and "Open a document..."
    win->homeRoot->Paint(gfx, l.rc);

    // thumbnails selection outline: after the entries so it sits on top of the
    // thumbnail, clipped so it cannot paint over the search field or the tip
    // band (issue #5978). A little slack above/below the thumbs band keeps the
    // first row's top curve from being cut off.
    int selIdx = win->homePageSelIdx;
    bool showSel = !HomePageIsListView() && !HomeSearchHasFocus(win) && selIdx >= 0 && selIdx < nThumbs;
    if (showSel) {
        ThumbnailLayout& t = l.thumbnails[selIdx];
        if (IsHomeThumbOnScreen(t.rcPage.Union(t.rcText), l.rcThumbsArea)) {
            Rect clip = HomeOutlinePaintClip(l.rcThumbsArea, l.rcSearchBorder, l.rcTip, l.hasTip);
            if (!clip.IsEmpty()) {
                gfx->PushClip(clip);
                DrawHomeSelectionOutline(gfx, HomeSelectionOutlineRect(t), DpiScale(2));
                gfx->PopClip();
            }
        }
        // the edit HWND is on top of the canvas; this is the canvas-drawn
        // border/fill around it. Repaint so a 1px antialiased leak cannot
        // show through the field.
        HomeChromeCtrl* chrome = HomeChrome(win);
        if (chrome && chrome->searchBorder) {
            chrome->searchBorder->PaintStandalone(gfx);
        }
    }
}

static bool HomePageShouldShow(MainWindow* win) {
    if (!win || !win->IsCurrentTabAbout()) {
        return false;
    }
    if (!HasPermission(Perm::SavePreferences | Perm::DiskAccess)) {
        return false;
    }
    return gSettings && SettingsRememberOpenedFiles() && gSettings->showStartPage;
}

static void UpdateHomeOverlayScrollbar(MainWindow* win) {
    auto& c = HomeLayout(win);
    bool show = c.valid && ScrollbarsUseOverlay() && c.totalContentDy > c.thumbsVisibleDy;
    if (show) {
        if (!win->overlayScrollV) {
            win->overlayScrollV =
                OverlayScrollbarCreate(win->hwndCanvas, OverlayScrollbar::Type::Vert, ScrollbarsOverlayMode());
        }
        SCROLLINFO si{};
        si.cbSize = sizeof(si);
        si.fMask = SIF_ALL;
        si.nMin = 0;
        si.nMax = c.totalContentDy - 1;
        si.nPage = c.thumbsVisibleDy;
        si.nPos = win->homePageScrollY;
        OverlayScrollbarShow(win->overlayScrollV, true);
        OverlayScrollbarSetInfo(win->overlayScrollV, &si, TRUE);
    }
    OverlayScrollbarShow(win->overlayScrollV, show);
}

void HomePageCreate(MainWindow* win) {
    if (!win || !win->hwndCanvas) {
        return;
    }
    EnsureHomeChrome(win);
    EnsureHomeSearchCreated(win);
}

void HomePageRelayout(MainWindow* win) {
    if (!win || !win->hwndCanvas) {
        return;
    }
    if (!HomePageShouldShow(win)) {
        HomePageHideSearch(win);
        return;
    }
    EnsureHomeChrome(win);
    EnsureHomeSearchCreated(win);

    HomePageLayout l;
    l.rc = HwndClientRect(win->hwndCanvas);
    l.win = win;
    if (l.rc.IsEmpty()) {
        HomePageHideSearch(win);
        return;
    }

    TempStr filterText = HomeSearchQueryTemp(win);
    bool usedCache = false;
    if (HomeLayoutCacheMatches(win, l.rc, filterText)) {
        Vec<FileState*> files;
        StrVec filterWords;
        CollectHomePageFiles(win, files, filterWords);
        if (HomeLayoutCacheFilesMatch(win, files)) {
            ApplyHomeLayoutCache(l, win->homePageScrollY);
            usedCache = true;
        }
    }
    if (!usedCache) {
        LayoutHomePage(l);
        SaveHomeLayoutCache(l, filterText, win->homePageScrollY);
    }
    HomePageSyncChrome(l);
    PlaceHomeSearchEdit(win, l.rcSearchBorder);
    if (win->homeSearch) {
        win->homeSearch->SetIsVisible(true);
    }
    UpdateHomeSearchCueBanner(win);
    UpdateHomeOverlayScrollbar(win);
}

void DrawHomePage(MainWindow* win, Gfx* gfx) {
    if (!HomeLayout(win).valid || !win->homeRoot) {
        HomePageRelayout(win);
    }
    if (!HomeLayout(win).valid || !win->homeRoot) {
        return;
    }

    auto& c = HomeLayout(win);
    HomePageLayout l;
    l.win = win;
    l.gfx = gfx;
    l.rc = c.canvasRc;
    l.rcThumbsArea = c.rcThumbsArea;
    l.rcSearchBorder = c.rcSearchBorder;
    l.rcTip = c.rcTip;
    l.hasTip = c.hasTip;
    l.thumbnails = c.thumbs;
    DrawHomePageLayout(l);
}

// --- keyboard navigation of the file list (issue #1136) ---

// Selection works off the layout cache, filled by HomePageRelayout.
static int HomeSelectableCount(MainWindow* win) {
    auto& c = HomeLayout(win);
    return c.valid ? len(c.thumbs) : 0;
}

// bounding box of an entry, in window coordinates for the current scroll
static Rect HomeEntryRect(const ThumbnailLayout& t) {
    if (HomePageIsListView()) {
        return t.rcListRow;
    }
    return t.rcPage.Union(t.rcText);
}

// how many thumbnails fit in a grid row: the run of entries sharing the y of
// the first one
static int HomeGridColumnCount(MainWindow* win) {
    auto& c = HomeLayout(win);
    int n = len(c.thumbs);
    if (n == 0) {
        return 1;
    }
    int y0 = c.thumbs[0].rcCell.y;
    int nCols = 0;
    while (nCols < n && c.thumbs[nCols].rcCell.y == y0) {
        nCols++;
    }
    return nCols > 0 ? nCols : 1;
}

// Select the first-row entry at the column remembered when leaving for search.
static void HomeSelectFromSearchReturnCol(MainWindow* win) {
    int n = HomeSelectableCount(win);
    if (n <= 0) {
        win->homePageSelIdx = 0;
        return;
    }
    if (HomePageIsListView()) {
        win->homePageSelIdx = 0;
        return;
    }
    int nCols = HomeGridColumnCount(win);
    nCols = std::max(nCols, 1);
    int col = win->homePageSearchReturnCol;
    col = std::max(col, 0);
    if (col >= nCols) {
        col = nCols - 1;
    }
    // first row only has min(nCols, n) entries
    int firstRowN = n < nCols ? n : nCols;
    if (col >= firstRowN) {
        col = firstRowN - 1;
    }
    win->homePageSelIdx = col;
}

// Keep layout-cache thumb rects in sync with homePageScrollY (without a full
// paint) so keyboard tooltips can use up-to-date geometry after scroll.
static void HomeSyncLayoutCacheScroll(MainWindow* win) {
    auto& c = HomeLayout(win);
    if (!c.valid || !win) {
        return;
    }
    int scrollY = win->homePageScrollY;
    int maxScrollY = std::max(0, c.totalContentDy - c.thumbsVisibleDy);
    if (scrollY > maxScrollY) {
        scrollY = maxScrollY;
        win->homePageScrollY = scrollY;
    }
    if (scrollY < 0) {
        scrollY = 0;
        win->homePageScrollY = 0;
    }
    int dy = c.scrollY - scrollY;
    OffsetThumbnailLayouts(c.thumbs, dy);
    c.scrollY = scrollY;
}

// scroll so the selected entry is fully visible
static void HomeScrollSelectionIntoView(MainWindow* win) {
    auto& c = HomeLayout(win);
    int idx = win->homePageSelIdx;
    if (!c.valid || idx < 0 || idx >= len(c.thumbs)) {
        return;
    }
    // rects must match current scroll before we measure visibility
    HomeSyncLayoutCacheScroll(win);
    Rect r = HomeEntryRect(c.thumbs[idx]);
    const Rect& area = c.rcThumbsArea;
    if (r.IsEmpty() || area.IsEmpty()) {
        return;
    }
    // scrollY grows as the content moves up
    int dy = 0;
    if (r.y < area.y) {
        dy = r.y - area.y;
    } else if (r.y + r.dy > area.y + area.dy) {
        dy = (r.y + r.dy) - (area.y + area.dy);
    }
    if (dy == 0) {
        return;
    }
    int newScrollY = win->homePageScrollY + dy;
    newScrollY = std::max(newScrollY, 0);
    win->homePageScrollY = newScrollY; // layout clamps against content height
    HomeSyncLayoutCacheScroll(win);
}

// Selection outline rect — must match DrawHomePageLayout / DrawHomeListRow.
static Rect HomeSelectionOutlineRect(const ThumbnailLayout& t) {
    if (HomePageIsListView()) {
        // list: outline is the row, 1px shorter (separator line), with 0.5rem
        // of breathing room on the left and right
        int pad = DpiScale(8);
        return {t.rcListRow.x - pad, t.rcListRow.y, t.rcListRow.dx + (2 * pad), t.rcListRow.dy - 1};
    }
    // thumbnails: a ring just outside the page
    Rect sel = t.rcPage;
    sel.Inflate(kThumbsSelectionGap, kThumbsSelectionGap);
    return sel;
}

// Show/update the infotip for the keyboard-selected home thumbnail. Placed just
// below the blue selection outline, left-aligned with the outline's left edge;
// shifted left if it would extend past the right edge of the last outline in
// that row.
static void HomePageShowSelectionTooltip(MainWindow* win) {
    if (!win || HomePageIsListView() || HomeSearchHasFocus(win)) {
        if (win) {
            win->DeleteToolTip();
        }
        return;
    }
    // never put a tip up from a window that isn't in front: track-mode tips are
    // topmost popups and showing one steals activation, which pulls the frame
    // over the command palette / Change Theme window
    if (GetForegroundWindow() != win->hwndFrame) {
        return;
    }
    auto& c = HomeLayout(win);
    int idx = win->homePageSelIdx;
    if (!c.valid || idx < 0 || idx >= len(c.thumbs)) {
        win->DeleteToolTip();
        return;
    }
    HomeSyncLayoutCacheScroll(win);
    ThumbnailLayout& t = c.thumbs[idx];
    FileState* fs = t.fs;
    if (!fs || len(fs->filePath) == 0) {
        win->DeleteToolTip();
        return;
    }

    // Same text as hover: path + size (size looked up only when shown)
    TempStr tip = str::DupTemp(fs->filePath);
    i64 size = file::GetSize(fs->filePath);
    if (size >= 0) {
        tip = fmt("%s  %s", tip, str::FormatSizeShortTemp(size, nullptr));
    }

    HWND hwnd = win->hwndCanvas;
    Rect outline = HomeSelectionOutlineRect(t);
    // a little below the outline so the tip clears the blue border
    int tipClientX = outline.x;
    int tipClientY = outline.y + outline.dy + DpiScale(4);

    int rightEdgeClient = outline.x + outline.dx;
    if (!HomePageIsListView()) {
        int n = len(c.thumbs);
        int nCols = HomeGridColumnCount(win);
        nCols = std::max(nCols, 1);
        int col = idx % nCols;
        int rowStart = idx - col;
        int lastInRow = rowStart + nCols - 1;
        if (lastInRow >= n) {
            lastInRow = n - 1;
        }
        Rect lastOutline = HomeSelectionOutlineRect(c.thumbs[lastInRow]);
        rightEdgeClient = lastOutline.x + lastOutline.dx;
    }

    POINT tl{tipClientX, tipClientY};
    POINT tr{rightEdgeClient, tipClientY};
    ClientToScreen(hwnd, &tl);
    ClientToScreen(hwnd, &tr);
    win->ShowToolTipAt(tip, outline, Point(tl.x, tl.y), false, tr.x);
}

// select the first entry, e.g. after the filter changed the list
void HomePageSelectFirst(MainWindow* win) {
    win->homePageSelIdx = 0;
    win->homePageSearchReturnCol = 0;
}

// hide keyboard-selection tip on deactivate; restore it when the frame is active
void HomePageOnWindowActivate(MainWindow* win, bool active) {
    if (!win) {
        return;
    }
    if (!active || IsIconic(win->hwndFrame)) {
        // Also when the frame is iconic: activate can fire while minimized and
        // ClientToScreen then pins the tip at the top-left of the desktop (#5928).
        win->DeleteToolTip();
        // a click in the no-activate About popup can report WA_INACTIVE while
        // the frame is still the foreground window; don't close it then
        HWND fg = GetForegroundWindow();
        HomeChromeCtrl* chrome = HomeChrome(win);
        bool stillUs = fg == win->hwndFrame || (chrome && chrome->aboutHover && fg == chrome->aboutHover->native);
        if (!stillUs) {
            HideHomeAboutHover(win);
        }
        return;
    }
    // only restore the selection tip (positioned at the active entry, not cursor)
    if (win->IsCurrentTabAbout()) {
        HomePageShowSelectionTooltip(win);
    }
}

// the entries wnd of the chrome tree, if the home page is showing
static HomeEntriesCtrl* HomeEntries(MainWindow* win) {
    HomeChromeCtrl* chrome = HomeChrome(win);
    return chrome ? chrome->entries : nullptr;
}

// mouse left the canvas (or the page scrolled): drop the active entry so the
// close button goes away
void HomePageClearActiveEntry(MainWindow* win) {
    HomeEntriesCtrl* entries = HomeEntries(win);
    if (!entries) {
        return;
    }
    if (win->homeRoot) {
        win->homeRoot->ClearHover();
    }
    entries->SetActiveEntry(-1);
}

// File of the entry at (x,y), empty if there is no entry there. Replaces the
// old "look the click up in win->staticLinks" - entries are VirtCtrls now
Str HomePageFilePathAtTemp(MainWindow* win, int x, int y) {
    HomeEntriesCtrl* entries = HomeEntries(win);
    if (!entries) {
        return {};
    }
    Point ptLocal{0, 0};
    ILayout* el = ElementFromPoint(win->homeRoot, {x, y}, &ptLocal);
    VirtCtrl* w = el ? el->AsVirtCtrl() : nullptr;
    HomeEntryCtrl* e = entries->EntryForCtrl(w);
    if (!e) {
        return {};
    }
    return str::DupTemp(e->filePath);
}

// Mouse over a file entry: update homePageSelIdx and, in thumbnail view, show
// the tip at that entry (not at the cursor).
bool HomePageOnHover(MainWindow* win, int x, int y) {
    HomeEntriesCtrl* entries = HomeEntries(win);
    if (!entries) {
        return false;
    }
    Point ptLocal{0, 0};
    ILayout* el = ElementFromPoint(win->homeRoot, {x, y}, &ptLocal);
    VirtCtrl* w = el ? el->AsVirtCtrl() : nullptr;
    HomeEntryCtrl* e = entries->EntryForCtrl(w);
    if (!e) {
        return false;
    }
    entries->SetActiveEntry(e->idx);
    return true;
}

// file of the keyboard-selected entry, empty if there's no selection
Str HomePageSelectedFilePathTemp(MainWindow* win) {
    auto& c = HomeLayout(win);
    int idx = win->homePageSelIdx;
    if (!c.valid || idx < 0 || idx >= len(c.thumbs)) {
        return {};
    }
    FileState* fs = c.thumbs[idx].fs;
    if (!fs) {
        return {};
    }
    return str::DupTemp(fs->filePath);
}

// keyboard navigation of the file list (issue #1136). dCol/dRow are in grid
// steps; in list view only dRow matters. Moving up past the first row puts
// focus in the search box
void HomePageMoveSelection(MainWindow* win, int dCol, int dRow) {
    int n = HomeSelectableCount(win);
    if (n == 0) {
        win->DeleteToolTip();
        return;
    }
    int idx = win->homePageSelIdx;
    if (idx < 0 || idx >= n) {
        // nothing selected yet: any arrow key selects the first entry
        win->homePageSelIdx = 0;
        HomeScrollSelectionIntoView(win);
        HwndInvalidate(win->hwndCanvas);
        HomePageShowSelectionTooltip(win);
        return;
    }

    int nCols = HomePageIsListView() ? 1 : HomeGridColumnCount(win);
    int delta;
    if (HomePageIsListView()) {
        // one entry per row; left/right have nothing to move along
        delta = dRow;
    } else {
        delta = dCol + (dRow * nCols);
    }
    if (delta == 0) {
        return;
    }
    int newIdx = idx + delta;
    if (newIdx < 0) {
        // above the first row: hand focus to the search box, remember column
        if (dRow < 0 && win->homeSearch) {
            win->homePageSearchReturnCol = HomePageIsListView() ? 0 : (idx % nCols);
            win->DeleteToolTip();
            EditSetFocus(win->homeSearch);
            HwndInvalidate(win->hwndCanvas); // drop selection outline while typing
            return;
        }
        newIdx = 0;
    }
    if (newIdx >= n) {
        newIdx = n - 1;
    }
    if (newIdx == idx) {
        return;
    }
    win->homePageSelIdx = newIdx;
    HomeScrollSelectionIntoView(win);
    HomePageRelayout(win);
    HwndInvalidate(win->hwndCanvas);
    HomePageShowSelectionTooltip(win);
}

void HomePageOnVScroll(MainWindow* win, WPARAM wp) {
    USHORT msg = LOWORD(wp);
    int lineDy = HomePageIsListView() ? kHomeListRowDy : ThumbnailFrameSize().dy + kThumbsSpaceBetweenY;
    int pageDy = lineDy * 3;

    int newScrollY = win->homePageScrollY;
    switch (msg) {
        case SB_LINEUP:
            newScrollY -= lineDy;
            break;
        case SB_LINEDOWN:
            newScrollY += lineDy;
            break;
        case SB_PAGEUP:
            newScrollY -= pageDy;
            break;
        case SB_PAGEDOWN:
            newScrollY += pageDy;
            break;
        case SB_THUMBTRACK:
        case SB_THUMBPOSITION: {
            int pos = (int)(short)HIWORD(wp);
            // overlay scrollbar sends full position in HIWORD for THUMBTRACK
            if (win->overlayScrollV) {
                pos = win->overlayScrollV->nTrackPos;
            }
            newScrollY = pos;
            break;
        }
        case SB_TOP:
            newScrollY = 0;
            break;
        case SB_BOTTOM:
            newScrollY = INT_MAX; // will be clamped by layout
            break;
    }
    newScrollY = std::max(newScrollY, 0);
    if (newScrollY != win->homePageScrollY) {
        win->homePageScrollY = newScrollY;
        HomePageRelayout(win);
        HwndInvalidate(win->hwndCanvas);
    }
}

void HomePageOnMouseWheel(MainWindow* win, int delta) {
    int thumbsRowDy = HomePageIsListView() ? kHomeListRowDy : ThumbnailFrameSize().dy + kThumbsSpaceBetweenY;

    int scrollBy = thumbsRowDy / 3;
    if (delta > 0) {
        scrollBy = -scrollBy;
    }
    int newScrollY = win->homePageScrollY + scrollBy;
    newScrollY = std::max(newScrollY, 0);
    if (newScrollY != win->homePageScrollY) {
        win->homePageScrollY = newScrollY;
        HomePageRelayout(win);
        HwndInvalidate(win->hwndCanvas);
    }
}

// Dimensions can arrive after layout; rebuild affected windows on the next paint.
void HomePageThumbnailChanged(FileState* fs) {
    for (MainWindow* win : gWindows) {
        if (!win->homeLayout) {
            continue;
        }
        auto& c = *win->homeLayout;
        if (!c.valid) {
            continue;
        }
        for (ThumbnailLayout& t : c.thumbs) {
            if (t.fs != fs) {
                continue;
            }
            c.valid = false;
            if (win->IsCurrentTabAbout()) {
                HwndInvalidate(win->hwndCanvas);
            }
            break;
        }
    }
}
