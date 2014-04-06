#include "stdafx.h"

#include "render-speed-test.h"

// a program to test various ways of measuring and drawing text
// TODO:
// - load a file with text to test
// - do layout and display
// - test GDI+
// - test GDI
// - test DirectDraw

HINSTANCE hInst;

typedef INT64 i64;
typedef UINT64 u64;

// TODO: static_assert(sizeof(i64)==8)
#define APP_TITLE       L"RenderSpeedTest"
#define APP_WIN_CLASS   L"RENDERSPEEDTEST_WIN_CLS"

#if 0
struct MeasuredString {
    const char *s;
    size_t sLen;
    SIZE measuredSize;
};
#endif

#if 0
struct Buf {
    char *s;
    u64 sLen;
    bool owned;

    Buf() {
        Reset();
    }

    void Reset() {
        s = nullptr;
        sLen = 0;
        owned = false;
    }

    explicit Buf(char *sIn, u64 sLenIn = (u64)-1, bool ownedIn = false) {
        Set(sIn, sLenIn, ownedIn);
    }
    void Free() {
        if (owned)
            free(s);
        Reset();
    }

    void Set(char *sIn, u64 sLenIn = (u64) -1, bool ownedIn = false) {
        Free();
        s = sIn;
        if (sLenIn == (u64) -1) {
            sLen = (u64) strlen(s);
        }
        else {
            sLen = sLenIn;
        }
        owned = ownedIn;
    }

    void TakeOwnership(char *sIn, u64 sLenIn = (u64) -1) {
        char *tmp = _strdup(sIn);
        Set(tmp, sLenIn, true);
    }

    ~Buf() {
        if (owned) {
            free(s);
        }
    }
};
#endif

inline bool is_ws(char c) {
    // TODO: probably more white-space characters
    return c == ' ' ||
        c == '\t';
}

inline char get_next(char*& s, char *end) {
    if (s >= end)
        return 0;
    return *s++;
}

inline char peek_next(char*& s, char *end) {
    if (s >= end)
        return 0;
    return *s;
}

void skip_ws(char*& s, char *end) {
    while (s < end && is_ws(*s))
        s++;
}

inline bool is_word_end(char c) {
    return is_ws(c) || c == '\n' || c == 0;
}

// normalizes newline characters in-place (i.e. replaces '\r' and "\r\n" with '\n')
// return the new end of the buffer (guaranteed to be <= end)
char *normalize_nl(char *, char *end) {
    // TODO: write me
    return end;
}

// iterates over words of the string and for each word calls a function f(char *s, size_t sLen)
// it collapses multile white-space characters as one.
// it emits newline as '\n' and normalizes '\r' and '\r\n' into '\n' (but doesn't collapse
// multiple new-lines into one)
template<typename Func>
void IterWords(char *s, size_t sLen, Func f) {
    char *end = s + sLen;
    // TODO: could possibly be faster by normalizing nl while we go,
    // but it would complicate the code
    end = normalize_nl(s, end);
    char c;
    char *currStart;
    size_t nWords = 0;
    size_t nLines = 0;
    for (;;) {
        skip_ws(s, end);
        currStart = s;
        for (;;) {
            c = get_next(s, end);
            if (is_word_end(c))
                break;
        }
        auto len = s - currStart - 1;
        if (len > 0)
            f(currStart, len);
        nWords++;
        if (c == '\n') {
            f("\n", 1);
            nLines++;
        }
        if (0 == c)
            break;
    }
}

char *strndup(char *s, size_t sLen) {
    auto res = (char*) malloc(sLen + 1);
    if (!res)
        return NULL;
    memcpy(res, s, sLen);
    res[sLen] = 0;
    return res;
}

void DoLayout(char *s) {
    auto sLen = strlen(s);
    IterWords(s, sLen, [](char *s, size_t sLen) {
        auto tmp = strndup(s, sLen+1);
        tmp[sLen] = '\n';
        OutputDebugStringA(tmp);
        free(tmp);
    });
}

// http://kennykerr.ca/2014/03/29/classy-windows-2/
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
    SampleWindow()
    {
        WNDCLASS wc = {};

        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hInstance = hInst;
        wc.lpszClassName = L"Render_Speed_Test_Wnd_Cls";
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = WndProc;

        RegisterClass(&wc);

        //ASSERT(!m_window);

        CreateWindowW(wc.lpszClassName,
            L"Render Speed Test",
            WS_OVERLAPPEDWINDOW | WS_VISIBLE,
            CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
            nullptr,
            nullptr,
            wc.hInstance,
            this);

        //ASSERT(m_window);
    }

    LRESULT MessageHandler(UINT message, WPARAM wparam, LPARAM lparam)
    {
        if (WM_PAINT == message)
        {
            PaintHandler();
            return 0;
        }

        return Window::MessageHandler(message, wparam, lparam);
    }

    void PaintHandler()
    {
        static auto didLayout = false;
        if (!didLayout) {
            DoLayout(SAMPLE_TEXT);
            didLayout = true;
        }
#if 0
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(m_window, &ps);
        // TODO: draw
        EndPaint(m_window, &ps);
#endif
    }
};

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE,
                     _In_ LPWSTR,
                     _In_ int )
{
    hInst = hInstance;

    //MSG msg;
    //HACCEL hAccelTable;

    SampleWindow window;
    MSG message;

    while (GetMessage(&message, nullptr, 0, 0))
    {
        DispatchMessage(&message);
    }

    /*
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
    */
    return 0;
}
