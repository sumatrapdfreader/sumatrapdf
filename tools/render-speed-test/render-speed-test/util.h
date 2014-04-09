#ifndef util_h
#define util_h

// stuff that, once implemented, doesn't change often
namespace str {

size_t Utf8ToWcharBuf(const char *s, size_t sLen, WCHAR *bufOut, size_t cchBufOutSize);
char *DupN(char *s, size_t sLen);

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

static inline bool is_ws(char c) {
    // TODO: probably more white-space characters
    return c == ' ' ||
        c == '\t';
}

static inline char get_next(char*& s, char *end) {
    if (s >= end)
        return 0;
    return *s++;
}

static inline char peek_next(char*& s, char *end) {
    if (s >= end)
        return 0;
    return *s;
}

static inline void skip_ws(char*& s, char *end) {
    while (s < end && is_ws(*s))
        s++;
}

static inline bool is_word_end(char c) {
    return is_ws(c) || c == '\n' || c == 0;
}

#endif
