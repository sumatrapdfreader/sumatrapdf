/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "SumatraAbout2.h"

#include "Mui.h"
#include "resource.h"
#include "SumatraPDF.h"
#include "Translations.h"
#include "Version.h"
#include "WinUtil.h"

using namespace mui;

/* This is an experiment to re-implement About window using a generic
layout logic */

#define WND_CLASS_ABOUT2        L"WND_CLASS_SUMATRA_ABOUT2"
#define ABOUT_WIN_TITLE         _TR("About SumatraPDF")

static ATOM gAboutWndAtom = 0;
static HWND gHwndAbout2 = NULL;
static HwndWrapper *mainWnd = NULL;

static Style *   styleMainWnd = NULL;
static Style *   styleBtnLeft = NULL;
static Style *   styleBtnRight = NULL;

// should only be called once at the end of the program
extern "C" static void DeleteAboutStyles()
{
    delete styleMainWnd;
    delete styleBtnLeft;
    delete styleBtnRight;
}

static void CreateAboutStyles()
{
    // only create styles once
    if (styleMainWnd)
        return;

    styleMainWnd = new Style();
    styleMainWnd->Set(Prop::AllocColorSolid(PropBgColor, "fff200"));

    styleBtnLeft = new Style();
    styleBtnLeft->Set(Prop::AllocFontName(L"Arial"));
    styleBtnLeft->Set(Prop::AllocFontWeight(FontStyleRegular));
    styleBtnLeft->Set(Prop::AllocFontSize(9.f));
    styleBtnLeft->Set(Prop::AllocColorSolid(PropColor, "black"));
    styleBtnLeft->Set(Prop::AllocPadding(2, 4, 2, 4));
    styleBtnLeft->Set(Prop::AllocColorSolid(PropBgColor, "transparent"));
    styleBtnLeft->SetBorderWidth(0);

    styleBtnRight = new Style(styleBtnLeft);
    styleBtnRight->Set(Prop::AllocFontName(L"Arial Black"));
    styleBtnRight->Set(Prop::AllocFontWeight(FontStyleUnderline));
    styleBtnRight->Set(Prop::AllocColorSolid(PropColor, "0020a0"));
    atexit(DeleteAboutStyles);
}

struct AboutLayoutInfoEl {
    /* static data, must be provided */
    const WCHAR *   leftTxt;
    const WCHAR *   rightTxt;
    const WCHAR *   url;
};

// TODO: replace this link with a better one where license information is nicely collected/linked
#if defined(SVN_PRE_RELEASE_VER) || defined(DEBUG)
#define URL_LICENSE L"http://sumatrapdf.googlecode.com/svn/trunk/AUTHORS"
#define URL_AUTHORS L"http://sumatrapdf.googlecode.com/svn/trunk/AUTHORS"
#define URL_TRANSLATORS L"http://sumatrapdf.googlecode.com/svn/trunk/TRANSLATORS"
#else
#define URL_LICENSE L"http://sumatrapdf.googlecode.com/svn/tags/" CURR_VERSION_STR L"rel/AUTHORS"
#define URL_AUTHORS L"http://sumatrapdf.googlecode.com/svn/tags/" CURR_VERSION_STR L"rel/AUTHORS"
#define URL_TRANSLATORS L"http://sumatrapdf.googlecode.com/svn/tags/" CURR_VERSION_STR L"rel/TRANSLATORS"
#endif

static AboutLayoutInfoEl gAboutLayoutInfo[] = {
    { L"website",        L"SumatraPDF website",   WEBSITE_MAIN_URL},
    { L"manual",         L"SumatraPDF manual",    WEBSITE_MANUAL_URL },
    { L"forums",         L"SumatraPDF forums",    L"http://blog.kowalczyk.info/forum_sumatra" },
    { L"programming",    L"The Programmers",      URL_AUTHORS },
    { L"translations",   L"The Translators",      URL_TRANSLATORS },
    { L"licenses",       L"Various Open Source",  URL_LICENSE },
#ifdef SVN_PRE_RELEASE_VER
    { L"a note",         L"Pre-release version, for testing only!", NULL },
#endif
#ifdef DEBUG
    { L"a note",         L"Debug version, for testing only!", NULL },
#endif
};

static HCURSOR gCursorHand = NULL;

class ButtonUrlHandler : public sigslot::has_slots
{
public:
    void Clicked(Control *c, int x, int y);
};

void ButtonUrlHandler::Clicked(Control *c, int x, int y)
{
    WCHAR *url = c->toolTip;
    LaunchBrowser(url);
}

// we only need one instance
static ButtonUrlHandler *gButtonUrlHandler = NULL;

static void CreateAboutMuiWindow(HWND hwnd)
{
    if (!gCursorHand)
        gCursorHand  = LoadCursor(NULL, IDC_HAND);
    if (!gButtonUrlHandler)
        gButtonUrlHandler = new ButtonUrlHandler();

    CreateAboutStyles();
    mainWnd = new HwndWrapper(hwnd);
    mainWnd->SetMinSize(Size(320, 200));
    mainWnd->SetStyle(styleMainWnd);
    EventMgr *em = mainWnd->evtMgr;
    CrashIf(!em);

    Grid *l = new Grid();
    Grid::CellData ld;

    int rows = dimof(gAboutLayoutInfo);
    Button *b;
    for (int row = 0; row < rows; row++) {
        const WCHAR *left = gAboutLayoutInfo[row].leftTxt;
        const WCHAR *right = gAboutLayoutInfo[row].rightTxt;
        const WCHAR *url = gAboutLayoutInfo[row].url;

        b = new Button(left, styleBtnLeft, styleBtnLeft);
        ld.Set(b, row, 0, ElAlignRight);
        l->Add(ld);
        mainWnd->AddChild(b);

        if (url) {
            b = new Button(right, styleBtnRight, styleBtnRight);
            b->SetToolTip(url);
            b->hCursor = gCursorHand;
        } else {
            b = new Button(right, styleBtnLeft, styleBtnLeft);
        }
        mainWnd->AddChild(b);
        em->EventsForControl(b)->Clicked.connect(gButtonUrlHandler, &ButtonUrlHandler::Clicked);
        ld.Set(b, row, 1);
        l->Add(ld);
    }

    mainWnd->layout = l;
}

static void DestroyAboutMuiWindow()
{
    EventMgr *em = mainWnd->evtMgr;
    size_t n = mainWnd->GetChildCount();
    // TODO: probably should disconnect everything when deleting a window
    for (size_t i = 0; i < n; i++)
    {
        Control *c = mainWnd->GetChild(i);
        em->EventsForControl(c)->Clicked.disconnect_all();
    }
    delete gButtonUrlHandler;
    gButtonUrlHandler = NULL;
    gHwndAbout2 = NULL;
    delete mainWnd;
    mainWnd = NULL;
}

static LRESULT CALLBACK WndProcAbout2(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
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

        case WM_DESTROY:
            DestroyAboutMuiWindow();
            break;

        case WM_ERASEBKGND:
            return 0;

        case WM_PAINT:
            if (mainWnd)
                mainWnd->OnPaint(hwnd);
            break;

        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

void OnMenuAbout2()
{
    WNDCLASSEX  wcex;

    if (gHwndAbout2) {
        SetActiveWindow(gHwndAbout2);
        return;
    }

    if (!gAboutWndAtom) {
        FillWndClassEx(wcex, ghinst, WND_CLASS_ABOUT2, WndProcAbout2);
        wcex.hIcon = LoadIcon(ghinst, MAKEINTRESOURCE(IDI_SUMATRAPDF));
        gAboutWndAtom = RegisterClassEx(&wcex);
        CrashIf(!gAboutWndAtom);
    }
    gHwndAbout2 = CreateWindow(
            WND_CLASS_ABOUT2, ABOUT_WIN_TITLE,
            WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
            CW_USEDEFAULT, CW_USEDEFAULT,
            520, 400,
            NULL, NULL,
            ghinst, NULL);
    if (!gHwndAbout2)
        return;

    ShowWindow(gHwndAbout2, SW_SHOW);

}

