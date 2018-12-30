#include "utils/BaseUtil.h"

#include "utils/WinUtil.h"
#include "Win32Window.h"
#include "utils/FileUtil.h"
#include "EditCtrl.h" // TODO: for MsgFilter

#define WIN_CLASS L"WC_WIN32_WINDOW"

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (WM_CREATE == msg) {
        CREATESTRUCT* cs = (CREATESTRUCT*)lp;
        Win32Window* w = (Win32Window*)cs->lpCreateParams;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)w);
        return DefWindowProc(hwnd, msg, wp, lp);
    }

    Win32Window* w = (Win32Window*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    // don't allow intercepting those messages
    if (WM_DESTROY == msg) {
        PostQuitMessage(0);
        return 0;
    }

    if (WM_NCDESTROY == msg) {
        free(w);
        return DefWindowProc(hwnd, msg, wp, lp);
    }
    if (!w) {
        goto Exit;
    }

    if (w->preFilter) {
        bool discardMsg = false;
        LRESULT res = w->preFilter(w, msg, wp, lp, discardMsg);
        if (discardMsg)
            return res;
    }

    if ((WM_COMMAND == msg) && w->onCommand) {
        bool discardMsg = false;
        LRESULT res = w->onCommand(w, LOWORD(wp), HIWORD(wp), lp, discardMsg);
        if (discardMsg)
            return res;
        return DefWindowProc(hwnd, msg, wp, lp);
    }

    if ((WM_SIZE == msg) && w->onSize) {
        int dx = LOWORD(lp);
        int dy = HIWORD(lp);
        w->onSize(w, dx, dy, wp);
        return 0;
    }

Exit:
    if (WM_PAINT == msg) {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        EndPaint(hwnd, &ps);
        return 0;
    }

    return DefWindowProc(hwnd, msg, wp, lp);
}

static void RegisterClass() {
    static ATOM atom = 0;

    if (atom != 0) {
        // already registered
        return;
    }

    WNDCLASSEXW wcex = {};
    FillWndClassEx(wcex, WIN_CLASS, WndProc);
    atom = RegisterClassExW(&wcex);
    CrashIf(!atom);
}

Win32Window::Win32Window(HWND parent, RECT* initialPosition) {
    this->parent = parent;
    if (initialPosition) {
        this->initialPos = *initialPosition;
    }

    this->dwExStyle = 0;
    this->dwStyle = WS_OVERLAPPEDWINDOW;
    if (parent != nullptr) {
        this->dwStyle |= WS_CHILD;
    }
}

bool Win32Window::Create(const WCHAR* title) {
    RegisterClass();

    RECT rc = this->initialPos;
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
    this->hwnd = CreateWindowExW(this->dwExStyle, WIN_CLASS, title, this->dwStyle, x, y, dx, dy, this->parent, nullptr,
                                 GetInstance(), (void*)this);

    return this->hwnd != nullptr;
}

Win32Window::~Win32Window() {
    // we free w in WM_DESTROY
    DestroyWindow(this->hwnd);
}
