/*
The purpose of this is to have a skeleton of an app for easy testing ideas.
Main benefit over adding code to Sumatra is that it's much smaller and compiles
much more quickly.
*/
#include "stdafx.h"
#include "test-app.h"

HINSTANCE hInst;


// http://kennykerr.ca/2014/03/29/classy-windows-2/s
template <typename T>
struct Window
{
    HWND m_window = nullptr;

    static T * GetThisFromHandle(HWND window)
    {
        return reinterpret_cast<T *>(GetWindowLongPtr(window,
            GWLP_USERDATA));
    }

    static LRESULT __stdcall WndProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
    {
        //ASSERT(window);

        if (WM_NCCREATE == message)
        {
            CREATESTRUCT * cs = reinterpret_cast<CREATESTRUCT *>(lparam);
            T * that = static_cast<T *>(cs->lpCreateParams);
            //ASSERT(that);
            //ASSERT(!that->m_window);
            that->m_window = window;
            SetWindowLongPtr(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(that));
        }
        else if (T * that = GetThisFromHandle(window))
        {
            return that->MessageHandler(message, wparam, lparam);
        }

        return DefWindowProc(window, message, wparam, lparam);
    }

    LRESULT MessageHandler(UINT message, WPARAM wparam, LPARAM lparam)
    {
        if (WM_DESTROY == message)
        {
            PostQuitMessage(0);
            return 0;
        }

        return DefWindowProc(m_window, message, wparam, lparam);
    }
};

struct SampleWindow : Window<SampleWindow>
{
    SampleWindow(const WCHAR *title)
    {
        WNDCLASS wc = {};

        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hInstance = hInst;
        wc.lpszClassName = APP_WIN_CLASS;
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = WndProc;

        RegisterClass(&wc);

        CreateWindowW(wc.lpszClassName,
            title,
            WS_OVERLAPPEDWINDOW | WS_VISIBLE,
            CW_USEDEFAULT, CW_USEDEFAULT, 720, 480,
            nullptr,
            nullptr,
            wc.hInstance,
            this);
    }

    ~SampleWindow() {
    }

    LRESULT MessageHandler(UINT message, WPARAM wparam, LPARAM lparam)
    {
        if (WM_PAINT == message)
        {
            PaintHandler();
            return 0;
        }

        if (WM_ERASEBKGND == message) {
            // do nothing, helps to avoid flicker
            return TRUE;
        }

        return Window::MessageHandler(message, wparam, lparam);
    }

    void PaintHandlerGdiplus()
    {
#if 0
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(m_window, &ps);
        ClientRect rcClient(m_window);
        RECT rTmp = rcClient.ToRECT();
        ScopedGdiObj<HBRUSH> brushAboutBg(CreateSolidBrush(RGB(0xff, 0xff, 0xff)));
        FillRect(hdc, &rTmp, brushAboutBg);
        Pen                  debugPen(Color(255, 0, 0), 1);
        EndPaint(m_window, &ps);
#endif
    }

    void PaintHandler()
    {
        PaintHandlerGdiplus();
    }
};

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE,
                     _In_ LPWSTR,
                     _In_ int )
{
    hInst = hInstance;

#if 0
    ScopedCom initCom;
    InitAllCommonControls();
    ScopedGdiPlus initGdiplus;
#endif
    MSG msg;
    HACCEL hAccelTable;

    SampleWindow window(L"Test app");

    hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_RENDERSPEEDTEST));
    while (GetMessage(&msg, NULL, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    return (int) msg.wParam;
}
