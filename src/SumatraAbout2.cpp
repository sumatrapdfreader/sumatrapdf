#include "BaseUtil.h"
#include "SumatraAbout2.h"
#include "Mui.h"

#include "resource.h"
#include "SumatraPDF.h"
#include "Translations.h"
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

#define COLOR_LIGHT_BLUE    "64C7EF"

// should only be called once at the end of the program
extern "C" static void DeleteAboutStyles()
{
    delete styleMainWnd;
}

static void CreateAboutStyles()
{
    // only create styles once
    if (styleMainWnd)
        return;

    styleMainWnd = new Style();
    styleMainWnd->Set(Prop::AllocColorSolid(PropBgColor, COLOR_LIGHT_BLUE));
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
#else
#define URL_LICENSE L"http://sumatrapdf.googlecode.com/svn/tags/" CURR_VERSION_STR L"rel/AUTHORS"
#endif

static AboutLayoutInfoEl gAboutLayoutInfo[] = {
    { L"website",        L"SumatraPDF website",   WEBSITE_MAIN_URL},
    { L"forums",         L"SumatraPDF forums",    L"http://blog.kowalczyk.info/forum_sumatra" },
    { L"programming",    L"Krzysztof Kowalczyk",  L"http://blog.kowalczyk.info" },
    { L"programming",    L"Simon B\xFCnzli",      L"http://www.zeniko.ch/#SumatraPDF" },
    { L"programming",    L"William Blum",         L"http://william.famille-blum.org/" },
    { L"license",        L"open source",          URL_LICENSE },
#ifdef SVN_PRE_RELEASE_VER
    { L"a note",         L"Pre-release version, for testing only!", NULL },
#endif
#ifdef DEBUG
    { L"a note",         L"Debug version, for testing only!", NULL },
#endif
    { L"pdf rendering",  L"MuPDF",                L"http://mupdf.com" },
    // TODO: remove these two lines in favor of the above license link?
    { L"program icon",   L"Zenon",                L"http://www.flashvidz.tk/" },
    { L"toolbar icons",  L"Yusuke Kamiyamane",    L"http://p.yusukekamiyamane.com/" },
    { L"translators",    L"The Translators",      L"http://blog.kowalczyk.info/software/sumatrapdf/translators.html" },
    { L"translations",   L"Contribute translation", WEBSITE_TRANSLATIONS_URL }
};

static void CreateAboutMuiWindow(HWND hwnd)
{
    CreateAboutStyles();
    mainWnd = new HwndWrapper(hwnd);
    mainWnd->SetMinSize(Size(320, 200));
    mainWnd->SetStyle(styleMainWnd);

    int rows = dimof(gAboutLayoutInfo);
    GridLayout *l = new GridLayout(rows, 2);
    GridLayoutData ld;

    for (int row = 0; row < rows; row++) {
        const WCHAR *left = gAboutLayoutInfo[row].leftTxt;
        const WCHAR *right = gAboutLayoutInfo[row].rightTxt;
        // TODO: use url

        ld.Set(new Button(left, NULL, NULL), row, 0);
        l->Add(ld);
        ld.Set(new Button(right, NULL, NULL), 0, 1);
        l->Add(ld);
    }

    // TODO: add some way to automatically add layout's children to window?
    for (GridLayoutData *ld = l->els.IterStart(); ld; ld = l->els.IterNext())
    {
        mainWnd->AddChild(reinterpret_cast<Control*>(ld->el));
    }
    mainWnd->layout = l;
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

    case WM_CREATE:
        CreateAboutMuiWindow(hwnd);
        break;

    case WM_DESTROY:
        delete mainWnd;
        mainWnd = NULL;
        gHwndAbout2 = NULL;
        break;

    case WM_ERASEBKGND:
        return 0;

    case WM_PAINT:
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
            CW_USEDEFAULT, CW_USEDEFAULT,
            NULL, NULL,
            ghinst, NULL);
    if (!gHwndAbout2)
        return;

    ShowWindow(gHwndAbout2, SW_SHOW);

}

