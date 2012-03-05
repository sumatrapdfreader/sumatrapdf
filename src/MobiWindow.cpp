#include "MobiWindow.h"

#include "EbookController.h"
#include "EbookControls.h"
#include "MobiDoc.h"
#include "Resource.h"
#include "SumatraPDF.h"
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

static LRESULT OnCommand(MobiWindow *win, UINT msg, WPARAM wParam, LPARAM lParam)
{
    int wmId = LOWORD(wParam);

    if ((IDM_EXIT == wmId) || (IDCANCEL == wmId)) {
        // TODO: exit
        return 0;
    }

    if (IDM_OPEN == wmId) {
        // TODO: open dialog
        return 0;
    }

#if 0
    if (IDM_TOGGLE_BBOX == wmId) {
        OnToggleBbox(win);
        return 0;
    }
#endif

    return DefWindowProc(win->hwndFrame, msg, wParam, lParam);
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
    case VK_F1:
        //TODO: write me
        //OnToggleBbox(hwnd);
        break;
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
            OnCommand(win, msg, wParam, lParam);
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
    MobiWindow *win = new MobiWindow();
    //HMENU menu = BuildMenu();
    //SetMenu(hwnd, menu);

    win->ebookControls = CreateEbookControls(hwnd);
    win->hwndWrapper = win->ebookControls->mainWnd;
    win->ebookController = new EbookController(win->ebookControls);
    //win->ebookController->SetHtml(gSampleHtml);
    win->ebookController->SetMobiDoc(mobiDoc);

    win->hwndFrame = hwnd;
    gMobiWindows->Append(win);
    ShowWindow(hwnd, SW_SHOW);
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
