#include "MobiWindow.h"

#include "EbookController.h"
#include "EbookControls.h"
#include "Menu.h"
#include "MobiDoc.h"
#include "Resource.h"
#include "SumatraPDF.h"
#include "Translations.h"
#include "WinUtil.h"

#define MOBI_FRAME_CLASS_NAME    _T("SUMATRA_MOBI_FRAME")

#define WIN_DX    720
#define WIN_DY    640

static bool gShowTextBoundingBoxes = false;

static float gUiDPIFactor = 1.0f;
inline int dpiAdjust(int value)
{
    return (int)(value * gUiDPIFactor);
}

static Vec<MobiWindow*> *gMobiWindows = NULL;

static MenuDef menuDefMobiFile[] = {
    { _TRN("&Open\tCtrl+O"),                IDM_OPEN ,                  MF_REQ_DISK_ACCESS },
    { _TRN("&Close\tCtrl+W"),               IDM_CLOSE,                  MF_REQ_DISK_ACCESS },
    { SEP_ITEM,                             0,                          MF_REQ_DISK_ACCESS },
    { _TRN("E&xit\tCtrl+Q"),                IDM_EXIT,                   0 }
};

static MenuDef menuDefMobiGoTo[] = {
    { _TRN("&Next Page\tRight Arrow"),      IDM_GOTO_NEXT_PAGE,         0 },
    { _TRN("&Previous Page\tLeft Arrow"),   IDM_GOTO_PREV_PAGE,         0 },
    { _TRN("&First Page\tHome"),            IDM_GOTO_FIRST_PAGE,        0 },
    { _TRN("&Last Page\tEnd"),              IDM_GOTO_LAST_PAGE,         0 },
};

static MenuDef menuDefMobiSettings[] = {
    { _TRN("Change Language"),              IDM_CHANGE_LANGUAGE,        0  },
    { _TRN("&Options..."),                  IDM_SETTINGS,               MF_REQ_PREF_ACCESS },
};

static MenuDef menuDefHelp[] = {
    { _TRN("Visit &Website"),               IDM_VISIT_WEBSITE,          MF_REQ_DISK_ACCESS },
    { _TRN("&Manual"),                      IDM_MANUAL,                 MF_REQ_DISK_ACCESS },
    { _TRN("Check for &Updates"),           IDM_CHECK_UPDATE,           MF_REQ_INET_ACCESS },
    { SEP_ITEM,                             0,                          MF_REQ_DISK_ACCESS },
    { _TRN("&About"),                       IDM_ABOUT,                  0 },
};

#ifdef SHOW_DEBUG_MENU_ITEMS
static MenuDef menuDefDebug[] = {
    { "Show bbox",                          IDM_DEBUG_SHOW_LINKS,       MF_NO_TRANSLATE },
};
#endif

static HMENU BuildMobiMenu()
{
    HMENU mainMenu = CreateMenu();
    HMENU m = CreateMenu();
    win::menu::Empty(m);
    BuildMenuFromMenuDef(menuDefMobiFile, dimof(menuDefMobiFile), m, false);
    AppendRecentFilesToMenu(m);

    AppendMenu(mainMenu, MF_POPUP | MF_STRING, (UINT_PTR)m, _TR("&File"));
    m = BuildMenuFromMenuDef(menuDefMobiGoTo, dimof(menuDefMobiGoTo), CreateMenu(), false);
    AppendMenu(mainMenu, MF_POPUP | MF_STRING, (UINT_PTR)m, _TR("&Go To"));
    m = BuildMenuFromMenuDef(menuDefMobiSettings, dimof(menuDefMobiSettings), CreateMenu());
    AppendMenu(mainMenu, MF_POPUP | MF_STRING, (UINT_PTR)m, _TR("&Settings"));
    m = BuildMenuFromMenuDef(menuDefHelp, dimof(menuDefHelp), CreateMenu());
    AppendMenu(mainMenu, MF_POPUP | MF_STRING, (UINT_PTR)m, _TR("&Help"));
#ifdef SHOW_DEBUG_MENU_ITEMS
    m = BuildMenuFromMenuDef(menuDefDebug, dimof(menuDefDebug), CreateMenu());
    AppendMenu(mainMenu, MF_POPUP | MF_STRING, (UINT_PTR)m, _T("Debug"));
#endif

    return mainMenu;
}

TCHAR *MobiWindow::LoadedFilePath() const
{
    if (!ebookController || !ebookController->GetMobiDoc())
        return NULL;
    return ebookController->GetMobiDoc()->GetFileName();
}

static MobiWindow* FindMobiWindowByHwnd(HWND hwnd)
{
    if (!gMobiWindows)
        return NULL;
    for (MobiWindow **w = gMobiWindows->IterStart(); w; w = gMobiWindows->IterNext()) {
        if ((*w)->hwndFrame == hwnd)
            return *w;
    }
    return NULL;
}

MobiWindow* FindMobiWindowByController(EbookController *controller)
{
    for (MobiWindow **w = gMobiWindows->IterStart(); w; w = gMobiWindows->IterNext()) {
        if ((*w)->ebookController == controller)
            return *w;
    }
    return NULL;
}

#define LAYOUT_TIMER_ID 1

void RestartLayoutTimer(EbookController *controller)
{
    MobiWindow *win = FindMobiWindowByController(controller);
    KillTimer(win->hwndFrame, LAYOUT_TIMER_ID);
    SetTimer(win->hwndFrame,  LAYOUT_TIMER_ID, 600, NULL);
}

static void OnTimer(MobiWindow *win, WPARAM timerId)
{
    CrashIf(timerId != LAYOUT_TIMER_ID);
    KillTimer(win->hwndFrame, LAYOUT_TIMER_ID);
    win->ebookController->OnLayoutTimer();
}

static void OnToggleBbox(MobiWindow *win)
{
    gShowTextBoundingBoxes = !gShowTextBoundingBoxes;
    SetDebugPaint(gShowTextBoundingBoxes);
    InvalidateRect(win->hwndFrame, NULL, FALSE);
}

static LRESULT OnKeyDown(MobiWindow *win, UINT msg, WPARAM key, LPARAM lParam)
{
    switch (key) {
    case VK_LEFT: case VK_PRIOR: case 'P':
        win->ebookController->AdvancePage(-1);
        break;
    case VK_RIGHT: case VK_NEXT: case 'N':
        win->ebookController->AdvancePage(1);
        break;
    case VK_SPACE:
        win->ebookController->AdvancePage(IsShiftPressed() ? -1 : 1);
        break;
#ifdef DEBUG
    case VK_F1:
        OnToggleBbox(win);
        break;
#endif
    case VK_HOME:
        win->ebookController->GoToPage(1);
        break;
    case VK_END:
        win->ebookController->GoToLastPage();
        break;
    default:
        return DefWindowProc(win->hwndFrame, msg, key, lParam);
    }
    return 0;
}

static void CloseMobiWindow(MobiWindow *win)
{
    // TODO: write me

}

static void OnMenuOpenMobi(MobiWindow *win)
{

}

static LRESULT OnCommand(MobiWindow *win, UINT msg, WPARAM wParam, LPARAM lParam)
{
    int wmId = LOWORD(wParam);

    switch (wmId)
    {
        case IDM_OPEN:
        case IDT_FILE_OPEN:
            OnMenuOpenMobi(win);
            break;

        case IDT_FILE_EXIT:
        case IDM_CLOSE:
            CloseMobiWindow(win);
            break;

        case IDM_EXIT:
            OnMenuExit();
            break;

        case IDM_GOTO_NEXT_PAGE:
            win->ebookController->AdvancePage(1);
            break;

        case IDM_GOTO_PREV_PAGE:
            win->ebookController->AdvancePage(-1);
            break;

        case IDM_GOTO_FIRST_PAGE:
            win->ebookController->GoToPage(1);
            break;

        case IDM_GOTO_LAST_PAGE:
            win->ebookController->GoToLastPage();
            break;

#if 0
        case IDM_CHANGE_LANGUAGE:
            OnMenuChangeLanguage(*win);
            break;

        case IDM_VIEW_BOOKMARKS:
            ToggleTocBox(win);
            break;
#endif

#if 0
        case IDM_GOTO_PAGE:
            OnMenuGoToPage(*win);
            break;
#endif

        case IDM_VISIT_WEBSITE:
            LaunchBrowser(_T("http://blog.kowalczyk.info/software/sumatrapdf/"));
            break;

        case IDM_MANUAL:
            LaunchBrowser(_T("http://blog.kowalczyk.info/software/sumatrapdf/manual.html"));
            break;

        case IDM_DEBUG_SHOW_LINKS:
            OnToggleBbox(win);
            return 0;

#if 0
        case IDM_ABOUT:
            OnMenuAbout();
            break;
#endif

        case IDM_CHECK_UPDATE:
            AutoUpdateCheckAsync(win->hwndFrame, false);
            break;

        case IDM_SETTINGS:
            OnMenuSettings(win->hwndFrame);
            break;

        default:
            return DefWindowProc(win->hwndFrame, msg, wParam, lParam);
    }

    return 0;
}

static LRESULT CALLBACK MobiWndProcFrame(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    // messages that don't require MobiWindow
    switch (msg)
    {
        case WM_CREATE:
            // we do nothing
            break;
        case WM_DESTROY:
            // TODO: if this is the last window, exit the program
            break;

        // if we return 0, during WM_PAINT we can check
        // PAINTSTRUCT.fErase to see if WM_ERASEBKGND
        // was sent before WM_PAINT
        case WM_ERASEBKGND:
            return 0;
    }

    // messages that do require MobiWindow
    MobiWindow *win = FindMobiWindowByHwnd(hwnd);
    if (!win)
        return DefWindowProc(hwnd, msg, wParam, lParam);

    bool wasHandled;
    LRESULT res = win->hwndWrapper->evtMgr->OnMessage(msg, wParam, lParam, wasHandled);
    if (wasHandled)
        return res;

    switch (msg)
    {
        case WM_PAINT:
            win->hwndWrapper->OnPaint(hwnd);
            break;

        case WM_KEYDOWN:
            return OnKeyDown(win, msg, wParam, lParam);

        case WM_COMMAND:
            return OnCommand(win, msg, wParam, lParam);
            break;

        case WM_TIMER:
            OnTimer(win, wParam);
            break;

        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    return 0;
}

void OpenMobiInWindow(MobiDoc *mobiDoc)
{
    if (!gMobiWindows)
        gMobiWindows = new Vec<MobiWindow*>();

    win::GetHwndDpi(NULL, &gUiDPIFactor);
    HWND hwnd = CreateWindow(MOBI_FRAME_CLASS_NAME, _T("Ebook Test"),
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT,
            dpiAdjust(WIN_DX), dpiAdjust(WIN_DY),
            NULL, NULL,
            ghinst, NULL);
    if (!hwnd)
        return;
    SetMenu(hwnd, BuildMobiMenu());

    MobiWindow *win = new MobiWindow();
    win->ebookControls = CreateEbookControls(hwnd);
    win->hwndWrapper = win->ebookControls->mainWnd;
    win->ebookController = new EbookController(win->ebookControls);
    //win->ebookController->SetHtml(gSampleHtml);
    win->ebookController->SetMobiDoc(mobiDoc);

    win->hwndFrame = hwnd;
    gMobiWindows->Append(win);
    ShowWindow(hwnd, SW_SHOW);
}

static void DeleteMobiWindow(MobiWindow *win)
{
    DestroyWindow(win->hwndFrame);
    delete win->ebookController;
    DestroyEbookControls(win->ebookControls);
    gMobiWindows->Remove(win);
    delete win;
}

void DeleteMobiWindows()
{
    if (!gMobiWindows)
        return;
    while (gMobiWindows->Count() > 0) {
        DeleteMobiWindow(gMobiWindows->At(0));
    }
    delete gMobiWindows;
}

bool RegisterMobiWinClass(HINSTANCE hinst)
{
    WNDCLASSEX  wcex;
    FillWndClassEx(wcex, hinst);
    // clear out CS_HREDRAW | CS_VREDRAW style so that resizing doesn't
    // cause the whole window redraw
    wcex.style = 0;
    wcex.lpszClassName  = MOBI_FRAME_CLASS_NAME;
    wcex.hIcon          = LoadIcon(hinst, MAKEINTRESOURCE(IDI_SUMATRAPDF));
    wcex.hbrBackground  = CreateSolidBrush(GetSysColor(COLOR_BTNFACE));
    wcex.lpfnWndProc    = MobiWndProcFrame;

    ATOM atom = RegisterClassEx(&wcex);
    return atom != NULL;
}
