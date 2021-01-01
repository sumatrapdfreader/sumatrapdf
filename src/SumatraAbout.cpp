/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/Dpi.h"
#include "utils/FileUtil.h"
#include "utils/WinUtil.h"

#include "wingui/WinGui.h"
#include "wingui/TreeModel.h"
#include "wingui/Layout.h"
#include "wingui/Window.h"
#include "wingui/TooltipCtrl.h"

#include "SumatraConfig.h"
#include "Annotation.h"
#include "EngineBase.h"
#include "DisplayMode.h"
#include "SettingsStructs.h"
#include "FileHistory.h"
#include "AppColors.h"
#include "GlobalPrefs.h"
#include "ProgressUpdateUI.h"
#include "Notifications.h"
#include "SumatraPDF.h"
#include "SumatraConfig.h"
#include "WindowInfo.h"
#include "resource.h"
#include "Commands.h"
#include "FileThumbnails.h"
#include "SumatraAbout.h"
#include "Translations.h"
#include "Version.h"

#ifndef ABOUT_USE_LESS_COLORS
#define ABOUT_LINE_OUTER_SIZE 2
#else
#define ABOUT_LINE_OUTER_SIZE 1
#endif
#define ABOUT_LINE_SEP_SIZE 1
#define ABOUT_LEFT_RIGHT_SPACE_DX DpiScale(8)
#define ABOUT_MARGIN_DX DpiScale(10)
#define ABOUT_BOX_MARGIN_DY DpiScale(6)
#define ABOUT_BORDER_COL RGB(0, 0, 0)
#define ABOUT_TXT_DY DpiScale(6)
#define ABOUT_RECT_PADDING DpiScale(8)
#define ABOUT_INNER_PADDING DpiScale(8)

#define ABOUT_CLASS_NAME L"SUMATRA_PDF_ABOUT"

#define ABOUT_WIN_TITLE _TR("About SumatraPDF")

#define SUMATRA_TXT_FONT L"Arial Black"
#define SUMATRA_TXT_FONT_SIZE 24

#define VERSION_TXT_FONT L"Arial Black"
#define VERSION_TXT_FONT_SIZE 12

#define VERSION_TXT L"v" CURR_VERSION_STR
#ifdef PRE_RELEASE_VER
#define VERSION_SUB_TXT L"Pre-release"
#else
#define VERSION_SUB_TXT L""
#endif

#ifdef GIT_COMMIT_ID
#define GIT_COMMIT_ID_STR TEXT(QM(GIT_COMMIT_ID))
#endif

#define URL_LICENSE L"https://github.com/sumatrapdfreader/sumatrapdf/blob/master/AUTHORS"
#define URL_AUTHORS L"https://github.com/sumatrapdfreader/sumatrapdf/blob/master/AUTHORS"
#define URL_TRANSLATORS L"https://github.com/sumatrapdfreader/sumatrapdf/blob/master/TRANSLATORS"
#define URL_SUPPORT_SUMATRA L"https://www.sumatrapdfreader.org/backers.html"

#define LAYOUT_LTR 0

static ATOM gAtomAbout;
static HWND gHwndAbout;
static TooltipCtrl* gAboutTooltip = nullptr;
static const WCHAR* gClickedURL = nullptr;

struct AboutLayoutInfoEl {
    /* static data, must be provided */
    const WCHAR* leftTxt;
    const WCHAR* rightTxt;
    const WCHAR* url;

    /* data calculated by the layout */
    Rect leftPos;
    Rect rightPos;
};

static AboutLayoutInfoEl gAboutLayoutInfo[] = {
    {L"website", L"SumatraPDF website", WEBSITE_MAIN_URL},
    {L"manual", L"SumatraPDF manual", WEBSITE_MANUAL_URL},
    {L"forums", L"SumatraPDF forums", L"https://forum.sumatrapdfreader.org/"},
    {L"programming", L"The Programmers", URL_AUTHORS},
    {L"translations", L"The Translators", URL_TRANSLATORS},
    {L"licenses", L"Various Open Source", URL_LICENSE},
#ifdef GIT_COMMIT_ID
    // TODO: use short ID for rightTxt (only first 7 digits) with less hackery
    {L"last change", L"git commit " GIT_COMMIT_ID_STR,
     L"https://github.com/sumatrapdfreader/sumatrapdf/commit/" GIT_COMMIT_ID_STR},
#endif
#ifdef PRE_RELEASE_VER
    {L"a note", L"Pre-release version, for testing only!", nullptr},
#endif
#ifdef DEBUG
    {L"a note", L"Debug version, for testing only!", nullptr},
#endif
    {nullptr, nullptr, nullptr}};

static Vec<StaticLinkInfo> gLinkInfo;

#define COL1 RGB(196, 64, 50)
#define COL2 RGB(227, 107, 35)
#define COL3 RGB(93, 160, 40)
#define COL4 RGB(69, 132, 190)
#define COL5 RGB(112, 115, 207)

static void DrawAppName(HDC hdc, Point pt) {
    const WCHAR* txt = GetAppName();
    if (gIsRaMicroBuild) {
        // simple black-ish version
        COLORREF col = RGB(0x43, 0x43, 0x43);
        SetTextColor(hdc, col);
        TextOutW(hdc, pt.x, pt.y, txt, (int)str::Len(txt));
        return;
    }

    // colorful version
    COLORREF cols[] = {COL1, COL2, COL3, COL4, COL5, COL5, COL4, COL3, COL2, COL1};
    for (size_t i = 0; i < str::Len(txt); i++) {
        SetTextColor(hdc, cols[i % dimof(cols)]);
        TextOutW(hdc, pt.x, pt.y, txt + i, 1);

        SIZE txtSize;
        GetTextExtentPoint32(hdc, txt + i, 1, &txtSize);
        pt.x += txtSize.cx;
    }
}

static WCHAR* GetAppVersion() {
    str::WStr s;
    s.Set(VERSION_TXT);
    if (IsProcess64()) {
        s.Append(L" 64-bit");
    }
#ifdef DEBUG
    s.Append(L" (dbg)");
#endif
    return s.StealData();
}

static Size CalcSumatraVersionSize(HWND hwnd, HDC hdc) {
    Size result{};

    AutoDeleteFont fontSumatraTxt(CreateSimpleFont(hdc, SUMATRA_TXT_FONT, SUMATRA_TXT_FONT_SIZE));
    AutoDeleteFont fontVersionTxt(CreateSimpleFont(hdc, VERSION_TXT_FONT, VERSION_TXT_FONT_SIZE));
    ScopedSelectObject selFont(hdc, fontSumatraTxt);

    SIZE txtSize{};
    /* calculate minimal top box size */
    const WCHAR* txt = GetAppName();

    GetTextExtentPoint32(hdc, txt, (int)str::Len(txt), &txtSize);
    result.dy = txtSize.cy + DpiScale(hwnd, ABOUT_BOX_MARGIN_DY * 2);
    result.dx = txtSize.cx;

    /* consider version and version-sub strings */
    SelectObject(hdc, fontVersionTxt);
    AutoFreeWstr ver = GetAppVersion();
    GetTextExtentPoint32(hdc, ver.Get(), (int)str::Len(ver.Get()), &txtSize);
    LONG minWidth = txtSize.cx + DpiScale(hwnd, 8);
    txt = VERSION_SUB_TXT;
    GetTextExtentPoint32(hdc, txt, (int)str::Len(txt), &txtSize);
    txtSize.cx = std::max(txtSize.cx, minWidth);
    result.dx += 2 * (txtSize.cx + ABOUT_INNER_PADDING);

    return result;
}

static void DrawSumatraVersion(HWND hwnd, HDC hdc, Rect rect) {
    AutoDeleteFont fontSumatraTxt(CreateSimpleFont(hdc, SUMATRA_TXT_FONT, SUMATRA_TXT_FONT_SIZE));
    AutoDeleteFont fontVersionTxt(CreateSimpleFont(hdc, VERSION_TXT_FONT, VERSION_TXT_FONT_SIZE));
    HGDIOBJ oldFont = SelectObject(hdc, fontSumatraTxt);

    SetBkMode(hdc, TRANSPARENT);

    SIZE txtSize;
    const WCHAR* txt = GetAppName();
    GetTextExtentPoint32(hdc, txt, (int)str::Len(txt), &txtSize);
    Rect mainRect(rect.x + (rect.dx - txtSize.cx) / 2, rect.y + (rect.dy - txtSize.cy) / 2, txtSize.cx, txtSize.cy);
    DrawAppName(hdc, mainRect.TL());

    SetTextColor(hdc, WIN_COL_BLACK);
    SelectObject(hdc, fontVersionTxt);
    Point pt(mainRect.x + mainRect.dx + ABOUT_INNER_PADDING, mainRect.y);

    AutoFreeWstr ver = GetAppVersion();
    TextOut(hdc, pt.x, pt.y, ver.Get(), (int)str::Len(ver.Get()));
    txt = VERSION_SUB_TXT;
    TextOut(hdc, pt.x, pt.y + DpiScale(hwnd, 13), txt, (int)str::Len(txt));

    SelectObject(hdc, oldFont);
}

// draw on the bottom right
static Rect DrawHideFrequentlyReadLink(HWND hwnd, HDC hdc, const WCHAR* txt) {
    AutoDeleteFont fontLeftTxt(CreateSimpleFont(hdc, L"MS Shell Dlg", 16));
    auto col = GetAppColor(AppColor::MainWindowLink);
    AutoDeletePen penLinkLine(CreatePen(PS_SOLID, 1, col));
    ScopedSelectObject font(hdc, fontLeftTxt);

    SetTextColor(hdc, col);
    SetBkMode(hdc, TRANSPARENT);
    Rect rc = ClientRect(hwnd);

    SIZE txtSize;
    GetTextExtentPoint32(hdc, txt, (int)str::Len(txt), &txtSize);
    int x = rc.dx - txtSize.cx - ABOUT_INNER_PADDING;
    int y = rc.y + rc.dy - txtSize.cy - ABOUT_INNER_PADDING;
    Rect rect(x, y, txtSize.cx, txtSize.cy);
    RECT rTmp = ToRECT(rect);
    DrawText(hdc, txt, -1, &rTmp, IsUIRightToLeft() ? DT_RTLREADING : DT_LEFT);
    {
        ScopedSelectObject pen(hdc, penLinkLine);
        PaintLine(hdc, Rect(rect.x, rect.y + rect.dy, rect.dx, 0));
    }

    // make the click target larger
    rect.Inflate(ABOUT_INNER_PADDING, ABOUT_INNER_PADDING);
    return rect;
}

// draw on the bottom left
static Rect DrawSupportLink(HWND hwnd, HDC hdc, const WCHAR* txt) {
    AutoDeleteFont fontLeftTxt(CreateSimpleFont(hdc, L"MS Shell Dlg", 16));
    auto col = GetAppColor(AppColor::MainWindowLink);
    AutoDeletePen penLinkLine(CreatePen(PS_SOLID, 1, col));
    ScopedSelectObject font(hdc, fontLeftTxt);

    SetTextColor(hdc, col);
    SetBkMode(hdc, TRANSPARENT);
    Rect rc = ClientRect(hwnd);

    SIZE txtSize;
    GetTextExtentPoint32(hdc, txt, (int)str::Len(txt), &txtSize);
    int y = rc.y + rc.dy - txtSize.cy - ABOUT_INNER_PADDING;
    Rect rect(ABOUT_INNER_PADDING, y, txtSize.cx, txtSize.cy);

    RECT rTmp = ToRECT(rect);
    DrawText(hdc, txt, -1, &rTmp, IsUIRightToLeft() ? DT_RTLREADING : DT_LEFT);
    {
        ScopedSelectObject pen(hdc, penLinkLine);
        PaintLine(hdc, Rect(rect.x, rect.y + rect.dy, rect.dx, 0));
    }

    // make the click target larger
    rect.Inflate(ABOUT_INNER_PADDING, ABOUT_INNER_PADDING);
    return rect;
}

/* Draws the about screen and remembers some state for hyperlinking.
   It transcribes the design I did in graphics software - hopeless
   to understand without seeing the design. */
static void DrawAbout(HWND hwnd, HDC hdc, Rect rect, Vec<StaticLinkInfo>& linkInfo) {
    auto col = GetAppColor(AppColor::MainWindowText);
    AutoDeletePen penBorder(CreatePen(PS_SOLID, ABOUT_LINE_OUTER_SIZE, col));
    AutoDeletePen penDivideLine(CreatePen(PS_SOLID, ABOUT_LINE_SEP_SIZE, col));
    col = GetAppColor(AppColor::MainWindowLink);
    AutoDeletePen penLinkLine(CreatePen(PS_SOLID, ABOUT_LINE_SEP_SIZE, col));

    AutoDeleteFont fontLeftTxt(CreateSimpleFont(hdc, LEFT_TXT_FONT, LEFT_TXT_FONT_SIZE));
    AutoDeleteFont fontRightTxt(CreateSimpleFont(hdc, RIGHT_TXT_FONT, RIGHT_TXT_FONT_SIZE));

    ScopedSelectObject font(hdc, fontLeftTxt); /* Just to remember the orig font */

    Rect rc = ClientRect(hwnd);
    RECT rTmp = ToRECT(rc);
    col = GetAppColor(AppColor::MainWindowBg);
    ScopedGdiObj<HBRUSH> brushAboutBg(CreateSolidBrush(col));
    FillRect(hdc, &rTmp, brushAboutBg);

    /* render title */
    Rect titleRect(rect.TL(), CalcSumatraVersionSize(hwnd, hdc));

    AutoDeleteBrush bgBrush(CreateSolidBrush(col));
    ScopedSelectObject brush(hdc, bgBrush);
    ScopedSelectObject pen(hdc, penBorder);
#ifndef ABOUT_USE_LESS_COLORS
    Rectangle(hdc, rect.x, rect.y + ABOUT_LINE_OUTER_SIZE, rect.x + rect.dx,
              rect.y + titleRect.dy + ABOUT_LINE_OUTER_SIZE);
#else
    Rect titleBgBand(0, rect.y, rc.dx, titleRect.dy);
    RECT rcLogoBg = titleBgBand.ToRECT();
    FillRect(hdc, &rcLogoBg, bgBrush);
    PaintLine(hdc, Rect(0, rect.y, rc.dx, 0));
    PaintLine(hdc, Rect(0, rect.y + titleRect.dy, rc.dx, 0));
#endif

    titleRect.Offset((rect.dx - titleRect.dx) / 2, 0);
    DrawSumatraVersion(hwnd, hdc, titleRect);

    /* render attribution box */
    col = GetAppColor(AppColor::MainWindowText);
    SetTextColor(hdc, col);
    SetBkMode(hdc, TRANSPARENT);

#ifndef ABOUT_USE_LESS_COLORS
    Rectangle(hdc, rect.x, rect.y + titleRect.dy, rect.x + rect.dx, rect.y + rect.dy);
#endif

    /* render text on the left*/
    SelectObject(hdc, fontLeftTxt);
    for (AboutLayoutInfoEl* el = gAboutLayoutInfo; el->leftTxt; el++) {
        TextOut(hdc, el->leftPos.x, el->leftPos.y, el->leftTxt, (int)str::Len(el->leftTxt));
    }

    /* render text on the right */
    SelectObject(hdc, fontRightTxt);
    SelectObject(hdc, penLinkLine);
    linkInfo.Reset();
    for (AboutLayoutInfoEl* el = gAboutLayoutInfo; el->leftTxt; el++) {
        bool hasUrl = HasPermission(Perm_DiskAccess) && el->url;
        if (hasUrl) {
            col = GetAppColor(AppColor::MainWindowLink);
        } else {
            col = GetAppColor(AppColor::MainWindowText);
        }
        SetTextColor(hdc, col);
        size_t txtLen = str::Len(el->rightTxt);
#ifdef GIT_COMMIT_ID
        if (str::EndsWith(el->rightTxt, GIT_COMMIT_ID_STR))
            txtLen -= str::Len(GIT_COMMIT_ID_STR) - 7;
#endif
        TextOutW(hdc, el->rightPos.x, el->rightPos.y, el->rightTxt, (int)txtLen);

        if (hasUrl) {
            int underlineY = el->rightPos.y + el->rightPos.dy - 3;
            PaintLine(hdc, Rect(el->rightPos.x, underlineY, el->rightPos.dx, 0));
            linkInfo.Append(StaticLinkInfo(el->rightPos, el->url, el->url));
        }
    }

    SelectObject(hdc, penDivideLine);
    Rect divideLine(gAboutLayoutInfo[0].rightPos.x - ABOUT_LEFT_RIGHT_SPACE_DX, rect.y + titleRect.dy + 4, 0,
                    rect.y + rect.dy - 4 - gAboutLayoutInfo[0].rightPos.y);
    PaintLine(hdc, divideLine);
}

static void UpdateAboutLayoutInfo(HWND hwnd, HDC hdc, Rect* rect) {
    AutoDeleteFont fontLeftTxt(CreateSimpleFont(hdc, LEFT_TXT_FONT, LEFT_TXT_FONT_SIZE));
    AutoDeleteFont fontRightTxt(CreateSimpleFont(hdc, RIGHT_TXT_FONT, RIGHT_TXT_FONT_SIZE));

    HGDIOBJ origFont = SelectObject(hdc, fontLeftTxt);

    /* calculate minimal top box size */
    Size headerSize = CalcSumatraVersionSize(hwnd, hdc);

    /* calculate left text dimensions */
    SelectObject(hdc, fontLeftTxt);
    int leftLargestDx = 0;
    int leftDy = 0;
    for (AboutLayoutInfoEl* el = gAboutLayoutInfo; el->leftTxt; el++) {
        SIZE txtSize;
        GetTextExtentPoint32(hdc, el->leftTxt, (int)str::Len(el->leftTxt), &txtSize);
        el->leftPos.dx = txtSize.cx;
        el->leftPos.dy = txtSize.cy;

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
        SIZE txtSize;
        size_t txtLen = str::Len(el->rightTxt);
#ifdef GIT_COMMIT_ID
        if (str::EndsWith(el->rightTxt, GIT_COMMIT_ID_STR)) {
            txtLen -= str::Len(GIT_COMMIT_ID_STR) - 7;
        }
#endif
        GetTextExtentPoint32W(hdc, el->rightTxt, (int)txtLen, &txtSize);
        el->rightPos.dx = txtSize.cx;
        el->rightPos.dy = txtSize.cy;

        if (el == &gAboutLayoutInfo[0]) {
            rightDy = el->rightPos.dy;
        } else {
            CrashIf(rightDy != el->rightPos.dy);
        }
        if (rightLargestDx < el->rightPos.dx) {
            rightLargestDx = el->rightPos.dx;
        }
    }

    /* calculate total dimension and position */
    Rect minRect;
    minRect.dx =
        ABOUT_LEFT_RIGHT_SPACE_DX + leftLargestDx + ABOUT_LINE_SEP_SIZE + rightLargestDx + ABOUT_LEFT_RIGHT_SPACE_DX;
    if (minRect.dx < headerSize.dx) {
        minRect.dx = headerSize.dx;
    }
    minRect.dx += 2 * ABOUT_LINE_OUTER_SIZE + 2 * ABOUT_MARGIN_DX;

    minRect.dy = headerSize.dy;
    for (AboutLayoutInfoEl* el = gAboutLayoutInfo; el->leftTxt; el++) {
        minRect.dy += rightDy + ABOUT_TXT_DY;
    }
    minRect.dy += 2 * ABOUT_LINE_OUTER_SIZE + 4;

    Rect rc = ClientRect(hwnd);
    minRect.x = (rc.dx - minRect.dx) / 2;
    minRect.y = (rc.dy - minRect.dy) / 2;

    if (rect) {
        *rect = minRect;
    }

    /* calculate text positions */
    int linePosX = ABOUT_LINE_OUTER_SIZE + ABOUT_MARGIN_DX + leftLargestDx + ABOUT_LEFT_RIGHT_SPACE_DX;
    int currY = minRect.y + headerSize.dy + 4;
    for (AboutLayoutInfoEl* el = gAboutLayoutInfo; el->leftTxt; el++) {
        el->leftPos.x = minRect.x + linePosX - ABOUT_LEFT_RIGHT_SPACE_DX - el->leftPos.dx;
        el->leftPos.y = currY + (rightDy - leftDy) / 2;
        el->rightPos.x = minRect.x + linePosX + ABOUT_LEFT_RIGHT_SPACE_DX;
        el->rightPos.y = currY;
        currY += rightDy + ABOUT_TXT_DY;
    }

    SelectObject(hdc, origFont);
}

static void OnPaintAbout(HWND hwnd) {
    PAINTSTRUCT ps;
    Rect rc;
    HDC hdc = BeginPaint(hwnd, &ps);
    SetLayout(hdc, LAYOUT_LTR);
    UpdateAboutLayoutInfo(hwnd, hdc, &rc);
    DrawAbout(hwnd, hdc, rc, gLinkInfo);
    EndPaint(hwnd, &ps);
}

static void CopyAboutInfoToClipboard([[maybe_unused]] HWND hwnd) {
    str::WStr info(512);
    AutoFreeWstr ver = GetAppVersion();
    info.AppendFmt(L"%s %s\r\n", GetAppName(), ver.Get());
    for (size_t i = info.size() - 2; i > 0; i--) {
        info.Append('-');
    }
    info.Append(L"\r\n");
    // concatenate all the information into a single string
    // (cf. CopyPropertiesToClipboard in SumatraProperties.cpp)
    size_t maxLen = 0;
    for (AboutLayoutInfoEl* el = gAboutLayoutInfo; el->leftTxt; el++) {
        maxLen = std::max(maxLen, str::Len(el->leftTxt));
    }
    for (AboutLayoutInfoEl* el = gAboutLayoutInfo; el->leftTxt; el++) {
        for (size_t i = maxLen - str::Len(el->leftTxt); i > 0; i--) {
            info.Append(' ');
        }
        info.AppendFmt(L"%s: %s\r\n", el->leftTxt, el->url ? el->url : el->rightTxt);
    }
    CopyTextToClipboard(info.LendData());
}

const WCHAR* GetStaticLink(Vec<StaticLinkInfo>& linkInfo, int x, int y, StaticLinkInfo* info) {
    if (!HasPermission(Perm_DiskAccess)) {
        return nullptr;
    }

    Point pt(x, y);
    for (size_t i = 0; i < linkInfo.size(); i++) {
        if (linkInfo.at(i).rect.Contains(pt)) {
            if (info) {
                *info = linkInfo.at(i);
            }
            return linkInfo.at(i).target;
        }
    }

    return nullptr;
}

static void CreateInfotipForLink(StaticLinkInfo& linkInfo) {
    if (gAboutTooltip != nullptr) {
        return;
    }

    gAboutTooltip = new TooltipCtrl(gHwndAbout);
    gAboutTooltip->Create();
    gAboutTooltip->Show(linkInfo.infotip, linkInfo.rect, false);
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
    const WCHAR* url;
    Point pt;

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
            if (GetCursorPosInHwnd(hwnd, pt)) {
                StaticLinkInfo linkInfo;
                if (GetStaticLink(gLinkInfo, pt.x, pt.y, &linkInfo)) {
                    CreateInfotipForLink(linkInfo);
                    SetCursorCached(IDC_HAND);
                    return TRUE;
                }
            }
            DeleteInfotip();
            return DefWindowProc(hwnd, msg, wp, lp);

        case WM_LBUTTONDOWN:
            gClickedURL = GetStaticLink(gLinkInfo, GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
            break;

        case WM_LBUTTONUP:
            url = GetStaticLink(gLinkInfo, GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
            if (url && url == gClickedURL) {
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
                CopyAboutInfoToClipboard(hwnd);
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

void OnMenuAbout() {
    if (gHwndAbout) {
        SetActiveWindow(gHwndAbout);
        return;
    }

    if (!gAtomAbout) {
        WNDCLASSEX wcex;
        FillWndClassEx(wcex, ABOUT_CLASS_NAME, WndProcAbout);
        HMODULE h = GetModuleHandleW(nullptr);
        wcex.hIcon = LoadIcon(h, MAKEINTRESOURCE(GetAppIconID()));
        gAtomAbout = RegisterClassEx(&wcex);
        CrashIf(!gAtomAbout);
    }

    gHwndAbout =
        CreateWindow(ABOUT_CLASS_NAME, ABOUT_WIN_TITLE, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, CW_USEDEFAULT,
                     CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
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
    rc.Inflate(ABOUT_RECT_PADDING, ABOUT_RECT_PADDING);

    // resize the new window to just match these dimensions
    Rect wRc = WindowRect(gHwndAbout);
    Rect cRc = ClientRect(gHwndAbout);
    wRc.dx += rc.dx - cRc.dx;
    wRc.dy += rc.dy - cRc.dy;
    MoveWindow(gHwndAbout, wRc.x, wRc.y, wRc.dx, wRc.dy, FALSE);

    ShowWindow(gHwndAbout, SW_SHOW);
}

void DrawAboutPage(WindowInfo* win, HDC hdc) {
    Rect rc = ClientRect(win->hwndCanvas);
    UpdateAboutLayoutInfo(win->hwndCanvas, hdc, &rc);
    DrawAbout(win->hwndCanvas, hdc, rc, win->staticLinks);
    if (HasPermission(Perm_SavePreferences | Perm_DiskAccess) && gGlobalPrefs->rememberOpenedFiles) {
        Rect rect = DrawHideFrequentlyReadLink(win->hwndCanvas, hdc, _TR("Show frequently read"));
        win->staticLinks.Append(StaticLinkInfo(rect, SLINK_LIST_SHOW));
    }
}

/* alternate static page to display when no document is loaded */

#define DOCLIST_SEPARATOR_DY 2
#define DOCLIST_THUMBNAIL_BORDER_W 1
#define DOCLIST_MARGIN_LEFT DpiScale(win->hwndFrame, 40)
#define DOCLIST_MARGIN_BETWEEN_X DpiScale(win->hwndFrame, 30)
#define DOCLIST_MARGIN_RIGHT DpiScale(win->hwndFrame, 40)
#define DOCLIST_MARGIN_TOP DpiScale(win->hwndFrame, 60)
#define DOCLIST_MARGIN_BETWEEN_Y DpiScale(win->hwndFrame, 50)
#define DOCLIST_MARGIN_BOTTOM DpiScale(win->hwndFrame, 40)
#define DOCLIST_MAX_THUMBNAILS_X 5
#define DOCLIST_BOTTOM_BOX_DY DpiScale(win->hwndFrame, 50)

void DrawStartPage(WindowInfo* win, HDC hdc, FileHistory& fileHistory, COLORREF textColor, COLORREF backgroundColor) {
    auto col = GetAppColor(AppColor::MainWindowText);
    AutoDeletePen penBorder(CreatePen(PS_SOLID, DOCLIST_SEPARATOR_DY, col));
    AutoDeletePen penThumbBorder(CreatePen(PS_SOLID, DOCLIST_THUMBNAIL_BORDER_W, col));
    col = GetAppColor(AppColor::MainWindowLink);
    AutoDeletePen penLinkLine(CreatePen(PS_SOLID, 1, col));

    AutoDeleteFont fontSumatraTxt(CreateSimpleFont(hdc, L"MS Shell Dlg", 24));
    int fontSize = 24;
    if (gIsRaMicroBuild) {
        fontSize = 20;
    }
    AutoDeleteFont fontFrequentlyRead(CreateSimpleFont(hdc, L"MS Shell Dlg", fontSize));
    AutoDeleteFont fontLeftTxt(CreateSimpleFont(hdc, L"MS Shell Dlg", 14));

    ScopedSelectObject font(hdc, fontSumatraTxt);

    Rect rc = ClientRect(win->hwndCanvas);
    RECT rTmp = ToRECT(rc);
    col = GetAppColor(AppColor::MainWindowBg);
    AutoDeleteBrush brushLogoBg(CreateSolidBrush(col));
    FillRect(hdc, &rTmp, brushLogoBg);

    ScopedSelectObject brush(hdc, brushLogoBg);
    ScopedSelectObject pen(hdc, penBorder);

    bool isRtl = IsUIRightToLeft();

    /* render title */
    Rect titleBox = Rect(Point(0, 0), CalcSumatraVersionSize(win->hwndCanvas, hdc));
    titleBox.x = rc.dx - titleBox.dx - 3;
    DrawSumatraVersion(win->hwndCanvas, hdc, titleBox);
    PaintLine(hdc, Rect(0, titleBox.dy, rc.dx, 0));

    /* render recent files list */
    SelectObject(hdc, penThumbBorder);
    SetBkMode(hdc, TRANSPARENT);
    col = GetAppColor(AppColor::MainWindowText);
    SetTextColor(hdc, col);

    rc.y += titleBox.dy;
    rc.dy -= titleBox.dy;
    rTmp = ToRECT(rc);
    col = GetAppColor(AppColor::MainWindowBg);
    ScopedGdiObj<HBRUSH> brushAboutBg(CreateSolidBrush(col));
    FillRect(hdc, &rTmp, brushAboutBg);
    rc.dy -= DOCLIST_BOTTOM_BOX_DY;

    Vec<DisplayState*> list;
    fileHistory.GetFrequencyOrder(list);

    int dx = (rc.dx - DOCLIST_MARGIN_LEFT - DOCLIST_MARGIN_RIGHT + DOCLIST_MARGIN_BETWEEN_X) /
             (THUMBNAIL_DX + DOCLIST_MARGIN_BETWEEN_X);
    int width = limitValue(dx, 1, DOCLIST_MAX_THUMBNAILS_X);
    int dy = (rc.dy - DOCLIST_MARGIN_TOP - DOCLIST_MARGIN_BOTTOM + DOCLIST_MARGIN_BETWEEN_Y) /
             (THUMBNAIL_DY + DOCLIST_MARGIN_BETWEEN_Y);
    int height = std::min(dy, FILE_HISTORY_MAX_FREQUENT / width);
    int x = rc.x + DOCLIST_MARGIN_LEFT +
            (rc.dx - width * THUMBNAIL_DX - (width - 1) * DOCLIST_MARGIN_BETWEEN_X - DOCLIST_MARGIN_LEFT -
             DOCLIST_MARGIN_RIGHT) /
                2;
    Point offset(x, rc.y + DOCLIST_MARGIN_TOP);
    if (offset.x < ABOUT_INNER_PADDING) {
        offset.x = ABOUT_INNER_PADDING;
    } else if (list.size() == 0) {
        offset.x = DOCLIST_MARGIN_LEFT;
    }

    SelectObject(hdc, fontFrequentlyRead);
    SIZE txtSize;
    const WCHAR* txt = _TR("Frequently Read");
    GetTextExtentPoint32(hdc, txt, (int)str::Len(txt), &txtSize);
    Rect headerRect(offset.x, rc.y + (DOCLIST_MARGIN_TOP - txtSize.cy) / 2, txtSize.cx, txtSize.cy);
    if (isRtl) {
        headerRect.x = rc.dx - offset.x - headerRect.dx;
    }
    rTmp = ToRECT(headerRect);
    DrawText(hdc, txt, -1, &rTmp, (isRtl ? DT_RTLREADING : DT_LEFT) | DT_NOPREFIX);

    SelectObject(hdc, fontLeftTxt);
    SelectObject(hdc, GetStockBrush(NULL_BRUSH));

    win->staticLinks.Reset();
    for (int h = 0; h < height; h++) {
        for (int w = 0; w < width; w++) {
            if (h * width + w >= (int)list.size()) {
                // display the "Open a document" link right below the last row
                height = w > 0 ? h + 1 : h;
                break;
            }
            DisplayState* state = list.at(h * width + w);

            Rect page(offset.x + w * (THUMBNAIL_DX + DOCLIST_MARGIN_BETWEEN_X),
                      offset.y + h * (THUMBNAIL_DY + DOCLIST_MARGIN_BETWEEN_Y), THUMBNAIL_DX, THUMBNAIL_DY);
            if (isRtl) {
                page.x = rc.dx - page.x - page.dx;
            }
            bool loadOk = true;
            if (!state->thumbnail) {
                loadOk = LoadThumbnail(*state);
            }
            if (loadOk && state->thumbnail) {
                Size thumbSize = state->thumbnail->Size();
                if (thumbSize.dx != THUMBNAIL_DX || thumbSize.dy != THUMBNAIL_DY) {
                    page.dy = thumbSize.dy * THUMBNAIL_DX / thumbSize.dx;
                    page.y += THUMBNAIL_DY - page.dy;
                }
                HRGN clip = CreateRoundRectRgn(page.x, page.y, page.x + page.dx, page.y + page.dy, 10, 10);
                SelectClipRgn(hdc, clip);
                RenderedBitmap* clone = state->thumbnail->Clone();
                if (clone) {
                    UpdateBitmapColors(clone->GetBitmap(), textColor, backgroundColor);
                    clone->StretchDIBits(hdc, page);
                    delete clone;
                } else {
                    state->thumbnail->StretchDIBits(hdc, page);
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
            rTmp = ToRECT(rect);
            DrawText(hdc, path::GetBaseNameNoFree(state->filePath), -1, &rTmp,
                     DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX | (isRtl ? DT_RIGHT : DT_LEFT));

            // note: this crashes asan build in windows code
            // see https://codeeval.dev/gist/bc761bb1ef1cce04e6a1d65e9d30201b
            SHFILEINFO sfi = {0};
            uint flags = SHGFI_SYSICONINDEX | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES;
            HIMAGELIST himl = (HIMAGELIST)SHGetFileInfoW(state->filePath, 0, &sfi, sizeof(sfi), flags);
            x = isRtl ? page.x + page.dx - DpiScale(win->hwndFrame, 16) : page.x;
            ImageList_Draw(himl, sfi.iIcon, hdc, x, rect.y, ILD_TRANSPARENT);
            win->staticLinks.Append(StaticLinkInfo(rect.Union(page), state->filePath, state->filePath));
        }
    }

    /* render bottom links */
    rc.y +=
        DOCLIST_MARGIN_TOP + height * THUMBNAIL_DY + (height - 1) * DOCLIST_MARGIN_BETWEEN_Y + DOCLIST_MARGIN_BOTTOM;
    rc.dy = DOCLIST_BOTTOM_BOX_DY;

    col = GetAppColor(AppColor::MainWindowLink);
    SetTextColor(hdc, col);
    SelectObject(hdc, penLinkLine);

    HIMAGELIST himl = (HIMAGELIST)SendMessageW(win->hwndToolbar, TB_GETIMAGELIST, 0, 0);
    Rect rectIcon(offset.x, rc.y, 0, 0);
    ImageList_GetIconSize(himl, &rectIcon.dx, &rectIcon.dy);
    rectIcon.y += (rc.dy - rectIcon.dy) / 2;
    if (isRtl) {
        rectIcon.x = rc.dx - offset.x - rectIcon.dx;
    }
    ImageList_Draw(himl, 0 /* index of Open icon */, hdc, rectIcon.x, rectIcon.y, ILD_NORMAL);

    txt = _TR("Open a document...");
    GetTextExtentPoint32(hdc, txt, (int)str::Len(txt), &txtSize);
    Rect rect(offset.x + rectIcon.dx + 3, rc.y + (rc.dy - txtSize.cy) / 2, txtSize.cx, txtSize.cy);
    if (isRtl) {
        rect.x = rectIcon.x - rect.dx - 3;
    }
    rTmp = ToRECT(rect);
    DrawText(hdc, txt, -1, &rTmp, isRtl ? DT_RTLREADING : DT_LEFT);
    PaintLine(hdc, Rect(rect.x, rect.y + rect.dy, rect.dx, 0));
    // make the click target larger
    rect = rect.Union(rectIcon);
    rect.Inflate(10, 10);
    win->staticLinks.Append(StaticLinkInfo(rect, SLINK_OPEN_FILE));

    rect = DrawHideFrequentlyReadLink(win->hwndCanvas, hdc, _TR("Hide frequently read"));
    win->staticLinks.Append(StaticLinkInfo(rect, SLINK_LIST_HIDE));

    if (!gIsRaMicroBuild) {
        rect = DrawSupportLink(win->hwndCanvas, hdc, _TR("Support SumatraPDF"));
        win->staticLinks.Append(StaticLinkInfo(rect, URL_SUPPORT_SUMATRA));
    }
}
