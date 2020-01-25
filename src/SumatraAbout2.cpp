/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/GdiPlusUtil.h"
#include "utils/HtmlParserLookup.h"
#include "mui/Mui.h"
#include "utils/WinUtil.h"
#include "SumatraPDF.h"

#include "SumatraConfig.h"
#include "resource.h"
#include "SumatraAbout2.h"
#include "Translations.h"
#include "Version.h"

using namespace mui;

/* This is an experiment to re-implement About window using a generic
layout logic */

#define WND_CLASS_ABOUT2 L"WND_CLASS_SUMATRA_ABOUT2"
#define ABOUT_WIN_TITLE _TR("About SumatraPDF")
#define TABLE_BORDER_WIDTH 2.f

#define VERSION_TXT L"v" CURR_VERSION_STR
#ifdef PRE_RELEASE_VER
#define VERSION_SUB_TXT L"Pre-release"
#else
#define VERSION_SUB_TXT L""
#endif

#define SUMATRA_TXT_FONT L"Arial Black"
#define SUMATRA_TXT_FONT_SIZE 18.f

static ATOM gAboutWndAtom = 0;
static HWND gHwndAbout2 = nullptr;
static HwndWrapper* mainWnd = nullptr;

static Style* styleMainWnd = nullptr;
static Style* styleGrid = nullptr;
static Style* styleCellLeft = nullptr;
static Style* styleCellVer = nullptr;
static Style* styleLogo = nullptr;
static Style* styleBtnVer = nullptr;
static Style* styleBtnLeft = nullptr;
static Style* styleBtnRight = nullptr;

static void CreateAboutStyles() {
    // only create styles once
    if (styleMainWnd)
        return;

    styleMainWnd = new Style();
    styleMainWnd->Set(Prop::AllocColorSolid(PropBgColor, "0xfff200"));

    styleGrid = new Style();
    styleGrid->Set(Prop::AllocColorSolid(PropBgColor, "transparent"));
    styleGrid->SetBorderWidth(TABLE_BORDER_WIDTH);
    styleGrid->SetBorderColor(ParseCssColor("#000"));

    styleCellLeft = new Style();
    // TODO: should change gStyleDefault to something more reasonable so no
    // need to re-define border style so much
    styleCellLeft->SetBorderWidth(0);
    styleCellLeft->Set(Prop::AllocWidth(PropBorderRightWidth, 1.f));
    styleCellLeft->SetBorderColor(ParseCssColor("black"));
    styleCellLeft->Set(Prop::AllocPadding(2, 4, 2, 0));

    styleCellVer = new Style();
    styleCellVer->SetBorderWidth(0);
    styleCellVer->Set(Prop::AllocWidth(PropBorderBottomWidth, TABLE_BORDER_WIDTH));
    styleCellVer->SetBorderColor(ParseCssColor("black"));
    styleCellVer->Set(Prop::AllocPadding(2, 0, 2, 0));

    styleLogo = new Style();
    styleLogo->Set(Prop::AllocFontName(SUMATRA_TXT_FONT));
    styleLogo->Set(Prop::AllocFontSize(SUMATRA_TXT_FONT_SIZE));

    styleBtnVer = new Style();
    styleBtnVer->Set(Prop::AllocFontName(SUMATRA_TXT_FONT));
    styleBtnVer->Set(Prop::AllocFontSize(10.f));
    styleBtnVer->SetBorderWidth(0);
    styleBtnVer->Set(Prop::AllocColorSolid(PropBgColor, "transparent"));
    styleBtnVer->SetPadding(0, 0, 6, 0);

    styleBtnLeft = new Style();
    styleBtnLeft->Set(Prop::AllocFontName(L"Arial"));
    styleBtnLeft->Set(Prop::AllocFontWeight(FontStyleRegular));
    styleBtnLeft->Set(Prop::AllocFontSize(9.f));
    styleBtnLeft->Set(Prop::AllocColorSolid(PropColor, "black"));
    styleBtnLeft->Set(Prop::AllocPadding(2, 8, 2, 16));
    styleBtnLeft->Set(Prop::AllocColorSolid(PropBgColor, "transparent"));
    styleBtnLeft->SetBorderWidth(0);

    styleBtnRight = new Style(styleBtnLeft);
    styleBtnRight->Set(Prop::AllocFontName(L"Arial Black"));
    styleBtnRight->Set(Prop::AllocFontWeight(FontStyleUnderline));
    styleBtnRight->Set(Prop::AllocColorSolid(PropColor, "0020a0"));
}

struct AboutLayoutInfoEl {
    /* static data, must be provided */
    const WCHAR* leftTxt;
    const WCHAR* rightTxt;
    const WCHAR* url;
};

// TODO: replace this link with a better one where license information is nicely collected/linked
#if defined(PRE_RELEASE_VER) || defined(DEBUG)
#define URL_LICENSE L"https://github.com/sumatrapdfreader/sumatrapdf/blob/master/AUTHORS"
#define URL_AUTHORS L"https://github.com/sumatrapdfreader/sumatrapdf/blob/master/AUTHORS"
#define URL_TRANSLATORS L"https://github.com/sumatrapdfreader/sumatrapdf/blob/master/TRANSLATORS"
#else
#define URL_LICENSE L"https://github.com/sumatrapdfreader/sumatrapdf/blob/" UPDATE_CHECK_VER L"rel/AUTHORS"
#define URL_AUTHORS L"https://github.com/sumatrapdfreader/sumatrapdf/blob/" UPDATE_CHECK_VER L"rel/AUTHORS"
#define URL_TRANSLATORS L"https://github.com/sumatrapdfreader/sumatrapdf/blob/" UPDATE_CHECK_VER L"rel/TRANSLATORS"
#endif

static AboutLayoutInfoEl gAboutLayoutInfo[] = {
    {L"website", L"SumatraPDF website", WEBSITE_MAIN_URL},
    {L"manual", L"SumatraPDF manual", WEBSITE_MANUAL_URL},
    {L"forums", L"SumatraPDF forums", L"https://forum.sumatrapdfreader.org/"},
    {L"programming", L"The Programmers", URL_AUTHORS},
    {L"translations", L"The Translators", URL_TRANSLATORS},
    {L"licenses", L"Various Open Source", URL_LICENSE},
#ifdef PRE_RELEASE_VER
    {L"a note", L"Pre-release version, for testing only!", nullptr},
#endif
#ifdef DEBUG
    {L"a note", L"Debug version, for testing only!", nullptr},
#endif
};

#define COL1 RGB(196, 64, 50)
#define COL2 RGB(227, 107, 35)
#define COL3 RGB(93, 160, 40)
#define COL4 RGB(69, 132, 190)
#define COL5 RGB(112, 115, 207)

#define LOGO_TEXT L"SumatraPDF"

static COLORREF gSumatraLogoCols[] = {COL1, COL2, COL3, COL4, COL5, COL5, COL4, COL3, COL2, COL1};

class SumatraLogo : public Control {
  public:
    SumatraLogo() {
    }
    virtual ~SumatraLogo() {
    }
    virtual Size Measure(const Size availableSize);
    virtual void Paint(Graphics* gfx, int offX, int offY);
};

Size SumatraLogo::Measure(const Size availableSize) {
    UNUSED(availableSize);
    Graphics* gfx = AllocGraphicsForMeasureText();
    CachedStyle* s = cachedStyle;
    CachedFont* cachedFont = GetCachedFont(s->fontName, s->fontSize, s->fontWeight);
    Font* font = cachedFont->font;
    CrashIf(!font);
    const WCHAR* txt = LOGO_TEXT;
    RectF bbox;
    int textDx = 0;
    while (*txt) {
        bbox = MeasureText(gfx, font, txt, 1);
        textDx += CeilI(bbox.Width);
        txt++;
    }
    desiredSize.Width = textDx;
    desiredSize.Height = CeilI(font->GetHeight(gfx));
    FreeGraphicsForMeasureText(gfx);
    return desiredSize;
}

void SumatraLogo::Paint(Graphics* gfx, int offX, int offY) {
    CrashIf(!IsVisible());

    CachedStyle* s = cachedStyle;
    CachedFont* cachedFont = GetCachedFont(s->fontName, s->fontSize, s->fontWeight);
    Font* font = cachedFont->font;
    CrashIf(!font);

    int x = offX;
    int y = offY;
    int n = 0;
    const WCHAR* txt = LOGO_TEXT;
    RectF bbox;
    while (*txt) {
        Color c;
        c.SetFromCOLORREF(gSumatraLogoCols[n++]);
        SolidBrush col(c);
        if (n >= dimof(gSumatraLogoCols))
            n = 0;
        gfx->DrawString(txt, 1, font, PointF((REAL)x, (REAL)y), nullptr, &col);
        bbox = MeasureText(gfx, font, txt, 1);
        x += CeilI(bbox.Width);
        txt++;
    }
}

class ButtonUrlHandler {
  public:
    void Clicked(Control* c, int x, int y);
};

void ButtonUrlHandler::Clicked(Control* c, int x, int y) {
    UNUSED(x);
    UNUSED(y);
    WCHAR* url = c->toolTip;
    LaunchBrowser(url);
}

// we only need one instance
static ButtonUrlHandler* gButtonUrlHandler = nullptr;

static void CreateAboutMuiWindow(HWND hwnd) {
    if (!gButtonUrlHandler)
        gButtonUrlHandler = new ButtonUrlHandler();

    CreateAboutStyles();
    mainWnd = new HwndWrapper(hwnd);
    mainWnd->centerContent = true;
    mainWnd->SetStyle(styleMainWnd);
    EventMgr* em = mainWnd->evtMgr;
    CrashIf(!em);

    Grid* grid = new Grid(styleGrid);
    Grid::CellData ld;

    SumatraLogo* logo = new SumatraLogo();
    logo->SetStyle(styleLogo);
    ld.Set(logo, 0, 0, ElAlign::Center);
    ld.colSpan = 2;
    grid->Add(ld);

    Button* b = new Button(VERSION_TXT, styleBtnVer, styleBtnVer);
    ld.Set(b, 1, 0, ElAlign::Center);
    ld.colSpan = 2;
    ld.SetStyle(styleCellVer);
    grid->Add(ld);

    int rows = dimof(gAboutLayoutInfo);
    for (int n = 0; n < rows; n++) {
        const WCHAR* left = gAboutLayoutInfo[n].leftTxt;
        const WCHAR* right = gAboutLayoutInfo[n].rightTxt;
        const WCHAR* url = gAboutLayoutInfo[n].url;

        int row = n + 2;
        b = new Button(left, styleBtnLeft, styleBtnLeft);
        ld.Set(b, row, 0, ElAlign::Right);
        ld.SetStyle(styleCellLeft);
        grid->Add(ld);

        if (url) {
            b = new Button(right, styleBtnRight, styleBtnRight);
            b->SetToolTip(url);
            b->hCursor = GetCursor(IDC_HAND);
        } else {
            b = new Button(right, styleBtnLeft, styleBtnLeft);
        }
        em->EventsForControl(b)->Clicked = [&](Control* c, int x, int y) { gButtonUrlHandler->Clicked(c, x, y); };
        ld.Set(b, row, 1);
        grid->Add(ld);
    }
    mainWnd->AddChild(grid);
}

static void DestroyAboutMuiWindow() {
    gHwndAbout2 = nullptr;
    delete mainWnd;
    mainWnd = nullptr;
    delete gButtonUrlHandler;
    gButtonUrlHandler = nullptr;
}

static void CopyAboutInfoToClipboard(HWND hwnd) {
    UNUSED(hwnd);
    str::WStr info(512);
    info.AppendFmt(L"%s %s\r\n", getAppName(), VERSION_TXT);
    for (size_t i = info.size() - 2; i > 0; i--) {
        info.Append('-');
    }
    info.Append(L"\r\n");
    // concatenate all the information into a single string
    // (cf. CopyPropertiesToClipboard in SumatraProperties.cpp)
    size_t maxLen = 0;
    for (size_t i = 0; i < dimof(gAboutLayoutInfo); i++) {
        AboutLayoutInfoEl& el = gAboutLayoutInfo[i];
        maxLen = std::max(maxLen, str::Len(el.leftTxt));
    }
    for (size_t i = 0; i < dimof(gAboutLayoutInfo); i++) {
        AboutLayoutInfoEl& el = gAboutLayoutInfo[i];
        for (size_t j = maxLen - str::Len(el.leftTxt); j > 0; j--)
            info.Append(' ');
        info.AppendFmt(L"%s: %s\r\n", el.leftTxt, el.url ? el.url : el.rightTxt);
    }
    CopyTextToClipboard(info.LendData());
}

static LRESULT CALLBACK WndProcAbout2(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (mainWnd) {
        bool wasHandled;
        LRESULT res = mainWnd->evtMgr->OnMessage(msg, wParam, lParam, wasHandled);
        if (wasHandled)
            return res;
    }

    switch (msg) {
        case WM_CHAR:
            if (VK_ESCAPE == wParam)
                DestroyWindow(hwnd);
            break;

        case WM_CREATE:
            CreateAboutMuiWindow(hwnd);
            break;

        case WM_COMMAND:
            if (IDM_COPY_SELECTION == LOWORD(wParam))
                CopyAboutInfoToClipboard(hwnd);
            break;

        case WM_DESTROY:
            DestroyAboutMuiWindow();
            break;

        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

void OnMenuAbout2() {
    WNDCLASSEX wcex;

    if (gHwndAbout2) {
        SetActiveWindow(gHwndAbout2);
        return;
    }

    if (!gAboutWndAtom) {
        FillWndClassEx(wcex, WND_CLASS_ABOUT2, WndProcAbout2);
        HMODULE h = GetModuleHandleW(nullptr);
        wcex.hIcon = LoadIcon(h, MAKEINTRESOURCE(getAppIconID()));
        gAboutWndAtom = RegisterClassEx(&wcex);
        CrashIf(!gAboutWndAtom);
    }
    gHwndAbout2 =
        CreateWindow(WND_CLASS_ABOUT2, ABOUT_WIN_TITLE, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, CW_USEDEFAULT,
                     CW_USEDEFAULT, 520, 400, nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
    if (!gHwndAbout2)
        return;

    SetRtl(gHwndAbout2, IsUIRightToLeft());

    ShowWindow(gHwndAbout2, SW_SHOW);
}
