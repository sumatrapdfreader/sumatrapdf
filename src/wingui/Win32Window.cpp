#include "BaseUtil.h"

#include "WinUtil.h"
#include "Win32Window.h"
#include "FileUtil.h"
#include "EditCtrl.h" // TODO: for MsgFilter

#define WIN_CLASS L"WC_WIN32_WINDOW"

static ATOM wndClass = 0;

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (WM_CREATE == msg) {
        CREATESTRUCT *cs = (CREATESTRUCT *)lp;
        Win32Window *w = (Win32Window *)cs->lpCreateParams;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)w);
        return DefWindowProc(hwnd, msg, wp, lp);
    }

    Win32Window *w = (Win32Window *)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    // don't allow intercepting those messages
    if (WM_DESTROY == msg) {
        PostQuitMessage(0);
        return 0;
    }

    if (WM_NCDESTROY == msg) {
        free(w);
        return DefWindowProc(hwnd, msg, wp, lp);
    }

    if (w && w->preFilter) {
        bool discardMsg = false;
        LRESULT res = w->preFilter(w, msg, wp, lp, discardMsg);
        if (discardMsg)
            return res;
    }

    if (w && (WM_COMMAND == msg) && w->onCommand) {
        bool discardMsg = false;
        LRESULT res = w->onCommand(w, LOWORD(wp), HIWORD(wp), lp, discardMsg);
        if (discardMsg)
            return res;
        return DefWindowProc(hwnd, msg, wp, lp);
    }

    if (w && (WM_SIZE == msg) && w->onSize) {
        int dx = LOWORD(lp);
        int dy = HIWORD(lp);
        w->onSize(w, dx, dy, wp);
        return 0;
    }

    if (WM_PAINT == msg) {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        EndPaint(hwnd, &ps);
        return 0;
    }

    return DefWindowProc(hwnd, msg, wp, lp);
}

static ATOM RegisterClass(Win32Window *w) {
    if (wndClass != 0) {
        return wndClass;
    }

    WNDCLASSEXW wcex = { 0 };
    auto hInst = GetInstance();
    wcex.cbSize = sizeof(WNDCLASSEXW);

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInst;
    wcex.hIcon = w->hIcon;
    wcex.hIconSm = w->hIconSm;
    wcex.lpszMenuName = w->lpszMenuName;
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszClassName = WIN_CLASS;

    wndClass = RegisterClassExW(&wcex);
    return wndClass;
}

void InitWin32Window(Win32Window *w, HWND parent, RECT *initialPosition) {
    w->parent = parent;
    if (initialPosition) {
        w->initialPos = *initialPosition;
    }

    w->dwExStyle = 0;
    w->dwStyle = WS_OVERLAPPEDWINDOW;
    if (parent != nullptr) {
        w->dwStyle |= WS_CHILD;
    }
}

Win32Window *AllocWin32Window(HWND parent, RECT *initialPosition) {
    auto w = AllocStruct<Win32Window>();
    InitWin32Window(w, parent, initialPosition);
    return w;
}

bool CreateWin32Window(Win32Window *w, const WCHAR *title) {
    RegisterClass(w);

    RECT rc = w->initialPos;
    int x = rc.left;
    int y = rc.top;
    int dx = RectDx(rc);
    int dy = RectDy(rc);
    if (dx == 0) {
        x = CW_USEDEFAULT;
        y = CW_USEDEFAULT;
        dx = CW_USEDEFAULT;
        dy = CW_USEDEFAULT;
    }
    w->hwnd = CreateWindowExW(w->dwExStyle, WIN_CLASS, title, w->dwStyle, x, y, dx, dy, w->parent,
                              nullptr, GetInstance(), (void *)w);

    return w->hwnd != nullptr;
}

void DeleteWin32Window(Win32Window *w) {
    if (!w)
        return;

    // we free w in WM_DESTROY
    DestroyWindow(w->hwnd);
}
