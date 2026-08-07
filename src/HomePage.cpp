/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/ScopedWin.h"
#include "base/Dpi.h"
#include "base/File.h"
#include "base/Pixmap.h"
#include "base/Win.h"

#include "wingui/UIModels.h"
#include "wingui/Layout.h"
#include "wingui/WinGui.h"
#include "wingui/VirtWnd.h"

#include "Settings.h"
#include "DocController.h"
#include "SumatraConfig.h"
#include "FileHistory.h"
#include "GlobalPrefs.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "Commands.h"
#include "Accelerators.h"
#include "CommandPalette.h"
#include "FilterHighlightDraw.h"
#include "FileThumbnails.h"
#include "Menu.h"
#include "TipText.h"
#include "HomePage.h"
#include "Translations.h"
#include "Version.h"
#include "Theme.h"
#include "AppSettings.h"
#include "DarkModeSubclass.h"
#include "SvgIcons.h"

// how the shared tip code (TipText.cpp) opens a url link
static void OpenTipUrl(Str url) {
    // documentation links open in the embedded manual browser
    if (!MaybeLaunchDocumentation(url)) {
        SumatraLaunchBrowser(url);
    }
}

struct TipUrlHookInstaller {
    TipUrlHookInstaller() { gTipOpenUrl = OpenTipUrl; }
};
static TipUrlHookInstaller gTipUrlHookInstaller;

#ifndef ABOUT_USE_LESS_COLORS
#define ABOUT_LINE_OUTER_SIZE 2
#else
#define ABOUT_LINE_OUTER_SIZE 1
#endif
#define ABOUT_LINE_SEP_SIZE 1

static Str sumatraTips = StrL(R"tips(You can [customize scrollbar](CmdChangeScrollbar).
You can [customize keyboard shortcuts](Help/Customize-keyboard-shortcuts).
You can [customize toolbar](Help/Customize-toolbar).
Press (Key/CmdCommandPalette) to open [command palette](CmdCommandPalette).
To open file from history open [command palette](CmdCommandPalette) with (Key/CmdCommandPalette) and type `#`.
You can [extract text from PDF file](Help/Tool-x-extract-text-from-pdf).
You can [toggle menu bar](CmdToggleMenuBar) with (Key/CmdToggleMenuBar).
You can [toggle toolbar](CmdToggleToolbar) with (Key/CmdToggleToolbar).
You can [edit PDF annotations](Help/Editing-annotations).
You can preview where a citation, figure or footnote link points by hovering it - enable in [advanced settings](CmdAdvancedSettings) via CitationHoverDelay.
)tips");

static Str sumatraPromos = StrL(R"promos(Try [Edna](https://edna.arslexis.io): a note taking web app for power users.
Try [MarkLexis](https://marklexis.arslexis.io): a bookmarking web application.
)promos");

static Str promoFromServer;

// must fit all non-empty lines in sumatraTips / sumatraPromos
constexpr int kMaxHomeTips = 16;
constexpr int kMaxHomePromos = 8;

static ParsedTip gParsedTipsStorage[kMaxHomeTips];
static int gParsedTipCount = 0;
static ParsedTip gParsedPromosStorage[kMaxHomePromos];
static int gParsedPromoCount = 0;
static bool gTipsParsed = false;
static bool gSelectedIsPromo = false;
static int gSelectedTipIdx = -1;

static void ResetHomeCloseBtn();

static int ParseTipsFromString(Str src, Str prefix, ParsedTip* buffer, int bufferCap) {
    StrVec lines;
    Split(&lines, src, "\n");
    int n = 0;
    for (int i = 0; i < len(lines); i++) {
        Str line = lines[i];
        if (!str::IsEmptyOrWhiteSpace(line)) {
            n++;
        }
    }
    if (n == 0) {
        return 0;
    }
    ReportIf(n > bufferCap);
    int count = 0;
    for (int i = 0; i < len(lines); i++) {
        Str line = lines[i];
        if (str::IsEmptyOrWhiteSpace(line)) {
            continue;
        }
        if (prefix) {
            TempStr prefixed = str::JoinTemp(prefix, line);
            ParseTip(buffer[count], prefixed);
        } else {
            ParseTip(buffer[count], line);
        }
        count++;
    }
    return count;
}

static void PickRandomTipOrPromo() {
    bool pickPromo = (gParsedPromoCount > 0) && (rand() % 100 < 30);
    if (pickPromo) {
        gSelectedIsPromo = true;
        gSelectedTipIdx = rand() % gParsedPromoCount;
    } else if (gParsedTipCount > 0) {
        gSelectedIsPromo = false;
        gSelectedTipIdx = rand() % gParsedTipCount;
    }
}

static void EnsureTipsParsed() {
    if (gTipsParsed) {
        return;
    }
    gParsedTipCount = ParseTipsFromString(sumatraTips, "Tip: ", gParsedTipsStorage, kMaxHomeTips);
    gParsedPromoCount = ParseTipsFromString(sumatraPromos, {}, gParsedPromosStorage, kMaxHomePromos);
    gTipsParsed = true;
    PickRandomTipOrPromo();
}

static void ClearHomeLayoutCache();

void FreeHomePageTips() {
    if (gTipsParsed) {
        for (int i = 0; i < gParsedTipCount; i++) {
            gParsedTipsStorage[i].Reset();
        }
        for (int i = 0; i < gParsedPromoCount; i++) {
            gParsedPromosStorage[i].Reset();
        }
        gParsedTipCount = 0;
        gParsedPromoCount = 0;
        gTipsParsed = false;
    }
    str::Free(promoFromServer);
    ResetHomeCloseBtn();
    ClearHomeLayoutCache();
}

static void PickAnotherRandomTip() {
    bool prevIsPromo = gSelectedIsPromo;
    int prev = gSelectedTipIdx;
    // keep picking until we get a different one
    int maxIter = 100;
    while (maxIter-- > 0) {
        PickRandomTipOrPromo();
        if (gSelectedIsPromo != prevIsPromo || gSelectedTipIdx != prev) {
            return;
        }
    }
}

constexpr COLORREF kAboutBorderCol = RGB(0, 0, 0);

constexpr int kAboutLeftRightSpaceDx = 8;
constexpr int kAboutMarginDx = 10;
constexpr int kAboutBoxMarginDy = 6;
constexpr int kAboutTxtDy = 6;
constexpr int kAboutRectPadding = 8;

constexpr int kInnerPadding = 8;

static const Str kSumatraTxtFont = StrL("Arial Black");
constexpr int kSumatraTxtFontSize = 24;

static const Str kVersionTxtFont = StrL("Arial Black");
constexpr int kVersionTxtFontSize = 12;

#define LAYOUT_LTR 0

static ATOM gAtomAbout;
static HWND gHwndAbout;
static Tooltip* gAboutTooltip = nullptr;
static Str gClickedURL;

struct AboutLayoutInfoEl {
    /* static data, must be provided */
    Str leftTxt;
    Str rightTxt;
    Str url;

    /* data calculated by the layout */
    Rect leftPos;
    Rect rightPos;
};

static AboutLayoutInfoEl gAboutLayoutInfo[] = {
    {"build", "Built: " __DATE__ " " __TIME__, nullptr},
    {"website", "SumatraPDF website", kWebsiteURL},
    {"manual", "SumatraPDF manual", kManualURL},
    {"forums", "SumatraPDF forums", "https://github.com/sumatrapdfreader/sumatrapdf/discussions"},
    {"programming", "The Programmers", "https://github.com/sumatrapdfreader/sumatrapdf/blob/master/AUTHORS"},
    {"licenses", "Various Open Source", "https://github.com/sumatrapdfreader/sumatrapdf/blob/master/AUTHORS"},
#if defined(GIT_COMMIT_ID_STR)
    {"last change", "git commit " GIT_COMMIT_ID_STR,
     "https://github.com/sumatrapdfreader/sumatrapdf/commit/" GIT_COMMIT_ID_STR},
#endif
#if defined(PRE_RELEASE_VER)
    {"a note", "Pre-release version, for testing only!", nullptr},
#endif
#ifdef DEBUG
    {"a note", "Debug version, for testing only!", nullptr},
#endif
    {nullptr, nullptr, nullptr}};

static Vec<StaticLink*> gStaticLinks;

void SetPromoString(Str s) {
    if (!s) return;
    str::ReplaceWithCopy(&promoFromServer, s);
}

static TempStr GetAppVersionTemp() {
    TempStr s = str::DupTemp("v" CURR_VERSION_STRA);
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

constexpr COLORREF kCol1 = RGB(196, 64, 50);
constexpr COLORREF kCol2 = RGB(227, 107, 35);
constexpr COLORREF kCol3 = RGB(93, 160, 40);
constexpr COLORREF kCol4 = RGB(69, 132, 190);
constexpr COLORREF kCol5 = RGB(112, 115, 207);

static void DrawSumatraVersion(HDC hdc, Rect rect) {
    uint fmt = DT_LEFT | DT_NOCLIP;
    HFONT fontSumatraTxt = HdcCreateSimpleFont(hdc, kSumatraTxtFont, kSumatraTxtFontSize);
    HFONT fontVersionTxt = HdcCreateSimpleFont(hdc, kVersionTxtFont, kVersionTxtFontSize);

    SetBkMode(hdc, TRANSPARENT);

    Str txt = kAppName;
    Size txtSize = HdcMeasureText(hdc, txt, fmt, fontSumatraTxt);
    Rect mainRect(rect.x + ((rect.dx - txtSize.dx) / 2), rect.y + ((rect.dy - txtSize.dy) / 2), txtSize.dx, txtSize.dy);

    // draw SumatraPDF in colorful way
    Point pt = mainRect.TL();
    // colorful version
    static COLORREF cols[] = {kCol1, kCol2, kCol3, kCol4, kCol5, kCol5, kCol4, kCol3, kCol2, kCol1};
    char buf[2] = {};
    for (int i = 0; i < len(kAppName); i++) {
        SetTextColor(hdc, cols[i % dimofi(cols)]);
        buf[0] = kAppName[i];
        HdcDrawText(hdc, buf, pt, fmt, fontSumatraTxt);
        txtSize = HdcMeasureText(hdc, buf, fmt, fontSumatraTxt);
        pt.x += txtSize.dx;
    }

    SetTextColor(hdc, ThemeWindowTextColor());
    int x = mainRect.x + mainRect.dx + DpiScale(hdc, kInnerPadding);
    int y = mainRect.y;

    TempStr ver = GetAppVersionTemp();
    Point p = {x, y};
    HdcDrawText(hdc, ver, p, fmt, fontVersionTxt);
    p.y += DpiScale(hdc, 13);
    if (gIsPreReleaseBuild) {
        HdcDrawText(hdc, "Pre-release", p, fmt);
    }
}

// draw on the bottom right
static Rect DrawHideFrequentlyReadLink(HWND hwnd, HDC hdc, Str txt) {
    HFONT fontLeftTxt = HdcCreateSimpleFont(hdc, "MS Shell Dlg", 16);

    VirtWndText w(hwnd, txt, fontLeftTxt);
    w.isRtl = IsUIRtl();
    w.withUnderline = true;
    Size txtSize = w.GetIdealSize(true);

    auto col = ThemeWindowLinkColor();
    ScopedSelectObject pen(hdc, CreatePen(PS_SOLID, 1, col), true);

    SetTextColor(hdc, col);
    SetBkMode(hdc, TRANSPARENT);
    Rect rc = HwndClientRect(hwnd);

    int innerPadding = DpiScale(hwnd, kInnerPadding);
    Rect r = {0, 0, txtSize.dx, txtSize.dy};
    PositionRB(rc, r);
    MoveXY(r, -innerPadding, -innerPadding);
    w.SetBounds(r);
    w.Paint(hdc);

    // make the click target larger
    r.Inflate(innerPadding, innerPadding);
    return r;
}

static Size CalcSumatraVersionSize(HDC hdc) {
    HFONT fontSumatraTxt = HdcCreateSimpleFont(hdc, kSumatraTxtFont, kSumatraTxtFontSize);
    HFONT fontVersionTxt = HdcCreateSimpleFont(hdc, kVersionTxtFont, kVersionTxtFontSize);

    /* calculate minimal top box size */
    Size sz = HdcMeasureText(hdc, kAppName, fontSumatraTxt);
    sz.dy = sz.dy + DpiScale(hdc, kAboutBoxMarginDy * 2);

    /* consider version and version-sub strings */
    TempStr ver = GetAppVersionTemp();
    Size txtSize = HdcMeasureText(hdc, ver, fontVersionTxt);
    int minWidth = txtSize.dx + DpiScale(hdc, 8);
    int dx = std::max(txtSize.dx, minWidth);
    sz.dx += 2 * (dx + DpiScale(hdc, kInnerPadding));
    return sz;
}

static TempStr TrimGitTemp(Str s) {
    if (gitCommidId && str::EndsWith(s, gitCommidId)) {
        int sLen = len(s);
        int gitLen = len(gitCommidId);
        return str::DupTemp(Str(s.s, sLen - gitLen - 7));
    }
    return s;
}

/* Draws the about screen and remembers some state for hyperlinking.
   It transcribes the design I did in graphics software - hopeless
   to understand without seeing the design. */
static void DrawAbout(HWND hwnd, HDC hdc, Rect rect, Vec<StaticLink*>& staticLinks) {
    auto col = ThemeWindowTextColor();
    AutoDeletePen penBorder(CreatePen(PS_SOLID, ABOUT_LINE_OUTER_SIZE, col));
    AutoDeletePen penDivideLine(CreatePen(PS_SOLID, ABOUT_LINE_SEP_SIZE, col));
    col = ThemeWindowLinkColor();
    AutoDeletePen penLinkLine(CreatePen(PS_SOLID, ABOUT_LINE_SEP_SIZE, col));

    HFONT fontLeftTxt = HdcCreateSimpleFont(hdc, kLeftTextFont, kLeftTextFontSize);
    HFONT fontRightTxt = HdcCreateSimpleFont(hdc, kRightTextFont, kRightTextFontSize);

    ScopedSelectObject font(hdc, fontLeftTxt); /* Just to remember the orig font */

    Rect rc = HwndClientRect(hwnd);
    col = ThemeMainWindowBackgroundColor();
    AutoDeleteBrush brushAboutBg = CreateSolidBrush(col);
    HdcFillRect(hdc, rc, brushAboutBg);

    /* render title */
    Rect titleRect(rect.TL(), CalcSumatraVersionSize(hdc));

    ScopedSelectObject brush(hdc, CreateSolidBrush(col), true);
    ScopedSelectObject pen(hdc, penBorder);
#ifndef ABOUT_USE_LESS_COLORS
    Rectangle(hdc, rect.x, rect.y + ABOUT_LINE_OUTER_SIZE, rect.x + rect.dx,
              rect.y + titleRect.dy + ABOUT_LINE_OUTER_SIZE);
#else
    Rect titleBgBand(0, rect.y, rc.dx, titleRect.dy);
    RECT rcLogoBg = titleBgBand.ToRECT();
    HdcFillRect(hdc, ToRect(rcLogoBg), bgBrush);
    HdcDrawLine(hdc, Rect(0, rect.y, rc.dx, 0));
    HdcDrawLine(hdc, Rect(0, rect.y + titleRect.dy, rc.dx, 0));
#endif

    titleRect.Offset((rect.dx - titleRect.dx) / 2, 0);
    DrawSumatraVersion(hdc, titleRect);

    /* render attribution box */
    col = ThemeWindowTextColor();
    SetTextColor(hdc, col);
    SetBkMode(hdc, TRANSPARENT);

#ifndef ABOUT_USE_LESS_COLORS
    Rectangle(hdc, rect.x, rect.y + titleRect.dy, rect.x + rect.dx, rect.y + rect.dy);
#endif

    /* render text on the left*/
    SelectObject(hdc, fontLeftTxt);
    uint fmt = DT_LEFT | DT_NOCLIP;
    for (AboutLayoutInfoEl* el = gAboutLayoutInfo; el->leftTxt; el++) {
        auto& pos = el->leftPos;
        HdcDrawText(hdc, el->leftTxt, pos, fmt);
    }

    /* render text on the right */
    SelectObject(hdc, fontRightTxt);
    SelectObject(hdc, penLinkLine);
    DeleteVecMembers(staticLinks);
    for (AboutLayoutInfoEl* el = gAboutLayoutInfo; el->leftTxt; el++) {
        bool hasUrl = CanAccessDisk() && el->url;
        if (hasUrl) {
            col = ThemeWindowLinkColor();
        } else {
            col = ThemeWindowTextColor();
        }
        SetTextColor(hdc, col);
        TempStr s = TrimGitTemp(el->rightTxt);
        auto& pos = el->rightPos;
        HdcDrawText(hdc, s, pos, fmt);

        if (hasUrl) {
            int underlineY = pos.y + pos.dy - 3;
            HdcDrawLine(hdc, Rect(pos.x, underlineY, pos.dx, 0));
            auto* sl = new StaticLink(pos, el->url, el->url);
            staticLinks.Append(sl);
        }
    }

    SelectObject(hdc, penDivideLine);
    Rect divideLine(gAboutLayoutInfo[0].rightPos.x - DpiScale(hwnd, kAboutLeftRightSpaceDx), rect.y + titleRect.dy + 4,
                    0, rect.y + rect.dy - 4 - gAboutLayoutInfo[0].rightPos.y);
    HdcDrawLine(hdc, divideLine);
}

static void UpdateAboutLayoutInfo(HWND hwnd, HDC hdc, Rect* rect) {
    HFONT fontLeftTxt = HdcCreateSimpleFont(hdc, kLeftTextFont, kLeftTextFontSize);
    HFONT fontRightTxt = HdcCreateSimpleFont(hdc, kRightTextFont, kRightTextFontSize);

    /* calculate minimal top box size */
    Size headerSize = CalcSumatraVersionSize(hdc);

    /* calculate left text dimensions */
    int leftLargestDx = 0;
    int leftDy = 0;
    uint fmt = DT_LEFT;
    for (AboutLayoutInfoEl* el = gAboutLayoutInfo; el->leftTxt; el++) {
        Size txtSize = HdcMeasureText(hdc, el->leftTxt, fmt, fontLeftTxt);
        el->leftPos.dx = txtSize.dx;
        el->leftPos.dy = txtSize.dy;

        if (el == &gAboutLayoutInfo[0]) {
            leftDy = el->leftPos.dy;
        } else {
            ReportIf(leftDy != el->leftPos.dy);
        }
        leftLargestDx = std::max(leftLargestDx, el->leftPos.dx);
    }

    /* calculate right text dimensions */
    int rightLargestDx = 0;
    int rightDy = 0;
    for (AboutLayoutInfoEl* el = gAboutLayoutInfo; el->leftTxt; el++) {
        TempStr s = TrimGitTemp(el->rightTxt);
        Size txtSize = HdcMeasureText(hdc, s, fmt, fontRightTxt);
        el->rightPos.dx = txtSize.dx;
        el->rightPos.dy = txtSize.dy;

        if (el == &gAboutLayoutInfo[0]) {
            rightDy = el->rightPos.dy;
        } else {
            ReportIf(rightDy != el->rightPos.dy);
        }
        rightLargestDx = std::max(rightLargestDx, el->rightPos.dx);
    }

    int leftRightSpaceDx = DpiScale(hwnd, kAboutLeftRightSpaceDx);
    int marginDx = DpiScale(hwnd, kAboutMarginDx);
    int aboutTxtDy = DpiScale(hwnd, kAboutTxtDy);
    /* calculate total dimension and position */
    Rect minRect;
    minRect.dx = leftRightSpaceDx + leftLargestDx + ABOUT_LINE_SEP_SIZE + rightLargestDx + leftRightSpaceDx;
    minRect.dx = std::max(minRect.dx, headerSize.dx);
    minRect.dx += (2 * ABOUT_LINE_OUTER_SIZE) + (2 * marginDx);

    minRect.dy = headerSize.dy;
    for (AboutLayoutInfoEl* el = gAboutLayoutInfo; el->leftTxt; el++) {
        minRect.dy += rightDy + aboutTxtDy;
    }
    minRect.dy += (2 * ABOUT_LINE_OUTER_SIZE) + 4;

    Rect rc = HwndClientRect(hwnd);
    minRect.x = (rc.dx - minRect.dx) / 2;
    minRect.y = (rc.dy - minRect.dy) / 2;

    if (rect) {
        *rect = minRect;
    }

    /* calculate text positions */
    int linePosX = ABOUT_LINE_OUTER_SIZE + marginDx + leftLargestDx + leftRightSpaceDx;
    int currY = minRect.y + headerSize.dy + 4;
    for (AboutLayoutInfoEl* el = gAboutLayoutInfo; el->leftTxt; el++) {
        el->leftPos.x = minRect.x + linePosX - leftRightSpaceDx - el->leftPos.dx;
        el->leftPos.y = currY + ((rightDy - leftDy) / 2);
        el->rightPos.x = minRect.x + linePosX + leftRightSpaceDx;
        el->rightPos.y = currY;
        currY += rightDy + aboutTxtDy;
    }
}

static void OnPaintAbout(HWND hwnd) {
    PAINTSTRUCT ps;
    Rect rc;
    HDC hdc = BeginPaint(hwnd, &ps);
    SetLayout(hdc, LAYOUT_LTR);
    UpdateAboutLayoutInfo(hwnd, hdc, &rc);
    DrawAbout(hwnd, hdc, rc, gStaticLinks);
    EndPaint(hwnd, &ps);
}

static void CopyAboutInfoToClipboard() {
    str::Builder info(512);
    TempStr ver = GetAppVersionTemp();
    info.Append(fmt("%s %s\r\n", Str(kAppName), ver));
    for (int i = len(info) - 2; i > 0; i--) {
        info.AppendChar('-');
    }
    info.Append("\r\n");
    // concatenate all the information into a single string
    // (cf. CopyPropertiesToClipboard in SumatraProperties.cpp)
    int maxLen = 0;
    for (AboutLayoutInfoEl* el = gAboutLayoutInfo; el->leftTxt; el++) {
        maxLen = std::max(maxLen, len(el->leftTxt));
    }
    for (AboutLayoutInfoEl* el = gAboutLayoutInfo; el->leftTxt; el++) {
        for (int i = maxLen - len(el->leftTxt); i > 0; i--) {
            info.AppendChar(' ');
        }
        info.Append(fmt("%s: %s\r\n", el->leftTxt, el->url ? el->url.s : el->rightTxt));
    }
    CopyTextToClipboard(ToStr(info));
}

TempStr GetStaticLinkAtTemp(Vec<StaticLink*>& linkInfo, int x, int y, StaticLink** info) {
    if (!CanAccessDisk()) {
        return {};
    }

    Point pt(x, y);
    for (int i = 0; i < len(linkInfo); i++) {
        if (linkInfo[i]->rect.Contains(pt)) {
            auto* link = linkInfo[i];
            if (info) {
                *info = link;
            }
            return str::DupTemp(link->target);
        }
    }

    return {};
}

static void CreateInfotipForLink(StaticLink* linkInfo) {
    if (gAboutTooltip != nullptr) {
        return;
    }

    Tooltip::CreateArgs args;
    args.parent = gHwndAbout;
    args.font = GetAppFont(gHwndAbout);
    args.isRtl = IsUIRtl();

    gAboutTooltip = new Tooltip();
    gAboutTooltip->Create(args);
    gAboutTooltip->SetSingle(linkInfo->tooltip, linkInfo->rect, false);
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
    TempStr url;
    Point pt;

    int x = GET_X_LPARAM(lp);
    int y = GET_Y_LPARAM(lp);
    switch (msg) {
        case WM_CREATE:
            ReportIf(gHwndAbout);
            if (UseDarkModeLib()) {
                DarkMode::setDarkTitleBarEx(hwnd, true);
            }
            break;

        case WM_ERASEBKGND:
            // do nothing, helps to avoid flicker
            return TRUE;

        case WM_PAINT:
            OnPaintAbout(hwnd);
            break;

        case WM_SETCURSOR:
            pt = HwndGetCursorPos(hwnd);
            if (!pt.IsEmpty()) {
                StaticLink* linkInfo;
                if (GetStaticLinkAtTemp(gStaticLinks, pt.x, pt.y, &linkInfo)) {
                    CreateInfotipForLink(linkInfo);
                    SetCursorCached(IDC_HAND);
                    return TRUE;
                }
            }
            DeleteInfotip();
            return DefWindowProc(hwnd, msg, wp, lp);

        case WM_LBUTTONDOWN: {
            url = GetStaticLinkAtTemp(gStaticLinks, x, y, nullptr);
            str::ReplaceWithCopy(&gClickedURL, url);
        } break;

        case WM_LBUTTONUP:
            url = GetStaticLinkAtTemp(gStaticLinks, x, y, nullptr);
            if (url && str::Eq(url, gClickedURL)) {
                SumatraLaunchBrowser(url);
            }
            break;

        case WM_CHAR:
            if (VK_ESCAPE == wp) {
                DestroyWindow(hwnd);
            }
            break;

        case WM_COMMAND:
            if (CmdCopySelection == LOWORD(wp)) {
                CopyAboutInfoToClipboard();
            }
            break;

        case WM_DESTROY:
            DeleteInfotip();
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

    WCHAR* title = CWStrTemp(_TRA("About SumatraPDF"));
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

    // get the dimensions required for the about box's content
    Rect rc;
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(gHwndAbout, &ps);
    SetLayout(hdc, LAYOUT_LTR);
    UpdateAboutLayoutInfo(gHwndAbout, hdc, &rc);
    EndPaint(gHwndAbout, &ps);
    int rectPadding = DpiScale(gHwndAbout, kAboutRectPadding);
    rc.Inflate(rectPadding, rectPadding);

    // resize the new window to just match these dimensions
    Rect wRc = HwndWindowRect(gHwndAbout);
    Rect cRc = HwndClientRect(gHwndAbout);
    wRc.dx += rc.dx - cRc.dx;
    wRc.dy += rc.dy - cRc.dy;
    MoveWindow(gHwndAbout, wRc.x, wRc.y, wRc.dx, wRc.dy, FALSE);

    HwndPositionInCenterOf(gHwndAbout, win->hwndFrame);
    ShowWindow(gHwndAbout, SW_SHOW);
}

void DrawAboutPage(MainWindow* win, HDC hdc) {
    Rect rc = HwndClientRect(win->hwndCanvas);
    UpdateAboutLayoutInfo(win->hwndCanvas, hdc, &rc);
    DrawAbout(win->hwndCanvas, hdc, rc, win->staticLinks);
    if (HasPermission(Perm::SavePreferences | Perm::DiskAccess) && SettingsRememberOpenedFiles()) {
        Rect rect = DrawHideFrequentlyReadLink(win->hwndCanvas, hdc, _TRA("Show frequently read"));
        auto* sl = new StaticLink(rect, kLinkShowList);
        win->staticLinks.Append(sl);
    }
}

/* alternate static page to display when no document is loaded */

constexpr int kThumbsSeparatorDy = 2;
constexpr int kThumbsBorderDx = 1;
#define kThumbsMarginLeft DpiScale(hdc, 40)
#define kThumbsMarginRight DpiScale(hdc, 40)
#define kThumbsMarginTop DpiScale(hdc, 50)
#define kThumbsMarginBottom DpiScale(hdc, 40)
#define kThumbsSpaceBetweenX DpiScale(hdc, 38)
#define kThumbsSpaceBetweenY DpiScale(hdc, 58)
#define kThumbsBottomBoxDy DpiScale(hdc, 50)
#define kHomeListThumbDx DpiScale(hdc, 30)
#define kHomeListThumbDy DpiScale(hdc, 40)
#define kHomeListRowDy DpiScale(hdc, 46)
#define kHomeListRowGapDx DpiScale(hdc, 8)

// ThumbnailLayout::fileSize cache: AppendBlanks zero-fills, so set
// kSizeNotFetched after each AppendBlanks (default member init never runs).
constexpr i64 kSizeNotFetched = -2;
constexpr i64 kSizeFetchFail = -1;

struct ThumbnailLayout {
    Rect rcPage;
    Size szThumb;
    Rect rcText;
    Rect rcListRow;
    Rect rcListThumb;
    Rect rcListFileName;
    Rect rcListPath;
    Rect rcListSize;
    Rect rcListRemove;
    Rect rcListPin;
    FileState* fs = nullptr; // info needed to draw the thumbnail
    StaticLink* sl = nullptr;
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

bool HomePageIsListView() {
    return gGlobalPrefs && str::EqI(gGlobalPrefs->homePageViewMode, StrL("list"));
}

void SetHomePageListView(bool listView) {
    Str mode = listView ? StrL("list") : StrL("thumbnails");
    str::ReplaceWithCopy(&gGlobalPrefs->homePageViewMode, mode);
}

struct HomePageLayout {
    // args in
    HWND hwnd = nullptr;
    HDC hdc = nullptr;
    Rect rc;
    MainWindow* win = nullptr;

    Rect rcAppWithVer; // SumatraPDF colorful text + version
    Rect rcLine;       // line under bApp
    Rect rcIconOpen;
    Rect rcIconListView;
    Rect rcIconThumbnailView;

    HIMAGELIST himlOpen = nullptr;
    VirtWndText* freqRead = nullptr;
    VirtWndText* openDoc = nullptr;
    VirtWndText* hideShowFreqRead = nullptr;
    Vec<ThumbnailLayout> thumbnails; // info for each thumbnail
    int totalContentDy = 0;          // total height of all thumbnail rows
    int thumbsVisibleDy = 0;         // visible height for thumbnails area
    Rect rcThumbsArea;               // clip rect for thumbnails

    // search filter
    StrVec filterWords;
    Vec<u8> highlighted;
    Rect rcSearchBorder; // border rect drawn around the edit control

    // tip layout
    Rect rcTip;               // background rect for tip area
    ParsedTip* tip = nullptr; // points into gParsedTipsStorage or gParsedPromosStorage, not owned

    ~HomePageLayout();
};

HomePageLayout::~HomePageLayout() {
    delete freqRead;
    delete openDoc;
}

constexpr int kOpenDocumentYShift = 7;
constexpr int kThumbsMiddleMargin = 32;
constexpr int kSearchEditDy = 28;
constexpr int kHeaderSearchGapY = 12;
constexpr int kSearchThumbnailsGapY = 12;

static WNDPROC DefWndProcHomeSearch = nullptr;

static void HomeSelectFromSearchReturnCol(MainWindow* win);
static void HomePageShowSelectionTooltip(MainWindow* win);

static LRESULT CALLBACK WndProcHomeSearch(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_KEYDOWN && wp == VK_DOWN) {
        // down from the search box moves into the file list (issue #1136),
        // restoring the column we left from when going up
        MainWindow* win = FindMainWindowByHwnd(GetParent(hwnd));
        if (win) {
            HomeSelectFromSearchReturnCol(win);
            HwndSetFocus(win->hwndCanvas);
            HwndInvalidate(win->hwndCanvas);
            HomePageShowSelectionTooltip(win);
        }
        return 0;
    }
    if (msg == WM_KEYDOWN && wp == VK_ESCAPE) {
        HwndSetText(hwnd, "");
        MainWindow* win = FindMainWindowByHwnd(GetParent(hwnd));
        if (win) {
            HwndSetFocus(win->hwndCanvas);
            win->RedrawAll(true);
        }
        return 0;
    }
    if (msg == WM_MOUSEWHEEL) {
        HWND parent = GetParent(hwnd);
        return SendMessageW(parent, msg, wp, lp);
    }
    return CallWindowProcW(DefWndProcHomeSearch, hwnd, msg, wp, lp);
}

// Home-list entries with a path (same set as thumbnails when search is empty).
static int CountHomePageFiles() {
    Vec<FileState*> all;
    if (gGlobalPrefs && gGlobalPrefs->homePageSortByFrequentlyRead) {
        gFileHistory.GetFrequencyOrder(all);
    } else {
        gFileHistory.GetRecentlyOpenedOrder(all);
    }
    int n = 0;
    for (FileState* fs : all) {
        if (fs && len(fs->filePath) > 0) {
            n++;
        }
    }
    return n;
}

// Cue banner when the search field is empty: "Search N files (Ctrl + F)".
static void UpdateHomeSearchCueBanner(MainWindow* win) {
    if (!win || !win->hwndHomeSearch) {
        return;
    }
    // _TRA returns Str; pass .s into type-safe fmt for the format string.
    TempStr cue = fmt(_TRA("Search %d files (Ctrl + F)").s, CountHomePageFiles());
    Edit_SetCueBannerText(win->hwndHomeSearch, CWStrTemp(cue));
}

static void EnsureHomeSearchCreated(MainWindow* win) {
    if (win->hwndHomeSearch) {
        UpdateHomeSearchCueBanner(win);
        return;
    }
    HMODULE hmod = GetModuleHandleW(nullptr);
    DWORD style = WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL;
    DWORD exStyle = 0;
    win->hwndHomeSearch = CreateWindowExW(exStyle, WC_EDITW, L"", style, 0, 0, 100, kSearchEditDy, win->hwndCanvas,
                                          nullptr, hmod, nullptr);
    HDC hdc = GetDC(win->hwndCanvas);
    HFONT font = HdcCreateSimpleFont(hdc, "MS Shell Dlg", 14);
    ReleaseDC(win->hwndCanvas, hdc);
    SetWindowFont(win->hwndHomeSearch, font, TRUE);
    if (!DefWndProcHomeSearch) {
        DefWndProcHomeSearch = (WNDPROC)GetWindowLongPtr(win->hwndHomeSearch, GWLP_WNDPROC);
    }
    SetWindowLongPtr(win->hwndHomeSearch, GWLP_WNDPROC, (LONG_PTR)WndProcHomeSearch);
    UpdateHomeSearchCueBanner(win);
    // add left/right padding so text doesn't overlap the border
    int margin = DpiScale(win->hwndCanvas, 6);
    SendMessage(win->hwndHomeSearch, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(margin, margin));
    // restore the query from before the edit control was destroyed
    // (e.g. by switching to a document tab and back)
    if (len(win->homeSearchQuery) > 0) {
        HwndSetText(win->hwndHomeSearch, win->homeSearchQuery);
    }
}

void HomePageDestroySearch(MainWindow* win) {
    if (win->hwndHomeSearch) {
        TempStr query = HwndGetTextTemp(win->hwndHomeSearch);
        str::ReplaceWithCopy(&win->homeSearchQuery, query);
        DestroyWindow(win->hwndHomeSearch);
        win->hwndHomeSearch = nullptr;
    }
}

void HomePageFocusSearch(MainWindow* win) {
    EnsureHomeSearchCreated(win);
    ShowWindow(win->hwndHomeSearch, SW_SHOW);
    HwndSetFocus(win->hwndHomeSearch);
}

void PickAnotherRandomPromotion() {
    PickAnotherRandomTip();
}

// Just the path here — no file::GetSize during layout/scroll, it's too slow on
// network drives (was ~11% of home-page scroll CPU). The size is appended when
// the tooltip is actually shown, see LinkTooltipTemp().
static TempStr HomeThumbTooltipTemp(Str path) {
    return str::DupTemp(path);
}

// A thumbnail / list-row tooltip is the file path, then two spaces and a
// human-readable size. Looking the size up on hover (rather than when the link
// is created) keeps it off the layout/scroll path, where file::GetSize() on a
// network drive is slow enough to be felt. Links whose target isn't a file
// (urls, commands) keep their plain tooltip.
TempStr LinkTooltipTemp(StaticLink* link) {
    Str tip = link->tooltip;
    if (!tip || !link->target) {
        return str::DupTemp(tip);
    }
    i64 size = file::GetSize(link->target);
    if (size < 0) {
        return str::DupTemp(tip);
    }
    return fmt("%s  %s", tip, str::FormatSizeShortTemp(size, nullptr));
}

// --- scroll-friendly layout cache: full LayoutHomePage only when content/size/
// filter changes; pure scrollY changes just offset stored thumb rects ---
struct HomePageLayoutCache {
    bool valid = false;
    Rect canvasRc;
    int scrollY = 0;
    int nFiles = 0;
    bool listView = false;
    bool sortByFreq = false;
    bool showTips = false;
    int tipIdx = -1;
    bool tipIsPromo = false;
    Str filterText; // owned

    Rect rcThumbsArea;
    Rect rcSearchBorder;
    Rect rcIconOpen;
    Rect rcIconListView;
    Rect rcIconThumbnailView;
    Rect rcTip;
    Rect rcAppWithVer;
    Rect rcLine;
    Rect rcFreqRead;
    Rect rcOpenDoc;
    int totalContentDy = 0;
    int thumbsVisibleDy = 0;
    ParsedTip* tip = nullptr;
    Vec<ThumbnailLayout> thumbs;
    StrVec filterWords;
};

static HomePageLayoutCache gHomeLayoutCache;

static void ClearHomeLayoutCache() {
    gHomeLayoutCache.valid = false;
    str::Free(gHomeLayoutCache.filterText);
    gHomeLayoutCache.filterText = {};
    gHomeLayoutCache.thumbs.Reset();
    gHomeLayoutCache.filterWords.Reset();
    gHomeLayoutCache.tip = nullptr;
    gHomeLayoutCache.nFiles = 0;
    gHomeLayoutCache.scrollY = 0;
}

// The cache holds raw FileState* (ThumbnailLayout::fs) owned by gGlobalPrefs.
// Reloading settings frees and rebuilds those, so the cache has to be dropped
// first or hover / selection reads freed memory (crash 8c34d7eda). It is
// rebuilt on the next paint.
void HomePageInvalidateLayoutCache() {
    ClearHomeLayoutCache();
}

static void OffsetThumbnailLayouts(Vec<ThumbnailLayout>& thumbs, int dy) {
    if (dy == 0) {
        return;
    }
    for (ThumbnailLayout& t : thumbs) {
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

// rebuild hit-test links for currently visible file rows/thumbs (scroll-safe)
static void HomePageAppendFileStaticLinks(HomePageLayout& l) {
    MainWindow* win = l.win;
    bool list = HomePageIsListView();
    for (ThumbnailLayout& thumb : l.thumbnails) {
        FileState* fs = thumb.fs;
        if (!fs || !fs->filePath) {
            continue;
        }
        Str path = fs->filePath;
        if (list) {
            Rect slRect = thumb.rcListRow.Intersect(l.rcThumbsArea);
            if (slRect.IsEmpty()) {
                continue;
            }
            TempStr removeTarget = str::JoinTemp(kLinkHomeRemoveFilePrefix, path);
            TempStr pinTarget = str::JoinTemp(kLinkHomePinFilePrefix, path);
            Str pinTip = fs->isPinned ? _TRA("Unpin") : _TRA("Pin");
            win->staticLinks.Append(new StaticLink(thumb.rcListRemove.Intersect(l.rcThumbsArea), removeTarget,
                                                   _TRA("Remove from Frequently Read")));
            win->staticLinks.Append(new StaticLink(thumb.rcListPin.Intersect(l.rcThumbsArea), pinTarget, pinTip));
            thumb.sl = new StaticLink(slRect, path, HomeThumbTooltipTemp(path));
            win->staticLinks.Append(thumb.sl);
        } else {
            Rect slRect = thumb.rcText.Union(thumb.rcPage).Intersect(l.rcThumbsArea);
            if (slRect.IsEmpty()) {
                continue;
            }
            thumb.sl = new StaticLink(slRect, path, HomeThumbTooltipTemp(path));
            win->staticLinks.Append(thumb.sl);
        }
    }
}

static void HomePageAppendChromeStaticLinks(HomePageLayout& l) {
    MainWindow* win = l.win;
    win->staticLinks.Append(new StaticLink(l.rcIconListView, kLinkHomeListView, _TRA("Show as list")));
    win->staticLinks.Append(new StaticLink(l.rcIconThumbnailView, kLinkHomeThumbnailView, _TRA("Show as thumbnails")));

    Rect rcOpen = l.rcIconOpen;
    if (l.openDoc) {
        rcOpen = rcOpen.Union(l.openDoc->lastBounds);
    }
    rcOpen.Inflate(10, 10);
    win->staticLinks.Append(new StaticLink(rcOpen, kLinkOpenFile));

    if (l.tip) {
        for (auto& link : l.tip->links) {
            Rect linkRect;
            for (int i = link.firstWord; i <= link.lastWord; i++) {
                auto& w = l.tip->words[i];
                Rect wr = {w.x, w.y, w.dx, w.dy};
                if (i == link.firstWord) {
                    linkRect = wr;
                } else {
                    linkRect = linkRect.Union(wr);
                }
            }
            win->staticLinks.Append(new StaticLink(linkRect, link.cmd, link.cmd));
        }
        win->staticLinks.Append(new StaticLink(l.rcTip, kLinkNextTip));
    }
}

static TempStr HomeSearchQueryTemp(MainWindow* win) {
    if (!win->hwndHomeSearch) {
        return {};
    }
    return HwndGetTextTemp(win->hwndHomeSearch);
}

static bool HomeLayoutCacheMatches(MainWindow* win, const Rect& rc, Str filterText) {
    auto& c = gHomeLayoutCache;
    if (!c.valid) {
        return false;
    }
    if (c.canvasRc != rc) {
        return false;
    }
    if (c.listView != HomePageIsListView()) {
        return false;
    }
    if (c.sortByFreq != (gGlobalPrefs && gGlobalPrefs->homePageSortByFrequentlyRead)) {
        return false;
    }
    if (c.showTips != (gGlobalPrefs && gGlobalPrefs->showTips)) {
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
static bool HomeLayoutCacheFilesMatch(const Vec<FileState*>& files) {
    auto& c = gHomeLayoutCache;
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
    if (gGlobalPrefs->homePageSortByFrequentlyRead) {
        gFileHistory.GetFrequencyOrder(allFileStates);
    } else {
        gFileHistory.GetRecentlyOpenedOrder(allFileStates);
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
        fileStates.Append(fs);
    }
}

static void SaveHomeLayoutCache(const HomePageLayout& l, Str filterText, int scrollY) {
    auto& c = gHomeLayoutCache;
    c.valid = true;
    c.canvasRc = l.rc;
    c.scrollY = scrollY;
    c.nFiles = len(l.thumbnails);
    c.listView = HomePageIsListView();
    c.sortByFreq = gGlobalPrefs && gGlobalPrefs->homePageSortByFrequentlyRead;
    c.showTips = gGlobalPrefs && gGlobalPrefs->showTips;
    c.tipIdx = gSelectedTipIdx;
    c.tipIsPromo = gSelectedIsPromo;
    str::ReplaceWithCopy(&c.filterText, filterText);
    c.rcThumbsArea = l.rcThumbsArea;
    c.rcSearchBorder = l.rcSearchBorder;
    c.rcIconOpen = l.rcIconOpen;
    c.rcIconListView = l.rcIconListView;
    c.rcIconThumbnailView = l.rcIconThumbnailView;
    c.rcTip = l.rcTip;
    c.rcAppWithVer = l.rcAppWithVer;
    c.rcLine = l.rcLine;
    c.rcFreqRead = l.freqRead ? l.freqRead->lastBounds : Rect{};
    c.rcOpenDoc = l.openDoc ? l.openDoc->lastBounds : Rect{};
    c.totalContentDy = l.totalContentDy;
    c.thumbsVisibleDy = l.thumbsVisibleDy;
    c.tip = l.tip;
    c.thumbs = l.thumbnails;
    for (ThumbnailLayout& t : c.thumbs) {
        t.sl = nullptr; // links are owned by win->staticLinks, recreated each paint
    }
    c.filterWords = l.filterWords;
}

// rebuild chrome VirtWndText + copy cached geometry into l (no full layout)
static void ApplyHomeLayoutCache(HomePageLayout& l, int scrollY) {
    auto& c = gHomeLayoutCache;
    auto* win = l.win;
    auto* hdc = l.hdc;
    auto* hwnd = l.hwnd;
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
    l.rcTip = c.rcTip;
    l.rcAppWithVer = c.rcAppWithVer;
    l.rcLine = c.rcLine;
    l.totalContentDy = c.totalContentDy;
    l.thumbsVisibleDy = c.thumbsVisibleDy;
    l.tip = c.tip;
    l.thumbnails = c.thumbs;
    l.filterWords = c.filterWords;

    l.himlOpen = TbGetImageList(win->hwndToolbar);

    HFONT hdrFont = HdcCreateSimpleFont(hdc, "MS Shell Dlg", 24);
    HFONT fontText = HdcCreateSimpleFont(hdc, "MS Shell Dlg", 14);

    Str txt = _TRA("Recently Opened");
    if (gGlobalPrefs->homePageSortByFrequentlyRead) {
        txt = _TRA("Frequently Read");
    }
    VirtWndText* hdr = new VirtWndText(hwnd, txt, hdrFont);
    hdr->isRtl = isRtl;
    hdr->SetBounds(c.rcFreqRead);
    l.freqRead = hdr;

    VirtWndText* openDoc = new VirtWndText(hwnd, _TRA("Open a document..."), fontText);
    openDoc->isRtl = isRtl;
    openDoc->withUnderline = true;
    openDoc->SetBounds(c.rcOpenDoc);
    l.openDoc = openDoc;

    HomePageAppendChromeStaticLinks(l);
    HomePageAppendFileStaticLinks(l);
}

// after paint, keep lazy fileSize values in the cache for the next frame
static void SyncHomeLayoutCacheFileSizes(const HomePageLayout& l) {
    auto& c = gHomeLayoutCache;
    if (!c.valid || len(c.thumbs) != len(l.thumbnails)) {
        return;
    }
    for (int i = 0; i < len(c.thumbs); i++) {
        c.thumbs[i].fileSize = l.thumbnails[i].fileSize;
        // geometry may have been filled for newly visible list rows
        c.thumbs[i].rcListFileName = l.thumbnails[i].rcListFileName;
        c.thumbs[i].rcListPath = l.thumbnails[i].rcListPath;
        c.thumbs[i].rcListSize = l.thumbnails[i].rcListSize;
        c.thumbs[i].listTextMeasured = l.thumbnails[i].listTextMeasured;
        c.thumbs[i].szThumb = l.thumbnails[i].szThumb;
    }
}

static void LayoutHomePage(HomePageLayout& l) {
    EnsureTipsParsed();

    Vec<FileState*> allFileStates;
    if (gGlobalPrefs->homePageSortByFrequentlyRead) {
        gFileHistory.GetFrequencyOrder(allFileStates);
    } else {
        gFileHistory.GetRecentlyOpenedOrder(allFileStates);
    }
    auto* hwnd = l.hwnd;
    auto* hdc = l.hdc;
    auto rc = l.rc;
    auto* win = l.win;

    // filter by search query if present
    TempStr searchQuery = nullptr;
    if (win->hwndHomeSearch) {
        searchQuery = HwndGetTextTemp(win->hwndHomeSearch);
    }
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
        fileStates.Append(fs);
    }

    bool isRtl = IsUIRtl();
    HFONT fontText = HdcCreateSimpleFont(hdc, "MS Shell Dlg", 14);
    HFONT hdrFont = HdcCreateSimpleFont(hdc, "MS Shell Dlg", 24);

    Size sz = CalcSumatraVersionSize(hdc);
    {
        Rect& r = l.rcAppWithVer;
        r.x = rc.dx - sz.dx - 3;
        r.y = 0;
        r.SetSize(sz);
    }

    l.rcLine = {0, sz.dy, rc.dx, 0};

    // --- Pre-compute thumbnail grid x offset so header can align with it ---
    // use unfiltered count so layout stays stable when search filters results
    int nFilesForLayout = len(allFileStates);
    int colsForLayout =
        (rc.dx - kThumbsMarginLeft - kThumbsMarginRight + kThumbsSpaceBetweenX) / (kThumbnailDx + kThumbsSpaceBetweenX);
    int thumbsColsForLayout = std::max(colsForLayout, 1);
    int thumbsStartX = rc.x + kThumbsMarginLeft +
                       ((rc.dx - (thumbsColsForLayout * kThumbnailDx) -
                         ((thumbsColsForLayout - 1) * kThumbsSpaceBetweenX) - kThumbsMarginLeft - kThumbsMarginRight) /
                        2);
    if (thumbsStartX < DpiScale(hdc, kInnerPadding)) {
        thumbsStartX = DpiScale(hdc, kInnerPadding);
    } else if (nFilesForLayout == 0) {
        thumbsStartX = kThumbsMarginLeft;
    }
    int thumbsContentWidth = (thumbsColsForLayout * kThumbnailDx) + ((thumbsColsForLayout - 1) * kThumbsSpaceBetweenX);

    // --- Step 1: layout header at the top ---
    l.himlOpen = TbGetImageList(win->hwndToolbar);
    Rect rcIconView(0, 0, 0, 0);
    ImageList_GetIconSize(l.himlOpen, &rcIconView.dx, &rcIconView.dy);

    Str txt = _TRA("Recently Opened");
    if (gGlobalPrefs->homePageSortByFrequentlyRead) {
        txt = _TRA("Frequently Read");
    }
    VirtWndText* hdr = new VirtWndText(hwnd, txt, hdrFont);
    l.freqRead = hdr;
    hdr->isRtl = isRtl;
    Size txtSize = hdr->GetIdealSize(true);

    int hdrY = DpiScale(hdc, 8);
    int iconGap = DpiScale(hdc, 4);
    int titleGap = DpiScale(hdc, 8);
    int viewIconsDx = (2 * rcIconView.dx) + iconGap;
    Rect rcHdr(thumbsStartX + viewIconsDx + titleGap, hdrY, txtSize.dx, txtSize.dy);
    l.rcIconThumbnailView = {thumbsStartX, rcHdr.y + ((rcHdr.dy - rcIconView.dy) / 2), rcIconView.dx, rcIconView.dy};
    l.rcIconListView = {l.rcIconThumbnailView.x + rcIconView.dx + iconGap, l.rcIconThumbnailView.y, rcIconView.dx,
                        rcIconView.dy};
    if (isRtl) {
        int groupDx = viewIconsDx + titleGap + rcHdr.dx;
        int groupX = rc.dx - thumbsStartX - groupDx;
        rcHdr.x = groupX;
        l.rcIconListView = {rcHdr.x + rcHdr.dx + titleGap, l.rcIconListView.y, rcIconView.dx, rcIconView.dy};
        l.rcIconThumbnailView = {l.rcIconListView.x + rcIconView.dx + iconGap, l.rcIconThumbnailView.y, rcIconView.dx,
                                 rcIconView.dy};
    }
    hdr->SetBounds(rcHdr);
    win->staticLinks.Append(new StaticLink(l.rcIconListView, kLinkHomeListView, _TRA("Show as list")));
    win->staticLinks.Append(new StaticLink(l.rcIconThumbnailView, kLinkHomeThumbnailView, _TRA("Show as thumbnails")));

    /* "Open a document" link next to header */
    Rect rcIconOpen(0, 0, 0, 0);
    ImageList_GetIconSize(l.himlOpen, &rcIconOpen.dx, &rcIconOpen.dy);

    txt = _TRA("Open a document...");
    auto* openDoc = new VirtWndText(hwnd, txt, fontText);
    openDoc->isRtl = isRtl;
    openDoc->withUnderline = true;
    txtSize = openDoc->GetIdealSize(true);

    int openDocSpacing = DpiScale(hdc, 16);
    rcIconOpen.x = rcHdr.x + rcHdr.dx + openDocSpacing;
    rcIconOpen.y = rcHdr.y + rcHdr.dy - rcIconOpen.dy - kOpenDocumentYShift + 3;
    if (isRtl) {
        rcIconOpen.x = rcHdr.x - openDocSpacing - rcIconOpen.dx;
    }
    l.rcIconOpen = rcIconOpen;

    Rect rcOpenDoc(rcIconOpen.x + rcIconOpen.dx + 3, rcHdr.y + rcHdr.dy - txtSize.dy - kOpenDocumentYShift, txtSize.dx,
                   txtSize.dy);
    if (isRtl) {
        rcOpenDoc.x = rcIconOpen.x - rcOpenDoc.dx - 3;
    }
    openDoc->SetBounds(rcOpenDoc);

    l.openDoc = openDoc;

    rcOpenDoc = rcOpenDoc.Union(rcIconOpen);
    rcOpenDoc.Inflate(10, 10);
    auto* sl = new StaticLink(rcOpenDoc, kLinkOpenFile);
    win->staticLinks.Append(sl);

    int headerBottomY = rcHdr.y + rcHdr.dy;

    // --- Position search edit below header ---
    EnsureHomeSearchCreated(win);
    int searchEditDy = DpiScale(hdc, kSearchEditDy);
    int headerSearchGap = DpiScale(hdc, kHeaderSearchGapY);
    int searchThumbsGap = DpiScale(hdc, kSearchThumbnailsGapY);
    {
        int borderDx = thumbsContentWidth * 3 / 4;
        borderDx = std::max(borderDx, DpiScale(hdc, 200));
        int borderX = thumbsStartX + ((thumbsContentWidth - borderDx) / 2);
        int borderY = headerBottomY + headerSearchGap;
        int borderDy = searchEditDy + 2; // 1px border on each side
        l.rcSearchBorder = {borderX, borderY, borderDx, borderDy};
        // measure font height so we can vertically center the edit
        HFONT editFont = (HFONT)SendMessage(win->hwndHomeSearch, WM_GETFONT, 0, 0);
        TEXTMETRIC tm;
        HFONT oldFont = (HFONT)SelectObject(hdc, editFont);
        GetTextMetrics(hdc, &tm);
        SelectObject(hdc, oldFont);
        int fontDy = tm.tmHeight + tm.tmExternalLeading + 2; // +2 for caret padding
        int editDy = std::min(fontDy, searchEditDy);
        int editY = borderY + 1 + ((searchEditDy - editDy) / 2);
        MoveWindow(win->hwndHomeSearch, borderX + 1, editY, borderDx - 2, editDy, TRUE);
    }
    // border is 1px top + 1px bottom = 2px
    int searchAreaDy = headerSearchGap + searchEditDy + 2 + searchThumbsGap;
    headerBottomY += searchAreaDy;

    // --- Step 2: calculate tip area at the bottom (before thumbnails) ---
    int tipHeight = 0;
    HFONT fontTip = HdcCreateSimpleFont(hdc, "MS Shell Dlg", 16);
    ParsedTip* tip = nullptr;
    if (gGlobalPrefs->showTips && gSelectedTipIdx >= 0) {
        if (gSelectedIsPromo && gSelectedTipIdx < gParsedPromoCount) {
            tip = &gParsedPromosStorage[gSelectedTipIdx];
        } else if (!gSelectedIsPromo && gSelectedTipIdx < gParsedTipCount) {
            tip = &gParsedTipsStorage[gSelectedTipIdx];
        }
    }
    if (tip) {
        MeasureTipWords(*tip, hdc, fontTip);
        int tipPadding = DpiScale(hdc, 8);
        // do a preliminary layout to get the height (use thumbnails content width)
        LayoutTip(*tip, thumbsContentWidth, 0, 0);
        tipHeight = tip->totalDy + (2 * tipPadding);
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
    int thumbsContentPadTop = showList ? DpiScale(hdc, 2) : DpiScale(hdc, 5);
    int thumbsRows = 0;
    int thumbsContentDy = 0;
    if (showList) {
        thumbsRows = nFiles;
        thumbsContentDy = nFiles * kHomeListRowDy;
    } else {
        thumbsRows = (nFiles + thumbsColsForLayout - 1) / thumbsColsForLayout;
        if (thumbsRows > 0) {
            thumbsContentDy = (thumbsRows * (kThumbnailDy + kThumbsSpaceBetweenY)) - kThumbsSpaceBetweenY;
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
        int listIconGap = DpiScale(hdc, 6);
        // fixed size column — never call file::GetSize during layout (disk/network I/O)
        int listSizeDx = DpiScale(hdc, 56);
        // one-row margin so a quick scroll still has measured name/path splits ready
        int listPrefetchY = kHomeListRowDy;
        for (int row = 0; row < nFiles; row++) {
            ThumbnailLayout& thumb = *l.thumbnails.AppendBlanks(1);
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
            Rect rcFileName(rcThumb.x + rcThumb.dx + kHomeListRowGapDx, rcRow.y,
                            rcSize.x - (rcThumb.x + rcThumb.dx + kHomeListRowGapDx) - kHomeListRowGapDx, rcRow.dy);
            if (isRtl) {
                rcThumb.x = rcRow.x + rcRow.dx - rcThumb.dx;
                rcPin.x = rcRow.x;
                rcRemove.x = rcPin.x + listIconDx + listIconGap;
                rcSize.x = rcRemove.x + listIconDx + listIconGap;
                rcFileName.x = rcSize.x + rcSize.dx + kHomeListRowGapDx;
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
            thumb.rcListFileName = rcFileName;
            // already-cached in-memory thumb size only (no LoadThumbnail / disk)
            if (onScreen && fs->thumbnail) {
                thumb.szThumb = Size(fs->thumbnail->width, fs->thumbnail->height);
            }
            if (!onScreen) {
                continue;
            }
            Str path = fs->filePath;
            Rect slRect = rcRow.Intersect(l.rcThumbsArea);
            if (!slRect.IsEmpty()) {
                TempStr removeTarget = str::JoinTemp(kLinkHomeRemoveFilePrefix, path);
                TempStr pinTarget = str::JoinTemp(kLinkHomePinFilePrefix, path);
                Str pinTip = fs->isPinned ? _TRA("Unpin") : _TRA("Pin");
                win->staticLinks.Append(new StaticLink(rcRemove.Intersect(l.rcThumbsArea), removeTarget,
                                                       _TRA("Remove from Frequently Read")));
                win->staticLinks.Append(new StaticLink(rcPin.Intersect(l.rcThumbsArea), pinTarget, pinTip));
                thumb.sl = new StaticLink(slRect, path, HomeThumbTooltipTemp(path));
                win->staticLinks.Append(thumb.sl);
            }
        }
    } else {
        int thumbPrefetchY = kThumbnailDy + kThumbsSpaceBetweenY;
        for (int row = 0; row < thumbsRows; row++) {
            for (int col = 0; col < thumbsColsForLayout; col++) {
                if ((row * thumbsColsForLayout) + col >= nFiles) {
                    // no more files to display
                    thumbsRows = col > 0 ? row + 1 : row;
                    break;
                }
                ThumbnailLayout& thumb = *l.thumbnails.AppendBlanks(1);
                thumb.fileSize = kSizeNotFetched;
                FileState* fs = fileStates[(row * thumbsColsForLayout) + col];
                thumb.fs = fs;

                Rect rcPage(ptOff.x + (col * (kThumbnailDx + kThumbsSpaceBetweenX)),
                            ptOff.y + (row * (kThumbnailDy + kThumbsSpaceBetweenY)), kThumbnailDx, kThumbnailDy);
                if (isRtl) {
                    rcPage.x = rc.dx - rcPage.x - rcPage.dx;
                }
                bool onScreen = IsHomeThumbOnScreen(rcPage, l.rcThumbsArea, thumbPrefetchY);
                // only use already-resident thumbnails for aspect adjust — never LoadThumbnail
                // during layout (disk I/O dominated scroll/paint CPU)
                if (onScreen && fs->thumbnail) {
                    Size szThumb(fs->thumbnail->width, fs->thumbnail->height);
                    if (szThumb.dx != kThumbnailDx || szThumb.dy != kThumbnailDy) {
                        rcPage.dy = szThumb.dy * kThumbnailDx / szThumb.dx;
                        rcPage.y += kThumbnailDy - rcPage.dy;
                    }
                    thumb.szThumb = szThumb;
                }
                thumb.rcPage = rcPage;
                int iconSpace = DpiScale(hdc, 20);
                Rect rcText(rcPage.x + iconSpace, rcPage.y + rcPage.dy + 3, rcPage.dx - iconSpace, iconSpace);
                if (isRtl) {
                    rcText.x -= iconSpace;
                }
                thumb.rcText = rcText;
                if (!onScreen) {
                    continue;
                }
                Str path = fs->filePath;
                Rect slRect = rcText.Union(rcPage).Intersect(l.rcThumbsArea);
                if (!slRect.IsEmpty()) {
                    thumb.sl = new StaticLink(slRect, path, HomeThumbTooltipTemp(path));
                    win->staticLinks.Append(thumb.sl);
                }
            }
        }
    }

    // layout tip at the bottom
    if (tip) {
        Rect rcClient = HwndClientRect(win->hwndCanvas);
        int tipPadding = DpiScale(hdc, 8);

        int tipY = rcClient.dy - tipHeight;
        // background spans full window width
        l.rcTip = {0, tipY, rcClient.dx, tipHeight};
        l.tip = tip;

        // text area aligned with thumbnails
        int tipStartX = thumbsStartX;
        int tipStartY = tipY + tipPadding;
        LayoutTip(*tip, thumbsContentWidth, tipStartX, tipStartY);

        // register tip links; per-link rects first so they take priority in hit testing
        for (auto& link : tip->links) {
            // compute bounding rect of all words in this link
            Rect linkRect;
            for (int i = link.firstWord; i <= link.lastWord; i++) {
                auto& w = tip->words[i];
                Rect wr = {w.x, w.y, w.dx, w.dy};
                if (i == link.firstWord) {
                    linkRect = wr;
                } else {
                    linkRect = linkRect.Union(wr);
                }
            }
            auto* slTip = new StaticLink(linkRect, link.cmd, link.cmd);
            win->staticLinks.Append(slTip);
        }
        // tip background: clicking outside of links picks another tip
        auto* slBg = new StaticLink(l.rcTip, kLinkNextTip);
        win->staticLinks.Append(slBg);
    }
}

static void GetFileStateIcon(FileState* fs) {
    if (fs->himl) {
        return;
    }
    SHFILEINFO sfi{};
    sfi.iIcon = -1;
    uint flags = SHGFI_SYSICONINDEX | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES;
    WCHAR* filePathW = CWStrTemp(fs->filePath);
    fs->himl = (HIMAGELIST)SHGetFileInfoW(filePathW, 0, &sfi, sizeof(sfi), flags);
    fs->iconIdx = sfi.iIcon;
}

// --- Close (✕) button for Frequently Read thumbnails (issue #283, #5745) ---
//
// Drawn directly onto the home-page canvas (over the top-right corner of the
// thumbnail under the mouse) rather than as a separate top-level window. The
// separate window could be left behind, drawing stray crosses over a document
// (#5745). Styled like the tab close button (gray X on a white circle; red
// circle + white X on hover).
//
// To keep updates cheap, the glyph is painted/erased in just its own rect: the
// double-buffer (win->buffer) holds the page without the button, so erasing is
// a small BitBlt of that area back to the window. A full home-page repaint
// resets the button (it reappears on the next mouse move).

struct HomeCloseBtn {
    MainWindow* win = nullptr; // window the button currently belongs to
    Str filePath;              // file removed when the button is clicked
    Rect rc;                   // button rect (canvas client coords)
    Rect thumbRc;              // thumbnail rect (canvas client coords)
    bool isHover = false;
    bool visible = false;
};
static HomeCloseBtn gHomeCloseBtn;
// where the glyph is currently painted on the window, so we can erase exactly
// that area (empty when nothing is painted)
static Rect gHomeCloseBtnPaintedRc;

static void DrawHomeCloseGlyph(HDC hdc, const Rect& rc, bool isHover) {
    Gdiplus::Graphics g(hdc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    u8 a = isHover ? 255 : 215;
    int w = rc.dx;
    int h = rc.dy;
    if (isHover) {
        COLORREF bg = kColCloseXHoverBg; // runtime var so GetXValue() isn't a constant cast
        Gdiplus::SolidBrush br(Gdiplus::Color(a, GetRValue(bg), GetGValue(bg), GetBValue(bg)));
        g.FillEllipse(&br, rc.x, rc.y, w - 1, h - 1);
    } else {
        // white circle so the gray X stays visible on any thumbnail background
        Gdiplus::SolidBrush br(Gdiplus::Color(a, 255, 255, 255));
        g.FillEllipse(&br, rc.x, rc.y, w - 1, h - 1);
    }
    COLORREF xcol = isHover ? kColCloseXHover : kColCloseX;
    Gdiplus::Pen pen(Gdiplus::Color(a, GetRValue(xcol), GetGValue(xcol), GetBValue(xcol)), 2.0f);
    int pad = w / 3;
    g.DrawLine(&pen, rc.x + pad, rc.y + pad, rc.x + w - pad, rc.y + h - pad);
    g.DrawLine(&pen, rc.x + w - pad, rc.y + pad, rc.x + pad, rc.y + h - pad);
}

// erase whatever glyph is currently on the window by blitting the button-free
// page back from the double-buffer
static void EraseHomeCloseGlyph(MainWindow* win) {
    Rect& pr = gHomeCloseBtnPaintedRc;
    if (pr.IsEmpty()) {
        return;
    }
    if (win && win->buffer) {
        HDC hdc = GetDC(win->hwndCanvas);
        HDC bufDC = win->buffer->GetDC();
        if (hdc && bufDC) {
            BitBlt(hdc, pr.x, pr.y, pr.dx, pr.dy, bufDC, pr.x, pr.y, SRCCOPY);
        }
        if (hdc) {
            ReleaseDC(win->hwndCanvas, hdc);
        }
    } else {
        // no buffer to restore from: fall back to invalidating the area
        if (win) {
            RECT r = ToRECT(pr);
            HwndInvalidateRect(win->hwndCanvas, ToRect(r), false);
        }
    }
    pr = {};
}

// re-paint the button: erase the previous glyph, then draw the current one
static void RepaintHomeCloseBtn(MainWindow* win) {
    EraseHomeCloseGlyph(win);
    HomeCloseBtn& b = gHomeCloseBtn;
    if (!b.visible || !win) {
        return;
    }
    HDC hdc = GetDC(win->hwndCanvas);
    if (hdc) {
        DrawHomeCloseGlyph(hdc, b.rc, b.isHover);
        ReleaseDC(win->hwndCanvas, hdc);
        gHomeCloseBtnPaintedRc = b.rc;
    }
}

// clear state without touching the window (used by a full home-page repaint,
// which redraws everything anyway)
static void ResetHomeCloseBtn() {
    HomeCloseBtn& b = gHomeCloseBtn;
    b.visible = false;
    b.isHover = false;
    str::Free(b.filePath);
    b.filePath = {};
    gHomeCloseBtnPaintedRc = {};
}

void HomePageHideCloseButton() {
    HomeCloseBtn& b = gHomeCloseBtn;
    if (!b.visible && gHomeCloseBtnPaintedRc.IsEmpty()) {
        return;
    }
    MainWindow* win = b.win;
    EraseHomeCloseGlyph(win);
    b.visible = false;
    b.isHover = false;
    str::Free(b.filePath);
    b.filePath = {};
}

// compute the button rect (canvas client coords) for a thumbnail link
static Rect HomeCloseBtnRectForThumb(MainWindow* win, const Rect& thumb) {
    int sz = DpiScale(win->hwndCanvas, 18);
    int margin = DpiScale(win->hwndCanvas, 5);
    int bx = IsUIRtl() ? (thumb.x + margin) : (thumb.x + thumb.dx - sz - margin);
    int by = thumb.y + margin;
    return Rect(bx, by, sz, sz);
}

void HomePageUpdateCloseButton(MainWindow* win, int x, int y) {
    if (!win || !CanAccessDisk() || HomePageIsListView()) {
        HomePageHideCloseButton();
        return;
    }
    HomeCloseBtn& b = gHomeCloseBtn;
    Point pt(x, y);

    // already showing a button: update hover state as the mouse moves over /
    // off the glyph, but keep it while the mouse stays on the same thumbnail
    if (b.visible) {
        bool overBtn = b.rc.Contains(pt);
        if (overBtn != b.isHover) {
            b.isHover = overBtn;
            RepaintHomeCloseBtn(win);
        }
        if (b.thumbRc.Contains(pt)) {
            return;
        }
    }

    StaticLink* link = nullptr;
    TempStr target = GetStaticLinkAtTemp(win->staticLinks, x, y, &link);
    // a thumbnail link's target is an absolute file path; everything else (a
    // "<...>" command, a "Cmd..." tip link, a URL) is not, so it gets no button
    bool isThumb = len(target) > 0 && link && path::IsAbsolute(target);
    if (!isThumb) {
        HomePageHideCloseButton();
        return;
    }
    if (b.visible && b.filePath && str::Eq(b.filePath, target)) {
        return; // same thumbnail, nothing to do
    }
    b.win = win;
    b.thumbRc = link->rect;
    b.rc = HomeCloseBtnRectForThumb(win, link->rect);
    str::ReplaceWithCopy(&b.filePath, target);
    b.isHover = b.rc.Contains(pt);
    b.visible = true;
    RepaintHomeCloseBtn(win);
}

// called from a left-button click; if the click is on the close button, remove
// the file and return true so the caller doesn't also open the thumbnail
bool HomePageOnCloseButtonClick(MainWindow* win, int x, int y) {
    HomeCloseBtn& b = gHomeCloseBtn;
    if (!b.visible || !b.rc.Contains(Point(x, y))) {
        return false;
    }
    TempStr path = str::DupTemp(b.filePath);
    HomePageHideCloseButton();
    if (win && len(path) > 0) {
        ForgetFileFromFrequentlyRead(win, path);
    }
    return true;
}

void HomePageOnCanvasMouseLeave() {
    // the button is part of the canvas now, so leaving the canvas always hides it
    HomePageHideCloseButton();
}

static void DrawHomeViewButton(HDC hdc, HIMAGELIST himl, Rect r, TbIcon icon, bool selected) {
    if (selected) {
        HdcFillRect(hdc, r, ThemeControlBackgroundColor());
        HBRUSH br = CreateSolidBrush(AccentColor(ThemeControlBackgroundColor(), 40));
        RECT rr = ToRECT(r);
        FrameRect(hdc, &rr, br);
        DeleteObject(br);
    }
    ImageList_Draw(himl, (int)icon, hdc, r.x, r.y, ILD_NORMAL);
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
        return str::DupTemp("");
    }
    return str::FormatSizeShortTemp(size, nullptr);
}

// light blue outline marking the keyboard-selected entry (issue #1136).
// A fixed color: it has to read as "selected" against both the light and the
// dark page background
constexpr COLORREF kHomeSelectionColor = RGB(0x4c, 0xa6, 0xff);

static void DrawHomeSelectionOutline(HDC hdc, const Rect& r, int radius) {
    int penDx = DpiScale(hdc, 2);
    ScopedSelectObject pen(hdc, CreatePen(PS_SOLID, penDx, kHomeSelectionColor), true);
    ScopedSelectObject brush(hdc, GetStockBrush(NULL_BRUSH));
    RoundRect(hdc, r.x, r.y, r.x + r.dx, r.y + r.dy, radius, radius);
}

// Give the file name the width it needs and put the directory path in what's
// left, right-aligned (mirrored for RTL). Done on first paint of a row, not
// during layout: measuring every history entry made layout (and so scrolling)
// slow, and measuring only the rows visible at layout time meant rows scrolled
// into view later never got a directory.
static void MeasureHomeListRowText(HDC hdc, ThumbnailLayout& thumb, HFONT font, bool isRtl) {
    if (thumb.listTextMeasured) {
        return;
    }
    thumb.listTextMeasured = true;

    Rect rcFileName = thumb.rcListFileName;
    TempStr fileName = path::GetBaseNameTemp(thumb.fs->filePath);
    int nameDx = HdcMeasureText(hdc, Str(fileName), font).dx + DpiScale(hdc, 4);
    int minPathDx = DpiScale(hdc, 80);
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
    return win && win->hwndHomeSearch && GetFocus() == win->hwndHomeSearch;
}

static void DrawHomeListRow(HomePageLayout& l, ThumbnailLayout& thumb, HFONT fontText, COLORREF backgroundColor,
                            bool isRtl, bool isSelected) {
    HDC hdc = l.hdc;
    FileState* fs = thumb.fs;
    Rect row = thumb.rcListRow;
    if (!IsHomeThumbOnScreen(row, l.rcThumbsArea)) {
        return;
    }
    MeasureHomeListRowText(hdc, thumb, fontText, isRtl);
    // no selection chrome while typing in the search box
    if (isSelected && !HomeSearchHasFocus(l.win)) {
        DrawHomeSelectionOutline(hdc, Rect(row.x, row.y, row.dx, row.dy - 1), 4);
    }

    COLORREF lineCol = AccentColor(ThemeMainWindowBackgroundColor(), 30);
    ScopedSelectObject pen(hdc, CreatePen(PS_SOLID, 1, lineCol), true);
    HdcDrawLine(hdc, Rect(row.x, row.y + row.dy - 1, row.dx, 0));

    // LoadThumbnail only hits disk the first time; result stays on fs->thumbnail
    Pixmap* thumbImg = LoadThumbnail(fs);
    Rect thumbBox = thumb.rcListThumb;
    if (thumbImg) {
        Size szThumb(thumbImg->width, thumbImg->height);
        Rect thumbDst = FitRectInRect(szThumb, thumbBox);
        BlitPixmap(thumbImg, hdc, thumbDst);
        thumb.szThumb = szThumb;
    }
    Str path = fs->filePath;
    TempStr fileName = path::GetBaseNameTemp(path);
    UINT nameFmt = DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX | (isRtl ? DT_RIGHT : DT_LEFT);
    SelectObject(hdc, fontText);
    {
        DrawMaybeHighlightedText(hdc, thumb.rcListFileName, fileName, l.filterWords, l.highlighted, backgroundColor,
                                 isRtl, false, nameFmt);
    }

    // directory path, right-aligned and muted, in the space the file name doesn't need.
    // Must use DrawTextW: dirPath is UTF-8; DrawTextA treated it as the system ANSI
    // code page and mangled non-ASCII path characters (#5824).
    if (!thumb.rcListPath.IsEmpty()) {
        TempStr dirPath = path::GetDirTemp(path);
        SetTextColor(hdc, ThemeWindowTextDisabledColor());
        UINT pathFmt = DT_SINGLELINE | DT_VCENTER | DT_PATH_ELLIPSIS | DT_NOPREFIX | (isRtl ? DT_LEFT : DT_RIGHT);
        Rect pathRect = thumb.rcListPath;
        HdcDrawText(hdc, dirPath, pathRect, pathFmt);
    }

    // file::GetSize once per row, then cache on ThumbnailLayout (scroll reuses
    // it). kSizeNotFetched = not tried; kSizeFetchFail = GetSize failed; >= 0
    // is a real size (including empty files).
    if (thumb.fileSize == kSizeNotFetched) {
        i64 sz = file::GetSize(path);
        thumb.fileSize = (sz < 0) ? kSizeFetchFail : sz;
    }
    TempStr fileSize = FileSizeForHomeListTemp(thumb.fileSize);
    SetTextColor(hdc, ThemeWindowTextColor());
    UINT sizeFmt = DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX | (isRtl ? DT_LEFT : DT_RIGHT);
    Rect sizeRect = thumb.rcListSize;
    HdcDrawText(hdc, fileSize, sizeRect, sizeFmt);

    ImageList_Draw(l.himlOpen, (int)TbIcon::Close, hdc, thumb.rcListRemove.x, thumb.rcListRemove.y, ILD_NORMAL);
    if (fs->isPinned) {
        HdcFillRect(hdc, thumb.rcListPin, ThemeControlBackgroundColor());
    }
    ImageList_Draw(l.himlOpen, (int)TbIcon::Pin, hdc, thumb.rcListPin.x, thumb.rcListPin.y, ILD_NORMAL);
}

static void DrawHomePageLayout(HomePageLayout& l) {
    bool isRtl = IsUIRtl();
    auto* hdc = l.hdc;
    auto* win = l.win;
    auto backgroundColor = ThemeMainWindowBackgroundColor();

    {
        Rect rc = HwndClientRect(win->hwndCanvas);
        auto color = ThemeMainWindowBackgroundColor();
        HdcFillRect(hdc, rc, color);
    }

    // draw search edit border and background on the canvas
    {
        COLORREF bgCol = ThemeControlBackgroundColor();
        const Rect& sb = l.rcSearchBorder;
        RECT rcBorder = {sb.x, sb.y, sb.x + sb.dx, sb.y + sb.dy};
        // fill interior with control background so padding matches the edit
        HBRUSH brBg = CreateSolidBrush(bgCol);
        HdcFillRect(hdc, ToRect(rcBorder), brBg);
        DeleteObject(brBg);
        // draw border frame
        COLORREF borderCol = AccentColor(bgCol, 40);
        HBRUSH brBorder = CreateSolidBrush(borderCol);
        FrameRect(hdc, &rcBorder, brBorder);
        DeleteObject(brBorder);
    }

    if (false) {
        const Rect& r = l.rcAppWithVer;
        DrawSumatraVersion(hdc, r);
    }

    auto color = ThemeWindowTextColor();
    if (false) {
        ScopedSelectObject pen(hdc, CreatePen(PS_SOLID, 1, color), true);
        HdcDrawLine(hdc, l.rcLine);
    }
    HFONT fontText = HdcCreateSimpleFont(hdc, "MS Shell Dlg", 14);

    AutoDeletePen penThumbBorder(CreatePen(PS_SOLID, kThumbsBorderDx, color));
    color = ThemeWindowLinkColor();
    AutoDeletePen penLinkLine(CreatePen(PS_SOLID, 1, color));

    SelectObject(hdc, penThumbBorder);
    SetBkMode(hdc, TRANSPARENT);
    color = ThemeWindowTextColor();
    SetTextColor(hdc, color);

    DrawHomeViewButton(hdc, l.himlOpen, l.rcIconThumbnailView, TbIcon::HomeThumbnails, !HomePageIsListView());
    DrawHomeViewButton(hdc, l.himlOpen, l.rcIconListView, TbIcon::HomeList, HomePageIsListView());
    l.freqRead->Paint(hdc);
    SelectObject(hdc, GetStockBrush(NULL_BRUSH));

    // clip thumbnails to the middle area
    {
        const Rect& ta = l.rcThumbsArea;
        HRGN thumbsClip = CreateRectRgn(ta.x, ta.y, ta.x + ta.dx, ta.y + ta.dy);
        SelectClipRgn(hdc, thumbsClip);
        DeleteObject(thumbsClip);
    }

    int nThumbs = len(l.thumbnails);
    // keep the keyboard selection inside the (possibly filtered) list
    if (win->homePageSelIdx >= nThumbs) {
        win->homePageSelIdx = nThumbs - 1;
    }
    // no selection rectangle while the search box has focus
    const bool showKeyboardSel = !HomeSearchHasFocus(win);
    // draw selection after restoring the thumbs clip so the outline on the
    // first row isn't clipped at the top edge of rcThumbsArea
    Rect pendingThumbSel;
    bool hasPendingThumbSel = false;
    for (int thumbIdx = 0; thumbIdx < nThumbs; thumbIdx++) {
        ThumbnailLayout& thumb = l.thumbnails[thumbIdx];
        FileState* fs = thumb.fs;
        bool isSelected = showKeyboardSel && (thumbIdx == win->homePageSelIdx);
        if (HomePageIsListView()) {
            DrawHomeListRow(l, thumb, fontText, backgroundColor, isRtl, isSelected);
            continue;
        }
        const Rect& page = thumb.rcPage;
        // skip off-screen thumbs (scroll was redoing Blit+text for every history
        // entry and dominated CPU in DrawHomePageLayout)
        if (!IsHomeThumbOnScreen(page.Union(thumb.rcText), l.rcThumbsArea)) {
            continue;
        }

        // disk load only first time; stays on fs->thumbnail afterwards
        Pixmap* thumbImg = LoadThumbnail(fs);
        if (thumbImg) {
            thumb.szThumb = Size(thumbImg->width, thumbImg->height);
            int savedDC = SaveDC(hdc);
            HRGN clip = CreateRoundRectRgn(page.x, page.y, page.x + page.dx, page.y + page.dy, 10, 10);
            ExtSelectClipRgn(hdc, clip, RGN_AND);
            // note: we used to invert bitmaps in dark theme but that doesn't
            // make sense for thumbnails
            BlitPixmap(thumbImg, hdc, page);
            RestoreDC(hdc, savedDC);
            DeleteObject(clip);
        }
        RoundRect(hdc, page.x, page.y, page.x + page.dx, page.y + page.dy, 10, 10);

        const Rect& rect = thumb.rcText;
        Str path = fs->filePath;
        TempStr fileName = path::GetBaseNameTemp(path);
        UINT fmt = DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX | (isRtl ? DT_RIGHT : DT_LEFT);

        SelectObject(hdc, fontText);
        {
            DrawMaybeHighlightedText(hdc, rect, fileName, l.filterWords, l.highlighted, backgroundColor, isRtl, false,
                                     fmt);
        }

        GetFileStateIcon(fs);
        int x = isRtl ? page.x + page.dx - DpiScale(hdc, 16) : page.x;
        ImageList_Draw(fs->himl, fs->iconIdx, hdc, x, rect.y, ILD_TRANSPARENT);

        if (isSelected) {
            Rect sel = page.Union(rect);
            sel.Inflate(DpiScale(hdc, 4), DpiScale(hdc, 3));
            pendingThumbSel = sel;
            hasPendingThumbSel = true;
        }
    }

    // restore full clip region
    SelectClipRgn(hdc, nullptr);

    if (hasPendingThumbSel) {
        DrawHomeSelectionOutline(hdc, pendingThumbSel, 10);
    }

    color = ThemeWindowLinkColor();
    SetTextColor(hdc, color);
    SelectObject(hdc, penLinkLine);

    int x = l.rcIconOpen.x;
    int y = l.rcIconOpen.y;
    int openIconIdx = 0;
    ImageList_Draw(l.himlOpen, openIconIdx, hdc, x, y, ILD_NORMAL);

    l.openDoc->Paint(hdc);

    if (false) {
        Rect rcFreqRead = DrawHideFrequentlyReadLink(win->hwndCanvas, hdc, _TRA("Hide frequently read"));
        auto* sl = new StaticLink(rcFreqRead, kLinkHideList);
        win->staticLinks.Append(sl);
    }

    // draw tip at the bottom
    if (l.tip) {
        COLORREF tipBgCol = ThemeControlBackgroundColor();
        HdcFillRect(hdc, l.rcTip, tipBgCol);

        HFONT fontTip = HdcCreateSimpleFont(hdc, "MS Shell Dlg", 16);
        COLORREF textCol = ThemeWindowTextColor();
        COLORREF linkCol = ThemeWindowLinkColor();
        DrawTipWords(hdc, *l.tip, fontTip, textCol, linkCol);
    }
}

void DrawHomePage(MainWindow* win, HDC hdc) {
    HWND hwnd = win->hwndFrame;
    // any home-page repaint (scroll, resize, filter) invalidates thumbnail
    // positions and rewrites the buffer, so drop the close button without
    // touching the window; it reappears on the next hover
    ResetHomeCloseBtn();
    DeleteVecMembers(win->staticLinks);

    HomePageLayout l;
    l.rc = HwndClientRect(win->hwndCanvas);
    l.hdc = hdc;
    l.hwnd = hwnd;
    l.win = win;

    TempStr filterText = HomeSearchQueryTemp(win);
    int scrollY = win->homePageScrollY;

    // Prefer the scroll-friendly path: when only scrollY changed, offset cached
    // thumb rects instead of re-running full LayoutHomePage (was ~30% of scroll CPU).
    bool usedCache = false;
    if (HomeLayoutCacheMatches(win, l.rc, filterText)) {
        Vec<FileState*> files;
        StrVec filterWords;
        CollectHomePageFiles(win, files, filterWords);
        if (HomeLayoutCacheFilesMatch(files)) {
            ApplyHomeLayoutCache(l, scrollY);
            usedCache = true;
        }
    }
    if (!usedCache) {
        LayoutHomePage(l);
        SaveHomeLayoutCache(l, filterText, win->homePageScrollY);
    }
    // Keep cue "Search N files …" in sync when history changes without recreating the edit.
    UpdateHomeSearchCueBanner(win);

    DrawHomePageLayout(l);
    SyncHomeLayoutCacheFileSizes(l);

    // update overlay scrollbar for home page if thumbnails overflow visible area
    bool showScrollbarV = ScrollbarsUseOverlay() && l.totalContentDy > l.thumbsVisibleDy;
    if (showScrollbarV) {
        if (!win->overlayScrollV) {
            win->overlayScrollV =
                OverlayScrollbarCreate(win->hwndCanvas, OverlayScrollbar::Type::Vert, ScrollbarsOverlayMode());
        }
        SCROLLINFO si{};
        si.cbSize = sizeof(si);
        si.fMask = SIF_ALL;
        si.nMin = 0;
        si.nMax = l.totalContentDy - 1;
        si.nPage = l.thumbsVisibleDy;
        si.nPos = win->homePageScrollY;
        OverlayScrollbarShow(win->overlayScrollV, true);
        OverlayScrollbarSetInfo(win->overlayScrollV, &si, TRUE);
    }
    // show thin scrollbar briefly to indicate content is scrollable
    OverlayScrollbarShow(win->overlayScrollV, showScrollbarV);
}

// --- keyboard navigation of the file list (issue #1136) ---

// Selection works off the layout cache, which is filled by the last paint, so
// the entries are exactly what's on screen (same order, same filtering).
static int HomeSelectableCount() {
    return gHomeLayoutCache.valid ? len(gHomeLayoutCache.thumbs) : 0;
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
static int HomeGridColumnCount() {
    auto& c = gHomeLayoutCache;
    int n = len(c.thumbs);
    if (n == 0) {
        return 1;
    }
    int y0 = c.thumbs[0].rcPage.y;
    int nCols = 0;
    while (nCols < n && c.thumbs[nCols].rcPage.y == y0) {
        nCols++;
    }
    return nCols > 0 ? nCols : 1;
}

// Select the first-row entry at the column remembered when leaving for search.
static void HomeSelectFromSearchReturnCol(MainWindow* win) {
    int n = HomeSelectableCount();
    if (n <= 0) {
        win->homePageSelIdx = 0;
        return;
    }
    if (HomePageIsListView()) {
        win->homePageSelIdx = 0;
        return;
    }
    int nCols = HomeGridColumnCount();
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
    auto& c = gHomeLayoutCache;
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
    auto& c = gHomeLayoutCache;
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
static Rect HomeSelectionOutlineRect(const ThumbnailLayout& t, HWND hwnd) {
    if (HomePageIsListView()) {
        // list: outline is the row, 1px shorter (separator line)
        return Rect(t.rcListRow.x, t.rcListRow.y, t.rcListRow.dx, t.rcListRow.dy - 1);
    }
    // thumbnails: page ∪ name, inflated by the same amounts as paint
    Rect sel = t.rcPage.Union(t.rcText);
    sel.Inflate(DpiScale(hwnd, 4), DpiScale(hwnd, 3));
    return sel;
}

// Show/update the infotip for the keyboard-selected home entry. Placed just
// below the blue selection outline, left-aligned with the outline's left edge;
// shifted left if it would extend past the right edge of the last outline in
// that row.
static void HomePageShowSelectionTooltip(MainWindow* win) {
    if (!win || HomeSearchHasFocus(win)) {
        if (win) {
            win->DeleteToolTip();
        }
        return;
    }
    auto& c = gHomeLayoutCache;
    int idx = win->homePageSelIdx;
    if (!c.valid || idx < 0 || idx >= len(c.thumbs)) {
        win->DeleteToolTip();
        return;
    }
    HomeSyncLayoutCacheScroll(win);
    ThumbnailLayout& t = c.thumbs[idx];
    FileState* fs = t.fs;
    if (!fs || !fs->filePath) {
        win->DeleteToolTip();
        return;
    }

    // Same text as hover: path + size (size looked up only when shown)
    TempStr tip = HomeThumbTooltipTemp(fs->filePath);
    i64 size = file::GetSize(fs->filePath);
    if (size >= 0) {
        tip = fmt("%s  %s", tip, str::FormatSizeShortTemp(size, nullptr));
    }

    HWND hwnd = win->hwndCanvas;
    Rect outline = HomeSelectionOutlineRect(t, hwnd);
    // a little below the outline so the tip clears the blue border
    int tipClientX = outline.x;
    int tipClientY = outline.y + outline.dy + DpiScale(hwnd, 4);

    int rightEdgeClient = outline.x + outline.dx;
    if (!HomePageIsListView()) {
        int n = len(c.thumbs);
        int nCols = HomeGridColumnCount();
        nCols = std::max(nCols, 1);
        int col = idx % nCols;
        int rowStart = idx - col;
        int lastInRow = rowStart + nCols - 1;
        if (lastInRow >= n) {
            lastInRow = n - 1;
        }
        Rect lastOutline = HomeSelectionOutlineRect(c.thumbs[lastInRow], hwnd);
        rightEdgeClient = lastOutline.x + lastOutline.dx;
    }

    POINT tl{tipClientX, tipClientY};
    POINT tr{rightEdgeClient, tipClientY};
    ClientToScreen(hwnd, &tl);
    ClientToScreen(hwnd, &tr);
    win->ShowToolTipAt(tip, outline, Point(tl.x, tl.y), false, tr.x);
}

void HomePageSelectFirst(MainWindow* win) {
    win->homePageSelIdx = 0;
    win->homePageSearchReturnCol = 0;
}

void HomePageOnWindowActivate(MainWindow* win, bool active) {
    if (!win) {
        return;
    }
    if (!active) {
        win->DeleteToolTip();
        return;
    }
    // only restore the selection tip (positioned at the active entry, not cursor)
    if (win->IsCurrentTabAbout()) {
        HomePageShowSelectionTooltip(win);
    }
}

// Index of the file entry under (x,y), or -1. Uses layout-cache geometry so
// it matches the keyboard selection outline.
static int HomeEntryIndexAt(int x, int y) {
    auto& c = gHomeLayoutCache;
    if (!c.valid) {
        return -1;
    }
    Point pt(x, y);
    int n = len(c.thumbs);
    for (int i = 0; i < n; i++) {
        if (HomeEntryRect(c.thumbs[i]).Contains(pt)) {
            return i;
        }
    }
    return -1;
}

// Last mouse position that drove a selection change. Keyboard nav invalidates
// the canvas and Windows may re-send WM_MOUSEMOVE / WM_SETCURSOR with the same
// coordinates — ignore those so selection does not snap back under the cursor.
static Point gHomeHoverLastPt{-1, -1};

bool HomePageOnHover(MainWindow* win, int x, int y) {
    if (!win) {
        return false;
    }
    Point pt(x, y);
    if (pt.x == gHomeHoverLastPt.x && pt.y == gHomeHoverLastPt.y) {
        // no real mouse movement — leave selection alone
        return HomeEntryIndexAt(x, y) >= 0;
    }
    gHomeHoverLastPt = pt;

    HomeSyncLayoutCacheScroll(win);
    int idx = HomeEntryIndexAt(x, y);
    if (idx < 0) {
        return false;
    }
    if (idx != win->homePageSelIdx) {
        win->homePageSelIdx = idx;
        HwndInvalidate(win->hwndCanvas);
    }
    // tip always anchored to the active entry, never the cursor
    HomePageShowSelectionTooltip(win);
    return true;
}

Str HomePageSelectedFilePathTemp(MainWindow* win) {
    auto& c = gHomeLayoutCache;
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

void HomePageMoveSelection(MainWindow* win, int dCol, int dRow) {
    int n = HomeSelectableCount();
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

    int nCols = HomePageIsListView() ? 1 : HomeGridColumnCount();
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
        if (dRow < 0 && win->hwndHomeSearch) {
            win->homePageSearchReturnCol = HomePageIsListView() ? 0 : (idx % nCols);
            win->DeleteToolTip();
            HwndSetFocus(win->hwndHomeSearch);
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
    HwndInvalidate(win->hwndCanvas);
    HomePageShowSelectionTooltip(win);
}

void HomePageOnVScroll(MainWindow* win, WPARAM wp) {
    USHORT msg = LOWORD(wp);
    HDC hdc = GetDC(win->hwndCanvas);
    int lineDy = HomePageIsListView() ? kHomeListRowDy : kThumbnailDy + kThumbsSpaceBetweenY;
    int pageDy = lineDy * 3;
    ReleaseDC(win->hwndCanvas, hdc);

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
        HwndInvalidate(win->hwndCanvas);
    }
}

void HomePageOnMouseWheel(MainWindow* win, int delta) {
    HDC hdc = GetDC(win->hwndCanvas);
    int thumbsRowDy = HomePageIsListView() ? kHomeListRowDy : kThumbnailDy + kThumbsSpaceBetweenY;
    ReleaseDC(win->hwndCanvas, hdc);

    int scrollBy = thumbsRowDy / 3;
    if (delta > 0) {
        scrollBy = -scrollBy;
    }
    int newScrollY = win->homePageScrollY + scrollBy;
    newScrollY = std::max(newScrollY, 0);
    if (newScrollY != win->homePageScrollY) {
        win->homePageScrollY = newScrollY;
        HwndInvalidate(win->hwndCanvas);
    }
}
