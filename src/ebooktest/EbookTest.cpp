/* Copyright 2010-2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include <locale.h>

#include "EbookController.h"
#include "EbookControls.h"
#include "EbookTestMenu.h"
#include "FileUtil.h"
#include "FrameTimeoutCalculator.h"
#include "MobiDoc.h"
#include "Mui.h"
#include "Resource.h"
#include "PageLayout.h"
#include "SvgPath.h"
#include "StrUtil.h"
#include "Scoped.h"
#include "Timer.h"
#include "UiMsgEbook.h"
#include "UiMsg.h"
#include "Version.h"
#include "WinUtil.h"

#include "DebugLog.h"

/* TODO: by hooking into mouse move events in HorizontalProgress control, we
could show a window telling the user which page would we go to if he was
to click there. */

using namespace mui;

#define ET_FRAME_CLASS_NAME    _T("ET_FRAME")

#define WIN_DX    720
#define WIN_DY    640

static HINSTANCE            ghinst = NULL;
static HWND                 gHwndFrame = NULL;
static EbookControls *      gEbookControls = NULL;
static HwndWrapper *        gMainWnd = NULL;
static EbookController *    gEbookController = NULL;

static bool gShowTextBoundingBoxes = false;

// A sample text to display if we don't show an actual mobi file
static const char *gSampleHtml = "<html><p align=justify width=1em><b>ClearType</b>, is <b>dependent</b> on the <i>orientation &amp; ordering</i> of the LCD stripes and possibly some other things unknown.</p> <p align='right height=13pt'><em>Currently</em>, ClearType is implemented <hr><br/> only for vertical stripes that are ordered RGB.</p> <p align=center height=8pt>This might be a concern if you are using a tablet PC.</p> <p width='1em'>Where the display can be oriented in any direction, or if you are using a screen that can be turned from landscape to portrait. The <strike>following example</strike> draws text with two <u>different quality</u> settings.</p> <p width=1em>This is a paragraph that should take at least two lines. With study and discreet inquiries, Abagnale picked up airline jargon and discovered that pilots could ride free anywhere in the world on any airline; and that hotels billed airlines direct and cashed checks issued by airline companies.</p><br><p width=1em>    And this is another paragraph tha we wrote today. Hiding out in a southern city, Abagnale learned that the state attorney general was seeking assistants. For nine months he practiced law, but when a real Harvard lawyer appeared on the scene, Abagnale figured it was time to move on.</p> On to the <b>next<mbp:pagebreak>page</b><p>ThisIsAVeryVeryVeryLongWordThatShouldBeBrokenIntoMultiple lines</p><mbp:pagebreak><hr><mbp:pagebreak>blah<br>Foodo.<p>And me</p></html>";

void LogProcessRunningTime()
{
    lf("EbookTest startup time: %.2f ms", GetProcessRunningTime());
}

#define TEN_SECONDS_IN_MS 10*1000

static float gUiDPIFactor = 1.0f;
inline int dpiAdjust(int value)
{
    return (int)(value * gUiDPIFactor);
}

static void OnExit()
{
    SendMessage(gHwndFrame, WM_CLOSE, 0, 0);
}

static inline void EnableAndShow(HWND hwnd, bool enable)
{
    ShowWindow(hwnd, enable ? SW_SHOW : SW_HIDE);
    EnableWindow(hwnd, enable);
}

static void OnCreateWindow(HWND hwnd)
{
    HMENU menu = BuildMenu();
    SetMenu(hwnd, menu);
    gEbookControls = CreateEbookControls(hwnd);
    gMainWnd = gEbookControls->mainWnd;
    gEbookController = new EbookController(gEbookControls);
    gEbookController->SetHtml(gSampleHtml);
}

static void OnOpen(HWND hwnd)
{
    OPENFILENAME ofn = { 0 };
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;

    str::Str<TCHAR> fileFilter;
    fileFilter.Append(_T("All supported documents"));

    ofn.lpstrFilter = _T("All supported documents\0;*.mobi;*.awz;\0\0");
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY |
                OFN_ALLOWMULTISELECT | OFN_EXPLORER;
    ofn.nMaxFile = MAX_PATH * 100;
    ScopedMem<TCHAR> file(SAZA(TCHAR, ofn.nMaxFile));
    ofn.lpstrFile = file;

    if (!GetOpenFileName(&ofn))
        return;

    TCHAR *fileName = ofn.lpstrFile + ofn.nFileOffset;
    if (*(fileName - 1)) {
        // special case: single filename without NULL separator
        gEbookController->LoadMobi(ofn.lpstrFile);
        return;
    }
    // this is just a test app, no need to support multiple files
    CrashIf(true);
}

static void OnToggleBbox(HWND hwnd)
{
    gShowTextBoundingBoxes = !gShowTextBoundingBoxes;
    SetDebugPaint(gShowTextBoundingBoxes);
    InvalidateRect(hwnd, NULL, FALSE);
}

static LRESULT OnCommand(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    int wmId = LOWORD(wParam);

    if ((IDM_EXIT == wmId) || (IDCANCEL == wmId)) {
        OnExit();
        return 0;
    }

    if (IDM_OPEN == wmId) {
        OnOpen(hwnd);
        return 0;
    }

    if (IDM_TOGGLE_BBOX == wmId) {
        OnToggleBbox(hwnd);
        return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

#define LAYOUT_TIMER_ID 1

void RestartLayoutTimer()
{
    KillTimer(gHwndFrame, LAYOUT_TIMER_ID);
    SetTimer(gHwndFrame,  LAYOUT_TIMER_ID, 600, NULL);
}

static void OnTimer(HWND hwnd, WPARAM timerId)
{
    switch (timerId) {
        case LAYOUT_TIMER_ID:
            KillTimer(hwnd, LAYOUT_TIMER_ID);
            gEbookController->OnLayoutTimer();
            break;
    }
}

static LRESULT OnKeyDown(HWND hwnd, UINT msg, WPARAM key, LPARAM lParam)
{
    switch (key) {
    case VK_LEFT: case VK_PRIOR: case 'P':
        gEbookController->AdvancePage(-1);
        break;
    case VK_RIGHT: case VK_NEXT: case 'N':
        gEbookController->AdvancePage(1);
        break;
    case VK_SPACE:
        gEbookController->AdvancePage(IsShiftPressed() ? -1 : 1);
        break;
    case VK_F1:
        OnToggleBbox(hwnd);
        break;
    case VK_HOME:
        gEbookController->GoToPage(1);
        break;
    case VK_END:
        gEbookController->GoToLastPage();
        break;
    default:
        return DefWindowProc(hwnd, msg, key, lParam);
    }
    return 0;
}

static LRESULT CALLBACK WndProcFrame(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static bool seenWmPaint = false;

    if (gMainWnd) {
        bool wasHandled;
        LRESULT res = gMainWnd->evtMgr->OnMessage(msg, wParam, lParam, wasHandled);
        if (wasHandled)
            return res;
    }

    switch (msg)
    {
        case WM_CREATE:
            OnCreateWindow(hwnd);
            break;

        case WM_DESTROY:
            PostQuitMessage(0);
            break;

        // if we return 0, during WM_PAINT we can check
        // PAINTSTRUCT.fErase to see if WM_ERASEBKGND
        // was sent before WM_PAINT
        case WM_ERASEBKGND:
            return 0;

        case WM_PAINT:
            if (!seenWmPaint) {
                LogProcessRunningTime();
                seenWmPaint = true;
            }
            gMainWnd->OnPaint(hwnd);
            break;

        case WM_KEYDOWN:
            return OnKeyDown(hwnd, msg, wParam, lParam);

        case WM_COMMAND:
            OnCommand(hwnd, msg, wParam, lParam);
            break;

        case WM_TIMER:
            OnTimer(hwnd, wParam);
            break;

        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    return 0;
}

static BOOL RegisterWinClass(HINSTANCE hInstance)
{
    WNDCLASSEX  wcex;

    FillWndClassEx(wcex, hInstance);
    // clear out CS_HREDRAW | CS_VREDRAW style so that resizing doesn't
    // cause the whole window redraw
    wcex.style = 0;
    wcex.lpszClassName  = ET_FRAME_CLASS_NAME;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SUMATRAPDF));
    wcex.hbrBackground  = CreateSolidBrush(GetSysColor(COLOR_BTNFACE));

    wcex.lpfnWndProc    = WndProcFrame;

    ATOM atom = RegisterClassEx(&wcex);
    return atom != NULL;
}

static BOOL InstanceInit(HINSTANCE hInstance, int nCmdShow)
{
    ghinst = hInstance;
    win::GetHwndDpi(NULL, &gUiDPIFactor);

    gHwndFrame = CreateWindow(
            ET_FRAME_CLASS_NAME, _T("Ebook Test ") CURR_VERSION_STR,
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT,
            dpiAdjust(WIN_DX), dpiAdjust(WIN_DY),
            NULL, NULL,
            ghinst, NULL);

    if (!gHwndFrame)
        return FALSE;

    CenterDialog(gHwndFrame);
    ShowWindow(gHwndFrame, SW_SHOW);

    return TRUE;
}

void DispatchUiMsg(UiMsg *msg)
{
    if (UiMsg::FinishedMobiLoading == msg->type) {
        gEbookController->HandleFinishedMobiLoadingMsg(msg);
    } else if (UiMsg::MobiLayout == msg->type) {
        gEbookController->HandleMobiLayoutMsg(msg);
    } else {
        CrashIf(true);
    }
    delete msg;
}

void DispatchUiMessages()
{
    for (UiMsg *msg = uimsg::RetriveNext(); msg; msg = uimsg::RetriveNext()) {
        DispatchUiMsg(msg);
    }
}

void DrainUiMsgQueu()
{
    for (UiMsg *msg = uimsg::RetriveNext(); msg; msg = uimsg::RetriveNext()) {
        delete msg;
    }
}

// inspired by http://engineering.imvu.com/2010/11/24/how-to-write-an-interactive-60-hz-desktop-application/
static int RunApp()
{
    MSG msg;
    FrameTimeoutCalculator ftc(60);
    for (;;) {
        DWORD timeout = ftc.GetTimeoutInMilliseconds();
        DWORD res = WAIT_TIMEOUT;
        HANDLE handles[MAXIMUM_WAIT_OBJECTS];
        DWORD handleCount = 0;
        handles[handleCount++] = uimsg::GetQueueEvent();
        CrashIf(handleCount >= MAXIMUM_WAIT_OBJECTS);
        if ((timeout > 0) || (handleCount > 0)) {
            if (0 == timeout)
                timeout = 1000;
            res = MsgWaitForMultipleObjects(handleCount, handles, FALSE, timeout, QS_ALLEVENTS);
        }

        if (res == WAIT_OBJECT_0) {
            DispatchUiMessages();
        }

#if 0
         if (res == WAIT_TIMEOUT) {
            ftc.Step();
        }
#endif

        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                return (int)msg.wParam;
            }
            if (!IsDialogMessage(gHwndFrame, &msg)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
        DispatchUiMessages();
    }
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    int ret = 1;
    LogProcessRunningTime();

#ifdef DEBUG
    // report memory leaks on DbgOut
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    SetErrorMode(SEM_NOOPENFILEERRORBOX | SEM_FAILCRITICALERRORS);
    // ensure that C functions behave consistently under all OS locales
    // (use Win32 functions where localized input or output is desired)
    setlocale(LC_ALL, "C");

#ifdef DEBUG
    extern void SvgPath_UnitTests();
    SvgPath_UnitTests();
    extern void TrivialHtmlParser_UnitTests();
    TrivialHtmlParser_UnitTests();
    extern void BaseUtils_UnitTests();
    BaseUtils_UnitTests();
    extern void HtmlPullParser_UnitTests();
    HtmlPullParser_UnitTests();
#endif
    ScopedCom com;
    InitAllCommonControls();
    ScopedGdiPlus gdi;

    mui::Initialize();

    if (!RegisterWinClass(hInstance))
        goto Exit;

    if (!InstanceInit(hInstance, nCmdShow))
        goto Exit;

    uimsg::Initialize();
    ret = RunApp();

Exit:
    delete gEbookController;
    DestroyEbookControls(gEbookControls);
    mui::Destroy();

    DrainUiMsgQueu();
    uimsg::Destroy();
    return ret;
}
