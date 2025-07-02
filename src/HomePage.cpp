/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/Dpi.h"
#include "utils/FileUtil.h"
#include "utils/WinUtil.h"
#include "utils/SquareTreeParser.h"

#include "wingui/UIModels.h"
#include "wingui/Layout.h"
#include "wingui/WinGui.h"
#include "wingui/VirtWnd.h"

#include "Settings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "SumatraConfig.h"
#include "FileHistory.h"
#include "AppColors.h"
#include "GlobalPrefs.h"
#include "Annotation.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "resource.h"
#include "Commands.h"
#include "FileThumbnails.h"
#include "HomePage.h"
#include "Translations.h"
#include "Version.h"
#include "Theme.h"
#include "DarkModeSubclass.h"

#ifndef ABOUT_USE_LESS_COLORS
#define ABOUT_LINE_OUTER_SIZE 2
#else
#define ABOUT_LINE_OUTER_SIZE 1
#endif
#define ABOUT_LINE_SEP_SIZE 1

constexpr const char* promoteBuiltIn = R"(
[
    Name = Edna
    URL = https://edna.arslexis.io
    Info = note taking app for develelopers
]
[
    Name = "ArsLexis"
    URL = https://arslexis.io
    Info = Various web tools
]
)";

constexpr COLORREF kAboutBorderCol = RGB(0, 0, 0);

constexpr int kAboutLeftRightSpaceDx = 8;
constexpr int kAboutMarginDx = 10;
constexpr int kAboutBoxMarginDy = 6;
constexpr int kAboutTxtDy = 6;
constexpr int kAboutRectPadding = 8;

constexpr int kInnerPadding = 8;

constexpr const char* kSumatraTxtFont = "Arial Black";
constexpr int kSumatraTxtFontSize = 24;

constexpr const char* kVersionTxtFont = "Arial Black";
constexpr int kVersionTxtFontSize = 12;

#define LAYOUT_LTR 0

static ATOM gAtomAbout;
static HWND gHwndAbout;
static Tooltip* gAboutTooltip = nullptr;
static const char* gClickedURL = nullptr;

struct AboutLayoutInfoEl {
    /* static data, must be provided */
    const char* leftTxt;
    const char* rightTxt;
    const char* url;

    /* data calculated by the layout */
    Rect leftPos;
    Rect rightPos;
};

static AboutLayoutInfoEl gAboutLayoutInfo[] = {
    {"website", "SumatraPDF website", kWebsiteURL},
    {"manual", "SumatraPDF manual", kManualURL},
    {"forums", "SumatraPDF forums", "https://github.com/sumatrapdfreader/sumatrapdf/discussions"},
    {"programming", "The Programmers", "https://github.com/sumatrapdfreader/sumatrapdf/blob/master/AUTHORS"},
    {"translations", "The Translators", "https://github.com/sumatrapdfreader/sumatrapdf/blob/master/TRANSLATORS"},
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

static Vec<StaticLinkInfo*> gStaticLinks;

static TempStr GetAppVersionTemp() {
    char* s = str::DupTemp("v" CURR_VERSION_STRA);
    if (IsProcess64()) {
        s = str::JoinTemp(s, " 64-bit");
    }
    if (gIsDebugBuild) {
        s = str::JoinTemp(s, " (dbg)");
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
    HFONT fontSumatraTxt = CreateSimpleFont(hdc, kSumatraTxtFont, kSumatraTxtFontSize);
    HFONT fontVersionTxt = CreateSimpleFont(hdc, kVersionTxtFont, kVersionTxtFontSize);

    SetBkMode(hdc, TRANSPARENT);

    const char* txt = kAppName;
    Size txtSize = HdcMeasureText(hdc, txt, fmt, fontSumatraTxt);
    Rect mainRect(rect.x + (rect.dx - txtSize.dx) / 2, rect.y + (rect.dy - txtSize.dy) / 2, txtSize.dx, txtSize.dy);

    // draw SumatraPDF in colorful way
    Point pt = mainRect.TL();
    // colorful version
    static COLORREF cols[] = {kCol1, kCol2, kCol3, kCol4, kCol5, kCol5, kCol4, kCol3, kCol2, kCol1};
    char buf[2] = {0};
    for (int i = 0; i < str::Leni(kAppName); i++) {
        SetTextColor(hdc, cols[i % dimofi(cols)]);
        buf[0] = kAppName[i];
        HdcDrawText(hdc, buf, pt, fmt, fontSumatraTxt);
        txtSize = HdcMeasureText(hdc, buf, fmt, fontSumatraTxt);
        pt.x += txtSize.dx;
    }

    SetTextColor(hdc, ThemeWindowTextColor());
    int x = mainRect.x + mainRect.dx + DpiScale(hdc, kInnerPadding);
    int y = mainRect.y;

    char* ver = GetAppVersionTemp();
    Point p = {x, y};
    HdcDrawText(hdc, ver, p, fmt, fontVersionTxt);
    p.y += DpiScale(hdc, 13);
    if (gIsPreReleaseBuild) {
        HdcDrawText(hdc, "Pre-release", p, fmt);
    }
}

// draw on the bottom right
static Rect DrawHideFrequentlyReadLink(HWND hwnd, HDC hdc, const char* txt) {
    HFONT fontLeftTxt = CreateSimpleFont(hdc, "MS Shell Dlg", 16);

    VirtWndText w(hwnd, txt, fontLeftTxt);
    w.isRtl = IsUIRtl();
    w.withUnderline = true;
    Size txtSize = w.GetIdealSize(true);

    auto col = ThemeWindowLinkColor();
    ScopedSelectObject pen(hdc, CreatePen(PS_SOLID, 1, col), true);

    SetTextColor(hdc, col);
    SetBkMode(hdc, TRANSPARENT);
    Rect rc = ClientRect(hwnd);

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
    HFONT fontSumatraTxt = CreateSimpleFont(hdc, kSumatraTxtFont, kSumatraTxtFontSize);
    HFONT fontVersionTxt = CreateSimpleFont(hdc, kVersionTxtFont, kVersionTxtFontSize);

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

static TempStr TrimGitTemp(char* s) {
    if (gitCommidId && str::EndsWith(s, gitCommidId)) {
        auto sLen = str::Len(s);
        auto gitLen = str::Len(gitCommidId);
        s = str::DupTemp(s, sLen - gitLen - 7);
    }
    return s;
}

/* Draws the about screen and remembers some state for hyperlinking.
   It transcribes the design I did in graphics software - hopeless
   to understand without seeing the design. */
static void DrawAbout(HWND hwnd, HDC hdc, Rect rect, Vec<StaticLinkInfo*>& staticLinks) {
    auto col = ThemeWindowTextColor();
    AutoDeletePen penBorder(CreatePen(PS_SOLID, ABOUT_LINE_OUTER_SIZE, col));
    AutoDeletePen penDivideLine(CreatePen(PS_SOLID, ABOUT_LINE_SEP_SIZE, col));
    col = ThemeWindowLinkColor();
    AutoDeletePen penLinkLine(CreatePen(PS_SOLID, ABOUT_LINE_SEP_SIZE, col));

    HFONT fontLeftTxt = CreateSimpleFont(hdc, kLeftTextFont, kLeftTextFontSize);
    HFONT fontRightTxt = CreateSimpleFont(hdc, kRightTextFont, kRightTextFontSize);

    ScopedSelectObject font(hdc, fontLeftTxt); /* Just to remember the orig font */

    Rect rc = ClientRect(hwnd);
    col = ThemeMainWindowBackgroundColor();
    AutoDeleteBrush brushAboutBg = CreateSolidBrush(col);
    FillRect(hdc, rc, brushAboutBg);

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
    FillRect(hdc, &rcLogoBg, bgBrush);
    DrawLine(hdc, Rect(0, rect.y, rc.dx, 0));
    DrawLine(hdc, Rect(0, rect.y + titleRect.dy, rc.dx, 0));
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
        char* s = (char*)el->rightTxt;
        s = TrimGitTemp(s);
        auto& pos = el->rightPos;
        HdcDrawText(hdc, s, pos, fmt);

        if (hasUrl) {
            int underlineY = pos.y + pos.dy - 3;
            DrawLine(hdc, Rect(pos.x, underlineY, pos.dx, 0));
            auto sl = new StaticLinkInfo(pos, el->url, el->url);
            staticLinks.Append(sl);
        }
    }

    SelectObject(hdc, penDivideLine);
    Rect divideLine(gAboutLayoutInfo[0].rightPos.x - DpiScale(hwnd, kAboutLeftRightSpaceDx), rect.y + titleRect.dy + 4,
                    0, rect.y + rect.dy - 4 - gAboutLayoutInfo[0].rightPos.y);
    DrawLine(hdc, divideLine);
}

static void UpdateAboutLayoutInfo(HWND hwnd, HDC hdc, Rect* rect) {
    HFONT fontLeftTxt = CreateSimpleFont(hdc, kLeftTextFont, kLeftTextFontSize);
    HFONT fontRightTxt = CreateSimpleFont(hdc, kRightTextFont, kRightTextFontSize);

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
        if (leftLargestDx < el->leftPos.dx) {
            leftLargestDx = el->leftPos.dx;
        }
    }

    /* calculate right text dimensions */
    int rightLargestDx = 0;
    int rightDy = 0;
    for (AboutLayoutInfoEl* el = gAboutLayoutInfo; el->leftTxt; el++) {
        char* s = (char*)el->rightTxt;
        s = TrimGitTemp(s);
        Size txtSize = HdcMeasureText(hdc, s, fmt, fontRightTxt);
        el->rightPos.dx = txtSize.dx;
        el->rightPos.dy = txtSize.dy;

        if (el == &gAboutLayoutInfo[0]) {
            rightDy = el->rightPos.dy;
        } else {
            ReportIf(rightDy != el->rightPos.dy);
        }
        if (rightLargestDx < el->rightPos.dx) {
            rightLargestDx = el->rightPos.dx;
        }
    }

    int leftRightSpaceDx = DpiScale(hwnd, kAboutLeftRightSpaceDx);
    int marginDx = DpiScale(hwnd, kAboutMarginDx);
    int aboutTxtDy = DpiScale(hwnd, kAboutTxtDy);
    /* calculate total dimension and position */
    Rect minRect;
    minRect.dx = leftRightSpaceDx + leftLargestDx + ABOUT_LINE_SEP_SIZE + rightLargestDx + leftRightSpaceDx;
    if (minRect.dx < headerSize.dx) {
        minRect.dx = headerSize.dx;
    }
    minRect.dx += 2 * ABOUT_LINE_OUTER_SIZE + 2 * marginDx;

    minRect.dy = headerSize.dy;
    for (AboutLayoutInfoEl* el = gAboutLayoutInfo; el->leftTxt; el++) {
        minRect.dy += rightDy + aboutTxtDy;
    }
    minRect.dy += 2 * ABOUT_LINE_OUTER_SIZE + 4;

    Rect rc = ClientRect(hwnd);
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
        el->leftPos.y = currY + (rightDy - leftDy) / 2;
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

static void OnSizeAbout(HWND hwnd) {
    // TODO: do I need anything here?
}

static void CopyAboutInfoToClipboard() {
    str::Str info(512);
    char* ver = GetAppVersionTemp();
    info.AppendFmt("%s %s\r\n", kAppName, ver);
    for (int i = info.Size() - 2; i > 0; i--) {
        info.AppendChar('-');
    }
    info.Append("\r\n");
    // concatenate all the information into a single string
    // (cf. CopyPropertiesToClipboard in SumatraProperties.cpp)
    int maxLen = 0;
    for (AboutLayoutInfoEl* el = gAboutLayoutInfo; el->leftTxt; el++) {
        maxLen = std::max(maxLen, str::Leni(el->leftTxt));
    }
    for (AboutLayoutInfoEl* el = gAboutLayoutInfo; el->leftTxt; el++) {
        for (int i = maxLen - str::Leni(el->leftTxt); i > 0; i--) {
            info.AppendChar(' ');
        }
        info.AppendFmt("%s: %s\r\n", el->leftTxt, el->url ? el->url : el->rightTxt);
    }
    CopyTextToClipboard(info.LendData());
}

char* GetStaticLinkTemp(Vec<StaticLinkInfo*>& staticLinks, int x, int y, StaticLinkInfo** linkOut) {
    if (!CanAccessDisk()) {
        return nullptr;
    }

    Point pt(x, y);
    for (size_t i = 0; i < staticLinks.size(); i++) {
        if (staticLinks.at(i)->rect.Contains(pt)) {
            if (linkOut) {
                *linkOut = staticLinks.at(i);
            }
            auto res = staticLinks.at(i)->target;
            return str::DupTemp(res);
        }
    }

    return nullptr;
}

static void CreateInfotipForLink(StaticLinkInfo* linkInfo) {
    if (gAboutTooltip != nullptr) {
        return;
    }

    gAboutTooltip = new Tooltip();
    Tooltip::CreateArgs args;
    args.parent = gHwndAbout;
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

LRESULT CALLBACK WndProcAbout(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    const char* url;
    Point pt;

    int x = GET_X_LPARAM(lp);
    int y = GET_Y_LPARAM(lp);
    switch (msg) {
        case WM_CREATE:
            ReportIf(gHwndAbout);
            if (gUseDarkModeLib) {
                DarkMode::setDarkTitleBarEx(hwnd, true);
            }
            break;

        case WM_ERASEBKGND:
            // do nothing, helps to avoid flicker
            return TRUE;

        case WM_SIZE:
            OnSizeAbout(hwnd);
            break;

        case WM_PAINT:
            OnPaintAbout(hwnd);
            break;

        case WM_SETCURSOR:
            pt = HwndGetCursorPos(hwnd);
            if (!pt.IsEmpty()) {
                StaticLinkInfo* linkInfo;
                if (GetStaticLinkTemp(gStaticLinks, pt.x, pt.y, &linkInfo)) {
                    CreateInfotipForLink(linkInfo);
                    SetCursorCached(IDC_HAND);
                    return TRUE;
                }
            }
            DeleteInfotip();
            return DefWindowProc(hwnd, msg, wp, lp);

        case WM_LBUTTONDOWN: {
            url = GetStaticLinkTemp(gStaticLinks, x, y, nullptr);
            str::ReplaceWithCopy(&gClickedURL, url);
        } break;

        case WM_LBUTTONUP:
            url = GetStaticLinkTemp(gStaticLinks, x, y, nullptr);
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

    TempWStr title = ToWStrTemp(_TRA("About SumatraPDF"));
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
    Rect wRc = WindowRect(gHwndAbout);
    Rect cRc = ClientRect(gHwndAbout);
    wRc.dx += rc.dx - cRc.dx;
    wRc.dy += rc.dy - cRc.dy;
    MoveWindow(gHwndAbout, wRc.x, wRc.y, wRc.dx, wRc.dy, FALSE);

    HwndPositionInCenterOf(gHwndAbout, win->hwndFrame);
    ShowWindow(gHwndAbout, SW_SHOW);
}

void DrawAboutPage(MainWindow* win, HDC hdc) {
    Rect rc = ClientRect(win->hwndCanvas);
    UpdateAboutLayoutInfo(win->hwndCanvas, hdc, &rc);
    DrawAbout(win->hwndCanvas, hdc, rc, win->staticLinks);
    if (HasPermission(Perm::SavePreferences | Perm::DiskAccess) && gGlobalPrefs->rememberOpenedFiles) {
        Rect rect = DrawHideFrequentlyReadLink(win->hwndCanvas, hdc, _TRA("Show frequently read"));
        auto sl = new StaticLinkInfo(rect, kLinkShowList);
        win->staticLinks.Append(sl);
    }
}

/* alternate static page to display when no document is loaded */

constexpr int kThumbsMaxCols = 5;
constexpr int kThumbsSeparatorDy = 2;
constexpr int kThumbsBorderDx = 1;
#define kThumbsMarginLeft DpiScale(hdc, 40)
#define kThumbsMarginRight DpiScale(hdc, 40)
#define kThumbsMarginTop DpiScale(hdc, 50)
#define kThumbsMarginBottom DpiScale(hdc, 40)
#define kThumbsSpaceBetweenX DpiScale(hdc, 30)
#define kThumbsSpaceBetweenY DpiScale(hdc, 50)
#define kThumbsBottomBoxDy DpiScale(hdc, 50)

struct ThumbnailLayout {
    Rect rcPage;
    Size szThumb;
    Rect rcText;
    FileState* fs = nullptr; // info needed to draw the thumbnail
    StaticLinkInfo* sl = nullptr;
};

struct Promote {
    struct Promote* next = nullptr;
    const char* name = nullptr;
    const char* url = nullptr;
    const char* info = nullptr;
    Promote() = default;
    ~Promote();
};

Promote::~Promote() {
    str::Free(name);
    str::Free(url);
    str::Free(info);
}

struct HomePageLayout {
    // args in
    HWND hwnd = nullptr;
    HDC hdc = nullptr;
    Rect rc;
    MainWindow* win = nullptr;

    Promote* promote = nullptr;

    Rect rcAppWithVer; // SumatraPDF colorful text + version
    Rect rcLine;       // line under bApp
    Rect rcIconOpen;

    HIMAGELIST himlOpen = nullptr;
    VirtWndText* freqRead = nullptr;
    VirtWndText* openDoc = nullptr;
    VirtWndText* hideShowFreqRead = nullptr;
    Vec<ThumbnailLayout> thumbnails; // info for each thumbnail
    ~HomePageLayout();
};

HomePageLayout::~HomePageLayout() {
    delete freqRead;
    delete openDoc;
    ListDelete(promote);
}

static Promote* ParsePromote(const char* s) {
    if (str::IsEmptyOrWhiteSpace(s)) {
        return nullptr;
    }
    SquareTreeNode* root = ParseSquareTree(s);
    if (!root) {
        return nullptr;
    }
    SquareTreeNode* node;
    Promote* first = nullptr;
    for (auto& i : root->data) {
        node = i.child;
        if (!node || node->data.Size() != 3) {
            continue;
        }
        Promote* p = new Promote();
        p->name = str::Dup(node->GetValue("Name"));
        p->url = str::Dup(node->GetValue("URL"));
        p->info = str::Dup(node->GetValue("Info"));
        bool ok = !str::IsEmptyOrWhiteSpace(p->name) && !str::IsEmptyOrWhiteSpace(p->url) &&
                  !str::IsEmptyOrWhiteSpace(p->info);
        if (!ok) {
            delete p;
            continue;
        }
        ListInsertEnd(&first, p);
    }
    delete root;
    return first;
}

// layout homepage in r
void LayoutHomePage(HomePageLayout& l) {
    l.promote = ParsePromote(promoteBuiltIn);

    Vec<FileState*> fileStates;
    gFileHistory.GetFrequencyOrder(fileStates);
    auto hwnd = l.hwnd;
    auto hdc = l.hdc;
    auto rc = l.rc;
    auto win = l.win;

    bool isRtl = IsUIRtl();
    HFONT fontText = CreateSimpleFont(hdc, "MS Shell Dlg", 14);
    HFONT fontFrequentlyRead = CreateSimpleFont(hdc, "MS Shell Dlg", 24);

    Size sz = CalcSumatraVersionSize(hdc);
    {
        Rect& r = l.rcAppWithVer;
        r.x = rc.dx - sz.dx - 3;
        r.y = 0;
        r.SetSize(sz);
    }

    l.rcLine = {0, sz.dy, rc.dx, 0};

    Rect& titleBox = l.rcAppWithVer;
    rc.SubTB(titleBox.dy, kThumbsBottomBoxDy);

    int dx =
        (rc.dx - kThumbsMarginLeft - kThumbsMarginRight + kThumbsSpaceBetweenX) / (kThumbnailDx + kThumbsSpaceBetweenX);
    int thumbsCols = limitValue(dx, 1, kThumbsMaxCols);
    int dy =
        (rc.dy - kThumbsMarginTop - kThumbsMarginBottom + kThumbsSpaceBetweenY) / (kThumbnailDy + kThumbsSpaceBetweenY);
    int thumbsRows = std::min(dy, kFileHistoryMaxFrequent / thumbsCols);
    int x = rc.x + kThumbsMarginLeft +
            (rc.dx - thumbsCols * kThumbnailDx - (thumbsCols - 1) * kThumbsSpaceBetweenX - kThumbsMarginLeft -
             kThumbsMarginRight) /
                2;
    rc.y = 0;
    Point ptOff(x, rc.y + kThumbsMarginTop);
    if (ptOff.x < DpiScale(hdc, kInnerPadding)) {
        ptOff.x = DpiScale(hdc, kInnerPadding);
    } else if (fileStates.size() == 0) {
        ptOff.x = kThumbsMarginLeft;
    }

    const char* txt = _TRA("Frequently Read");
    VirtWndText* freqRead = new VirtWndText(hwnd, txt, fontFrequentlyRead);
    l.freqRead = freqRead;
    freqRead->isRtl = isRtl;
    Size txtSize = freqRead->GetIdealSize(true);

    Rect rcHdr(ptOff.x, rc.y + (kThumbsMarginTop - txtSize.dy) / 2, txtSize.dx, txtSize.dy);
    if (isRtl) {
        rcHdr.x = rc.dx - ptOff.x - rcHdr.dx;
    }
    freqRead->SetBounds(rcHdr);

    int nFiles = fileStates.Size();
    for (int row = 0; row < thumbsRows; row++) {
        for (int col = 0; col < thumbsCols; col++) {
            if (row * thumbsCols + col >= nFiles) {
                // display the "Open a document" link right below the last row
                thumbsRows = col > 0 ? row + 1 : row;
                break;
            }
            int idx = row * thumbsCols + col;
            ThumbnailLayout& thumb = *l.thumbnails.AppendBlanks(1);
            FileState* fs = fileStates.at(row * thumbsCols + col);
            thumb.fs = fs;

            Rect rcPage(ptOff.x + col * (kThumbnailDx + kThumbsSpaceBetweenX),
                        ptOff.y + row * (kThumbnailDy + kThumbsSpaceBetweenY), kThumbnailDx, kThumbnailDy);
            if (isRtl) {
                rcPage.x = rc.dx - rcPage.x - rcPage.dx;
            }
            RenderedBitmap* thumbImg = LoadThumbnail(fs);
            if (thumbImg) {
                Size szThumb = thumbImg->GetSize();
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
            char* path = fs->filePath;
            thumb.sl = new StaticLinkInfo(rcText.Union(rcPage), path, path);
            win->staticLinks.Append(thumb.sl);
        }
    }
    /* bottom links */
    rc.y +=
        kThumbsMarginTop + thumbsRows * kThumbnailDy + (thumbsRows - 1) * kThumbsSpaceBetweenY + kThumbsMarginBottom;
    rc.dy = kThumbsBottomBoxDy;

    l.himlOpen = (HIMAGELIST)SendMessageW(win->hwndToolbar, TB_GETIMAGELIST, 0, 0);
    Rect rcIconOpen(ptOff.x, rc.y, 0, 0);
    ImageList_GetIconSize(l.himlOpen, &rcIconOpen.dx, &rcIconOpen.dy);
    rcIconOpen.y += (rc.dy - rcIconOpen.dy) / 2;
    if (isRtl) {
        rcIconOpen.x = rc.dx - ptOff.x - rcIconOpen.dx;
    }
    l.rcIconOpen = rcIconOpen;

    txt = _TRA("Open a document...");
    auto openDoc = new VirtWndText(hwnd, txt, fontText);
    openDoc->isRtl = isRtl;
    openDoc->withUnderline = true;
    txtSize = openDoc->GetIdealSize(true);
    Rect rcOpenDoc(ptOff.x + rcIconOpen.dx + 3, rc.y + (rc.dy - txtSize.dy) / 2, txtSize.dx, txtSize.dy);
    if (isRtl) {
        rcOpenDoc.x = rcIconOpen.x - rcOpenDoc.dx - 3;
    }
    openDoc->SetBounds(rcOpenDoc);

    // make the click target larger
    rcOpenDoc = rcOpenDoc.Union(rcIconOpen);
    rcOpenDoc.Inflate(10, 10);
    l.openDoc = openDoc;
    auto sl = new StaticLinkInfo(rcOpenDoc, kLinkOpenFile);
    win->staticLinks.Append(sl);
}

static void GetFileStateIcon(FileState* fs) {
    if (fs->himl) {
        return;
    }
    SHFILEINFO sfi{};
    sfi.iIcon = -1;
    uint flags = SHGFI_SYSICONINDEX | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES;
    WCHAR* filePathW = ToWStrTemp(fs->filePath);
    fs->himl = (HIMAGELIST)SHGetFileInfoW(filePathW, 0, &sfi, sizeof(sfi), flags);
    fs->iconIdx = sfi.iIcon;
}

static void DrawHomePageLayout(const HomePageLayout& l) {
    bool isRtl = IsUIRtl();
    auto hdc = l.hdc;
    auto win = l.win;
    auto textColor = ThemeWindowTextColor();
    auto backgroundColor = ThemeMainWindowBackgroundColor();

    {
        Rect rc = ClientRect(win->hwndCanvas);
        auto color = ThemeMainWindowBackgroundColor();
        FillRect(hdc, rc, color);
    }

    if (false) {
        const Rect& r = l.rcAppWithVer;
        DrawSumatraVersion(hdc, r);
    }

    auto color = ThemeWindowTextColor();
    if (false) {
        ScopedSelectObject pen(hdc, CreatePen(PS_SOLID, 1, color), true);
        DrawLine(hdc, l.rcLine);
    }
    HFONT fontText = CreateSimpleFont(hdc, "MS Shell Dlg", 14);

    AutoDeletePen penThumbBorder(CreatePen(PS_SOLID, kThumbsBorderDx, color));
    color = ThemeWindowLinkColor();
    AutoDeletePen penLinkLine(CreatePen(PS_SOLID, 1, color));

    SelectObject(hdc, penThumbBorder);
    SetBkMode(hdc, TRANSPARENT);
    color = ThemeWindowTextColor();
    SetTextColor(hdc, color);

    l.freqRead->Paint(hdc);
    SelectObject(hdc, GetStockBrush(NULL_BRUSH));

    for (const ThumbnailLayout& thumb : l.thumbnails) {
        FileState* fs = thumb.fs;
        const Rect& page = thumb.rcPage;

        RenderedBitmap* thumbImg = LoadThumbnail(fs);
        if (thumbImg) {
            HRGN clip = CreateRoundRectRgn(page.x, page.y, page.x + page.dx, page.y + page.dy, 10, 10);
            SelectClipRgn(hdc, clip);
            // note: we used to invert bitmaps in dark theme but that doesn't
            // make sense for thumbnails
            RenderedBitmap* clone = nullptr; // state->thumbnail->Clone();
            if (clone) {
                UpdateBitmapColors(clone->GetBitmap(), textColor, backgroundColor);
                clone->Blit(hdc, page);
                delete clone;
            } else {
                thumbImg->Blit(hdc, page);
            }
            SelectClipRgn(hdc, nullptr);
            DeleteObject(clip);
        }
        RoundRect(hdc, page.x, page.y, page.x + page.dx, page.y + page.dy, 10, 10);

        const Rect& rect = thumb.rcText;
        char* path = fs->filePath;
        TempStr fileName = path::GetBaseNameTemp(path);
        UINT fmt = DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX | (isRtl ? DT_RIGHT : DT_LEFT);
        HdcDrawText(hdc, fileName, rect, fmt, fontText);

        GetFileStateIcon(fs);
        int x = isRtl ? page.x + page.dx - DpiScale(hdc, 16) : page.x;
        ImageList_Draw(fs->himl, fs->iconIdx, hdc, x, rect.y, ILD_TRANSPARENT);
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
        auto sl = new StaticLinkInfo(rcFreqRead, kLinkHideList);
        win->staticLinks.Append(sl);
    }
}

void DrawHomePage(MainWindow* win, HDC hdc) {
    HWND hwnd = win->hwndFrame;
    DeleteVecMembers(win->staticLinks);

    HomePageLayout l;
    l.rc = ClientRect(win->hwndCanvas);
    ;
    l.hdc = hdc;
    l.hwnd = hwnd;
    l.win = win;
    LayoutHomePage(l);

    DrawHomePageLayout(l);
}
