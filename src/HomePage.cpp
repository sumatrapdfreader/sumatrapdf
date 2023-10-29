/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/Dpi.h"
#include "utils/FileUtil.h"
#include "utils/WinUtil.h"

#include "wingui/UIModels.h"
#include "wingui/Layout.h"
#include "wingui/WinGui.h"

#include "Settings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "SumatraConfig.h"
#include "FileHistory.h"
#include "AppColors.h"
#include "GlobalPrefs.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "resource.h"
#include "Commands.h"
#include "FileThumbnails.h"
#include "HomePage.h"
#include "Translations.h"
#include "Version.h"
#include "Theme.h"
#include "Widget.h"

#ifndef ABOUT_USE_LESS_COLORS
#define ABOUT_LINE_OUTER_SIZE 2
#else
#define ABOUT_LINE_OUTER_SIZE 1
#endif
#define ABOUT_LINE_SEP_SIZE 1

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

#ifdef PRE_RELEASE_VER
#define VERSION_SUB_TXT "Pre-release"
#else
#define VERSION_SUB_TXT ""
#endif

#ifdef GIT_COMMIT_ID
#define GIT_COMMIT_ID_STR QM(GIT_COMMIT_ID)
#endif

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
#ifdef GIT_COMMIT_ID
    // TODO: use short ID for rightTxt (only first 7 digits) with less hackery
    {"last change", "git commit " GIT_COMMIT_ID_STR,
     "https://github.com/sumatrapdfreader/sumatrapdf/commit/" GIT_COMMIT_ID_STR},
#endif
#ifdef PRE_RELEASE_VER
    {"a note", "Pre-release version, for testing only!", nullptr},
#endif
#ifdef DEBUG
    {"a note", "Debug version, for testing only!", nullptr},
#endif
    {nullptr, nullptr, nullptr}};

static Vec<StaticLinkInfo*> gStaticLinks;

#define COL1 RGB(196, 64, 50)
#define COL2 RGB(227, 107, 35)
#define COL3 RGB(93, 160, 40)
#define COL4 RGB(69, 132, 190)
#define COL5 RGB(112, 115, 207)

static void DrawAppName(HDC hdc, Point pt, HFONT font) {
    const char* txt = kAppName;
    // colorful version
    COLORREF cols[] = {COL1, COL2, COL3, COL4, COL5, COL5, COL4, COL3, COL2, COL1};
    char buf[2] = {0};
    uint fmt = DT_LEFT | DT_NOCLIP;
    for (size_t i = 0; i < str::Len(txt); i++) {
        SetTextColor(hdc, cols[i % dimof(cols)]);
        Rect r{pt.x, pt.y, 1024, 1024};
        buf[0] = txt[i];
        HdcDrawText(hdc, buf, r, fmt, font);
        Size txtSize = HdcMeasureText(hdc, buf, fmt, font);
        pt.x += txtSize.dx;
    }
}

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

static Size CalcSumatraVersionSize(HDC hdc) {
    Size result{};

    HFONT fontSumatraTxt = CreateSimpleFont(hdc, kSumatraTxtFont, kSumatraTxtFontSize);
    HFONT fontVersionTxt = CreateSimpleFont(hdc, kVersionTxtFont, kVersionTxtFontSize);

    /* calculate minimal top box size */
    Size txtSize = HdcMeasureText(hdc, kAppName, fontSumatraTxt);
    result.dy = txtSize.dy + DpiScale(hdc, kAboutBoxMarginDy * 2);
    result.dx = txtSize.dx;

    /* consider version and version-sub strings */
    TempStr ver = GetAppVersionTemp();
    txtSize = HdcMeasureText(hdc, ver, fontVersionTxt);
    int minWidth = txtSize.dx + DpiScale(hdc, 8);
    int dx = std::max(txtSize.dx, minWidth);
    result.dx += 2 * (dx + DpiScale(hdc, kInnerPadding));
    return result;
}

static void DrawSumatraVersion(HWND hwnd, HDC hdc, Rect rect) {
    uint fmt = DT_LEFT | DT_NOCLIP;
    HFONT fontSumatraTxt = CreateSimpleFont(hdc, kSumatraTxtFont, kSumatraTxtFontSize);
    HFONT fontVersionTxt = CreateSimpleFont(hdc, kVersionTxtFont, kVersionTxtFontSize);

    SetBkMode(hdc, TRANSPARENT);

    const char* txt = kAppName;
    Size txtSize = HdcMeasureText(hdc, txt, fmt, fontSumatraTxt);
    Rect mainRect(rect.x + (rect.dx - txtSize.dx) / 2, rect.y + (rect.dy - txtSize.dy) / 2, txtSize.dx, txtSize.dy);
    DrawAppName(hdc, mainRect.TL(), fontSumatraTxt);

    SetTextColor(hdc, gCurrentTheme->window.textColor);
    int x = mainRect.x + mainRect.dx + DpiScale(hwnd, kInnerPadding);
    int y = mainRect.y;

    char* ver = GetAppVersionTemp();
    Rect r = {x, y, 1024, 1024};
    HdcDrawText(hdc, ver, r, fmt, fontVersionTxt);
    r.y += DpiScale(hwnd, 13);
    HdcDrawText(hdc, VERSION_SUB_TXT, r, fmt);
}

// draw on the bottom right
static Rect DrawHideFrequentlyReadLink(HWND hwnd, HDC hdc, const char* txt) {
    HFONT fontLeftTxt = CreateSimpleFont(hdc, "MS Shell Dlg", 16);

    HwndWidgetText w(txt, hwnd, fontLeftTxt);
    w.isRtl = IsUIRightToLeft();
    w.withUnderline = true;
    Size txtSize = w.Measure(true);

    auto col = gCurrentTheme->window.linkColor;
    ScopedSelectObject pen(hdc, CreatePen(PS_SOLID, 1, col), true);

    SetTextColor(hdc, col);
    SetBkMode(hdc, TRANSPARENT);
    Rect rc = ClientRect(hwnd);

    int innerPadding = DpiScale(hwnd, kInnerPadding);
    Rect r = {0, 0, txtSize.dx, txtSize.dy};
    PositionRB(rc, r);
    MoveXY(r, -innerPadding, -innerPadding);
    w.SetBounds(r);
    w.Draw(hdc);

    // make the click target larger
    r.Inflate(innerPadding, innerPadding);
    return r;
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
    auto col = gCurrentTheme->window.textColor;
    AutoDeletePen penBorder(CreatePen(PS_SOLID, ABOUT_LINE_OUTER_SIZE, col));
    AutoDeletePen penDivideLine(CreatePen(PS_SOLID, ABOUT_LINE_SEP_SIZE, col));
    col = gCurrentTheme->window.linkColor;
    AutoDeletePen penLinkLine(CreatePen(PS_SOLID, ABOUT_LINE_SEP_SIZE, col));

    HFONT fontLeftTxt = CreateSimpleFont(hdc, kLeftTextFont, kLeftTextFontSize);
    HFONT fontRightTxt = CreateSimpleFont(hdc, kRightTextFont, kRightTextFontSize);

    ScopedSelectObject font(hdc, fontLeftTxt); /* Just to remember the orig font */

    Rect rc = ClientRect(hwnd);
    col = GetMainWindowBackgroundColor();
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
    DrawSumatraVersion(hwnd, hdc, titleRect);

    /* render attribution box */
    col = gCurrentTheme->window.textColor;
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
        Rect r{pos.x, pos.y, 1024, 1024};
        HdcDrawText(hdc, el->leftTxt, r, fmt);
    }

    /* render text on the right */
    SelectObject(hdc, fontRightTxt);
    SelectObject(hdc, penLinkLine);
    DeleteVecMembers(staticLinks);
    for (AboutLayoutInfoEl* el = gAboutLayoutInfo; el->leftTxt; el++) {
        bool hasUrl = HasPermission(Perm::DiskAccess) && el->url;
        if (hasUrl) {
            col = gCurrentTheme->window.linkColor;
        } else {
            col = gCurrentTheme->window.textColor;
        }
        SetTextColor(hdc, col);
        char* s = (char*)el->rightTxt;
        s = TrimGitTemp(s);
        auto& pos = el->rightPos;
        Rect r{pos.x, pos.y, 1024, 1024};
        HdcDrawText(hdc, s, r, fmt);

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

    HGDIOBJ origFont = SelectObject(hdc, fontLeftTxt);

    /* calculate minimal top box size */
    Size headerSize = CalcSumatraVersionSize(hdc);

    /* calculate left text dimensions */
    SelectObject(hdc, fontLeftTxt);
    int leftLargestDx = 0;
    int leftDy = 0;
    uint fmt = DT_LEFT;
    for (AboutLayoutInfoEl* el = gAboutLayoutInfo; el->leftTxt; el++) {
        Size txtSize = HdcMeasureText(hdc, el->leftTxt, fmt);
        el->leftPos.dx = txtSize.dx;
        el->leftPos.dy = txtSize.dy;

        if (el == &gAboutLayoutInfo[0]) {
            leftDy = el->leftPos.dy;
        } else {
            CrashIf(leftDy != el->leftPos.dy);
        }
        if (leftLargestDx < el->leftPos.dx) {
            leftLargestDx = el->leftPos.dx;
        }
    }

    /* calculate right text dimensions */
    SelectObject(hdc, fontRightTxt);
    int rightLargestDx = 0;
    int rightDy = 0;
    for (AboutLayoutInfoEl* el = gAboutLayoutInfo; el->leftTxt; el++) {
        char* s = (char*)el->rightTxt;
        s = TrimGitTemp(s);
        Size txtSize = HdcMeasureText(hdc, s, fmt);
        el->rightPos.dx = txtSize.dx;
        el->rightPos.dy = txtSize.dy;

        if (el == &gAboutLayoutInfo[0]) {
            rightDy = el->rightPos.dy;
        } else {
            CrashIf(rightDy != el->rightPos.dy);
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

    SelectObject(hdc, origFont);
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
    str::Str info(512);
    char* ver = GetAppVersionTemp();
    info.AppendFmt("%s %s\r\n", kAppName, ver);
    for (size_t i = info.size() - 2; i > 0; i--) {
        info.AppendChar('-');
    }
    info.Append("\r\n");
    // concatenate all the information into a single string
    // (cf. CopyPropertiesToClipboard in SumatraProperties.cpp)
    size_t maxLen = 0;
    for (AboutLayoutInfoEl* el = gAboutLayoutInfo; el->leftTxt; el++) {
        maxLen = std::max(maxLen, str::Len(el->leftTxt));
    }
    for (AboutLayoutInfoEl* el = gAboutLayoutInfo; el->leftTxt; el++) {
        for (size_t i = maxLen - str::Len(el->leftTxt); i > 0; i--) {
            info.AppendChar(' ');
        }
        info.AppendFmt("%s: %s\r\n", el->leftTxt, el->url ? el->url : el->rightTxt);
    }
    CopyTextToClipboard(info.LendData());
}

char* GetStaticLinkTemp(Vec<StaticLinkInfo*>& staticLinks, int x, int y, StaticLinkInfo** linkOut) {
    if (!HasPermission(Perm::DiskAccess)) {
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
    TooltipCreateArgs args;
    args.parent = gHwndAbout;
    gAboutTooltip->Create(args);
    gAboutTooltip->SetSingle(linkInfo->infotip, linkInfo->rect, false);
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
            CrashIf(gHwndAbout);
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
            CrashIf(!gHwndAbout);
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
        CrashIf(!gAtomAbout);
    }

    const WCHAR* title = _TR("About SumatraPDF");
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

    SetRtl(gHwndAbout, IsUIRightToLeft());

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
#define kThumbsMarginTop DpiScale(hdc, 60)
#define kThumbsMarginBottom DpiScale(hdc, 40)
#define kThumbsSpaceBetweenX DpiScale(hdc, 30)
#define kThumbsSpaceBetweenY DpiScale(hdc, 50)
#define kThumbsBottomBoxDy DpiScale(hdc, 50)

struct ThumbnailLayot {
    Rect bThumb;
    Rect bIcon;
    Rect bName;
};

struct HomePageLayout {
    Rect bApp;
    Rect bAppVer;
    Rect bLine;

    Rect bFreqRead;

    Rect bOpenDocIcon;
    Rect bOpenDoc;

    Rect bHideFreqRead;
    Vec<ThumbnailLayot> thumbnails;
};

void LaoutHomePage(HDC hdc, Rect r, const FileHistory& fileHistory, HomePageLayout& l) {
    // TODO: write me
}

void DrawHomePage(MainWindow* win, HDC hdc, const FileHistory& fileHistory, COLORREF textColor, COLORREF backgroundColor) {
    HWND hwnd = win->hwndFrame;
    auto color = gCurrentTheme->window.textColor;
    AutoDeletePen penBorder(CreatePen(PS_SOLID, kThumbsSeparatorDy, color));
    AutoDeletePen penThumbBorder(CreatePen(PS_SOLID, kThumbsBorderDx, color));
    color = gCurrentTheme->window.linkColor;
    AutoDeletePen penLinkLine(CreatePen(PS_SOLID, 1, color));

    HFONT fontText = CreateSimpleFont(hdc, "MS Shell Dlg", 14);

    Rect rc = ClientRect(win->hwndCanvas);
    color = GetMainWindowBackgroundColor();
    FillRect(hdc, rc, color);

    ScopedSelectObject pen(hdc, penBorder);

    bool isRtl = IsUIRightToLeft();

    /* render title */
    Rect titleBox = Rect(Point(0, 0), CalcSumatraVersionSize(hdc));
    titleBox.x = rc.dx - titleBox.dx - 3;
    DrawSumatraVersion(win->hwndCanvas, hdc, titleBox);
    DrawLine(hdc, Rect(0, titleBox.dy, rc.dx, 0));

    /* render recent files list */
    SelectObject(hdc, penThumbBorder);
    SetBkMode(hdc, TRANSPARENT);
    color = gCurrentTheme->window.textColor;
    SetTextColor(hdc, color);

    rc.y += titleBox.dy;
    rc.dy -= titleBox.dy;
    color = GetMainWindowBackgroundColor();
    FillRect(hdc, rc, color);
    rc.dy -= kThumbsBottomBoxDy;

    Vec<FileState*> list;
    fileHistory.GetFrequencyOrder(list);

    int dx = (rc.dx - kThumbsMarginLeft - kThumbsMarginRight + kThumbsSpaceBetweenX) /
             (kThumbnailDx + kThumbsSpaceBetweenX);
    int thumbsCols = limitValue(dx, 1, kThumbsMaxCols);
    int dy = (rc.dy - kThumbsMarginTop - kThumbsMarginBottom + kThumbsSpaceBetweenY) /
             (kThumbnailDy + kThumbsSpaceBetweenY);
    int thumbsRows = std::min(dy, kFileHistoryMaxFrequent / thumbsCols);
    int x = rc.x + kThumbsMarginLeft +
            (rc.dx - thumbsCols * kThumbnailDx - (thumbsCols - 1) * kThumbsSpaceBetweenX - kThumbsMarginLeft -
             kThumbsMarginRight) /
                2;
    Point offset(x, rc.y + kThumbsMarginTop);
    if (offset.x < DpiScale(hwnd, kInnerPadding)) {
        offset.x = DpiScale(hwnd, kInnerPadding);
    } else if (list.size() == 0) {
        offset.x = kThumbsMarginLeft;
    }

    const char* txt = _TRA("Frequently Read");
    HFONT fontFrequentlyRead = CreateSimpleFont(hdc, "MS Shell Dlg", 24);
    HwndWidgetText freqRead(txt, hwnd, fontFrequentlyRead);
    freqRead.isRtl = isRtl;
    Size txtSize = freqRead.Measure(true);

    Rect headerRect(offset.x, rc.y + (kThumbsMarginTop - txtSize.dy) / 2, txtSize.dx, txtSize.dy);
    if (isRtl) {
        headerRect.x = rc.dx - offset.x - headerRect.dx;
    }
    freqRead.SetBounds(headerRect);
    freqRead.Draw(hdc);

    SelectObject(hdc, GetStockBrush(NULL_BRUSH));

    DeleteVecMembers(win->staticLinks);
    for (int row = 0; row < thumbsRows; row++) {
        for (int col = 0; col < thumbsCols; col++) {
            if (row * thumbsCols + col >= list.isize()) {
                // display the "Open a document" link right below the last row
                thumbsRows = col > 0 ? row + 1 : row;
                break;
            }
            FileState* state = list.at(row * thumbsCols + col);

            Rect page(offset.x + col * (kThumbnailDx + kThumbsSpaceBetweenX),
                      offset.y + row * (kThumbnailDy + kThumbsSpaceBetweenY), kThumbnailDx, kThumbnailDy);
            if (isRtl) {
                page.x = rc.dx - page.x - page.dx;
            }
            bool loadOk = true;
            if (!state->thumbnail) {
                loadOk = LoadThumbnail(state);
            }
            if (loadOk && state->thumbnail) {
                Size thumbSize = state->thumbnail->GetSize();
                if (thumbSize.dx != kThumbnailDx || thumbSize.dy != kThumbnailDy) {
                    page.dy = thumbSize.dy * kThumbnailDx / thumbSize.dx;
                    page.y += kThumbnailDy - page.dy;
                }
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
                    state->thumbnail->Blit(hdc, page);
                }
                SelectClipRgn(hdc, nullptr);
                DeleteObject(clip);
            }
            RoundRect(hdc, page.x, page.y, page.x + page.dx, page.y + page.dy, 10, 10);

            int iconSpace = DpiScale(win->hwndFrame, 20);
            Rect rect(page.x + iconSpace, page.y + page.dy + 3, page.dx - iconSpace, iconSpace);
            if (isRtl) {
                rect.x -= iconSpace;
            }
            char* path = state->filePath;
            const char* fileName = path::GetBaseNameTemp(path);
            UINT fmt = DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX | (isRtl ? DT_RIGHT : DT_LEFT);
            HdcDrawText(hdc, fileName, rect, fmt, fontText);

            SHFILEINFO sfi = {nullptr};
            uint flags = SHGFI_SYSICONINDEX | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES;
            WCHAR* filePathW = ToWStrTemp(path);
            HIMAGELIST himl = (HIMAGELIST)SHGetFileInfoW(filePathW, 0, &sfi, sizeof(sfi), flags);
            x = isRtl ? page.x + page.dx - DpiScale(win->hwndFrame, 16) : page.x;
            ImageList_Draw(himl, sfi.iIcon, hdc, x, rect.y, ILD_TRANSPARENT);

            auto sl = new StaticLinkInfo(rect.Union(page), path, path);
            win->staticLinks.Append(sl);
        }
    }

    /* render bottom links */
    rc.y += kThumbsMarginTop + thumbsRows * kThumbnailDy + (thumbsRows - 1) * kThumbsSpaceBetweenY + kThumbsMarginBottom;
    rc.dy = kThumbsBottomBoxDy;

    color = gCurrentTheme->window.linkColor;
    SetTextColor(hdc, color);
    SelectObject(hdc, penLinkLine);

    HIMAGELIST himl = (HIMAGELIST)SendMessageW(win->hwndToolbar, TB_GETIMAGELIST, 0, 0);
    Rect rectIcon(offset.x, rc.y, 0, 0);
    ImageList_GetIconSize(himl, &rectIcon.dx, &rectIcon.dy);
    rectIcon.y += (rc.dy - rectIcon.dy) / 2;
    if (isRtl) {
        rectIcon.x = rc.dx - offset.x - rectIcon.dx;
    }
    ImageList_Draw(himl, 0 /* index of Open icon */, hdc, rectIcon.x, rectIcon.y, ILD_NORMAL);

    txt = _TRA("Open a document...");
    HwndWidgetText openDoc(txt, hwnd, fontText);
    openDoc.isRtl = isRtl;
    openDoc.withUnderline = true;
    txtSize = openDoc.Measure(true);
    Rect rect(offset.x + rectIcon.dx + 3, rc.y + (rc.dy - txtSize.dy) / 2, txtSize.dx, txtSize.dy);
    if (isRtl) {
        rect.x = rectIcon.x - rect.dx - 3;
    }
    openDoc.SetBounds(rect);
    openDoc.Draw(hdc);

    // make the click target larger
    rect = rect.Union(rectIcon);
    rect.Inflate(10, 10);
    auto sl = new StaticLinkInfo(rect, kLinkOpenFile);
    win->staticLinks.Append(sl);

    rect = DrawHideFrequentlyReadLink(win->hwndCanvas, hdc, _TRA("Hide frequently read"));
    sl = new StaticLinkInfo(rect, kLinkHideList);
    win->staticLinks.Append(sl);
}
