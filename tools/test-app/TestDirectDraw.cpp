#include "utils/BaseUtil.h"
#include "utils/WinUtil.h"

#include "wingui/WinGui.h"
#include "wingui/Layout.h"
#include "wingui/Window.h"

#include <d2d1.h>

#include "test-app.h"

static HINSTANCE hInst;
static const WCHAR *WIN_CLASS = L"DirectDrawTestWndCls";
static const WCHAR *gWindowTitle = L"Test application";

static ID2D1Factory* gD2DFactory = nullptr;

static INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNUSED(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

static void InitD2D() {
    if (gD2DFactory != nullptr)
        return;

    HRESULT hr = D2D1CreateFactory(
        D2D1_FACTORY_TYPE_SINGLE_THREADED,
        &gD2DFactory
    );
    CrashAlwaysIf(FAILED(hr));
}

static ID2D1HwndRenderTarget* gRT = NULL;

#if 0
static bool CreateSurface(HWND hwnd, RECT rc) {
    auto size = D2D1::SizeU(RectDx(rc), RectDy(rc));
    HRESULT hr = gD2DFactory->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(),
        D2D1::HwndRenderTargetProperties(hwnd, size),
        &gRT);
    CrashAlwaysIf(FAILED(hr));
    return SUCCEEDED(hr);
}

static void ReleaseSurface() {
    if (gRT) {
        gRT->Release();
        gRT = nullptr;
    }
}
#endif

class ScopedSurface {
public:
    ScopedSurface(HWND hwnd, RECT rc) {
        rt = nullptr;
        auto size = D2D1::SizeU(RectDx(rc), RectDy(rc));
        HRESULT hr = gD2DFactory->CreateHwndRenderTarget(
            D2D1::RenderTargetProperties(),
            D2D1::HwndRenderTargetProperties(hwnd, size),
            &rt);
        CrashAlwaysIf(FAILED(hr));
    }

    ~ScopedSurface() {
        if (rt) {
            rt->Release();
            rt = nullptr;
        }
    }

    ID2D1HwndRenderTarget* get() {
        return rt;
    }

    ID2D1HwndRenderTarget *rt;
};

class ScopedSolidColorBrush {
public:
    ScopedSolidColorBrush(ID2D1HwndRenderTarget *rt, const D2D1::ColorF& col) {
        HRESULT hr = rt->CreateSolidColorBrush(col, &brush);
        CrashAlwaysIf(FAILED(hr));
    }

    ~ScopedSolidColorBrush() {
        if (brush) {
            brush->Release();
            brush = nullptr;
        }
    }

    ID2D1SolidColorBrush* get() {
        return brush;
    }

    ID2D1SolidColorBrush* brush;
};

static void Draw(HWND hwnd) {
    RECT rc = GetClientRect(hwnd);

    auto rt = ScopedSurface(hwnd, rc);
    auto br = ScopedSolidColorBrush(rt.get(), D2D1::ColorF(D2D1::ColorF::Blue));
    auto bgCol = ScopedSolidColorBrush(rt.get(), D2D1::ColorF(D2D1::ColorF::White));

    auto surface = rt.get();
    surface->BeginDraw();
    surface->SetTransform(D2D1::Matrix3x2F::Identity());
    surface->Clear(D2D1::ColorF(D2D1::ColorF::LightBlue));
    //auto r = D2D1::RectF(0.0f, 0.0f, (float)RectDx(rc), (float)RectDy(rc));
    //surface->FillRectangle(r, bgCol.get());

    auto r = D2D1::RectF(
        rc.left + 100.5f,
        rc.top + 100.5f,
        rc.right - 100.5f,
        rc.bottom - 100.5f);
    surface->DrawRectangle(r, br.get(), 5.0f);
    HRESULT hr = surface->EndDraw();
    CrashAlwaysIf(FAILED(hr));
}

static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
    {
        InitD2D();
    }
    break;

    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
        switch (wmId)
        {
        case IDM_ABOUT:
            DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
            break;
        case IDM_EXIT:
            DestroyWindow(hWnd);
            break;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
    }
    break;

    case WM_PAINT:
    {
        Draw(hWnd);
        ValidateRect(hWnd, NULL);
    }
    break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

static ATOM RegisterWinClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_TESTWIN));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_TESTWIN);
    wcex.lpszClassName = WIN_CLASS;
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));
    return RegisterClassExW(&wcex);
}

static BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    hInst = hInstance;
    const WCHAR *cls = WIN_CLASS;
    HWND hWnd = CreateWindowW(cls, gWindowTitle, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

    if (!hWnd)
        return FALSE;

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);
    return TRUE;
}

int TestDirectDraw(HINSTANCE hInstance, int nCmdShow) {
    RegisterWinClass(hInstance);

    if (!InitInstance(hInstance, nCmdShow)) {
        CrashAlwaysIf(true);
        return FALSE;
    }

    HACCEL accelTable = LoadAccelerators(hInst, MAKEINTRESOURCE(IDC_TESTWIN));
    auto res = RunMessageLoop(accelTable);
    return res;
}
