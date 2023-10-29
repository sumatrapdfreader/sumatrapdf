/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct ScopedCritSec {
    CRITICAL_SECTION* cs = nullptr;

    explicit ScopedCritSec(CRITICAL_SECTION* cs) : cs(cs) {
        EnterCriticalSection(cs);
    }
    ~ScopedCritSec() {
        LeaveCriticalSection(cs);
    }
};

class AutoCloseHandle {
    HANDLE handle = nullptr;

  public:
    AutoCloseHandle() = default;

    AutoCloseHandle(HANDLE h) : handle(h) {
    }

    ~AutoCloseHandle() {
        if (IsValid()) {
            CloseHandle(handle);
        }
    }

    AutoCloseHandle& operator=(HANDLE h) {
        CrashIf(handle != nullptr);
        CrashIf(h == nullptr);
        handle = h;
        return *this;
    }

    operator HANDLE() const { // NOLINT
        return handle;
    }

    bool IsValid() const {
        return handle != nullptr && handle != INVALID_HANDLE_VALUE;
    }
};

template <class T>
class ScopedComPtr {
  protected:
    T* ptr = nullptr;

  public:
    ScopedComPtr() = default;

    explicit ScopedComPtr(T* ptr) : ptr(ptr) {
    }
    ~ScopedComPtr() {
        if (ptr) {
            ptr->Release();
        }
    }
    bool Create(const CLSID clsid) {
        CrashIf(ptr);
        if (ptr) {
            return false;
        }
        HRESULT hr = CoCreateInstance(clsid, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&ptr));
        return SUCCEEDED(hr);
    }
    T* Get() const {
        return ptr;
    }
    operator T*() const { // NOLINT
        return ptr;
    }
    T** operator&() {
        return &ptr;
    }
    T* operator->() const {
        return ptr;
    }
    ScopedComPtr<T>& operator=(T* newPtr) {
        if (ptr) {
            ptr->Release();
        }
        ptr = newPtr;
        return *this;
    }
};

template <class T>
class ScopedComQIPtr {
  protected:
    T* ptr = nullptr;

  public:
    ScopedComQIPtr() = default;

    explicit ScopedComQIPtr(IUnknown* unk) {
        HRESULT hr = unk->QueryInterface(&ptr);
        if (FAILED(hr)) {
            ptr = nullptr;
        }
    }
    ~ScopedComQIPtr() {
        if (ptr) {
            ptr->Release();
        }
    }
    bool Create(const CLSID clsid) {
        CrashIf(ptr);
        if (ptr)
            return false;
        HRESULT hr = CoCreateInstance(clsid, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&ptr));
        return SUCCEEDED(hr);
    }
    T* operator=(IUnknown* newUnk) {
        if (ptr)
            ptr->Release();
        HRESULT hr = newUnk->QueryInterface(&ptr);
        if (FAILED(hr))
            ptr = nullptr;
        return ptr;
    }
    operator T*() const { // NOLINT
        return ptr;
    }
    T** operator&() {
        return &ptr;
    }
    T* operator->() const {
        return ptr;
    }
    T* operator=(T* newPtr) {
        if (ptr)
            ptr->Release();
        return (ptr = newPtr);
    }
};

struct AutoDeleteDC {
    HDC hdc = nullptr;

    explicit AutoDeleteDC(HDC hdc) {
        this->hdc = hdc;
    }
    AutoDeleteDC() = default;

    ~AutoDeleteDC() {
        DeleteDC(hdc);
    }
    operator HDC() const { // NOLINT
        return hdc;
    }
};

struct AutoReleaseDC {
    HWND hwnd = nullptr;
    HDC hdc = nullptr;

    explicit AutoReleaseDC(HWND hwnd) {
        hdc = GetWindowDC(hwnd);
    }
    AutoReleaseDC() = default;

    ~AutoReleaseDC() {
        ReleaseDC(hwnd, hdc);
    }
    operator HDC() const { // NOLINT
        return hdc;
    }
};

template <typename T>
class ScopedGdiObj {
    T obj;

  public:
    ScopedGdiObj(T obj) { // NOLINT
        this->obj = obj;
    }
    ~ScopedGdiObj() {
        DeleteObject(obj);
    }
    operator T() const { // NOLINT
        return obj;
    }
};
using AutoDeletePen = ScopedGdiObj<HPEN>;
using AutoDeleteBrush = ScopedGdiObj<HBRUSH>;

class ScopedGetDC {
    HDC hdc = nullptr;
    HWND hwnd = nullptr;

  public:
    explicit ScopedGetDC(HWND hwnd) {
        this->hwnd = hwnd;
        this->hdc = GetDC(hwnd);
    }
    ~ScopedGetDC() {
        ReleaseDC(hwnd, hdc);
    }
    operator HDC() const { // NOLINT
        return hdc;
    }
};

class ScopedSelectObject {
    HDC hdc = nullptr;
    HGDIOBJ obj = nullptr;
    HGDIOBJ prev = nullptr;

  public:
    ScopedSelectObject(HDC hdc, HGDIOBJ obj, bool alsoDelete = false) {
        this->hdc = hdc;
        this->prev = SelectObject(hdc, obj);
        if (alsoDelete) {
            this->obj = obj;
        }
    }

    ~ScopedSelectObject() {
        SelectObject(hdc, prev);
        if (obj) {
            DeleteObject(obj);
        }
    }
};

class ScopedSelectFont {
    HDC hdc = nullptr;
    HGDIOBJ prev = nullptr;

  public:
    // font can be nullptr
    explicit ScopedSelectFont(HDC hdc, HFONT font) {
        this->hdc = hdc;
        if (font) {
            prev = (HFONT)SelectObject(hdc, font);
        }
    }

    ~ScopedSelectFont() {
        if (prev) {
            SelectObject(hdc, prev);
        }
    }
};

struct ScopedSelectPen {
    HDC hdc = nullptr;
    HPEN prevPen = nullptr;

    explicit ScopedSelectPen(HDC hdc, HPEN pen) {
        prevPen = (HPEN)SelectObject(hdc, pen);
    }

    ~ScopedSelectPen() {
        SelectObject(hdc, prevPen);
    }
};

class ScopedSelectBrush {
    HDC hdc = nullptr;
    HBRUSH prevBrush = nullptr;

  public:
    explicit ScopedSelectBrush(HDC hdc, HBRUSH pen) {
        prevBrush = (HBRUSH)SelectObject(hdc, pen);
    }

    ~ScopedSelectBrush() {
        SelectObject(hdc, prevBrush);
    }
};
class ScopedCom {
  public:
    ScopedCom() {
        (void)CoInitialize(nullptr);
    }
    ~ScopedCom() {
        CoUninitialize();
    }
};

class ScopedOle {
  public:
    ScopedOle() {
        (void)OleInitialize(nullptr);
    }
    ~ScopedOle() {
        OleUninitialize();
    }
};

class ScopedGdiPlus {
  protected:
    Gdiplus::GdiplusStartupInput si{};
    Gdiplus::GdiplusStartupOutput so{};
    ULONG_PTR token = 0;
    ULONG_PTR hookToken = 0;
    bool noBgThread = false;

  public:
    // suppress the GDI+ background thread when initiating in WinMain,
    // as that thread causes DDE messages to be sent too early and
    // thus causes unexpected timeouts
    explicit ScopedGdiPlus(bool inWinMain = false) : noBgThread(inWinMain) {
        si.SuppressBackgroundThread = noBgThread;
        Gdiplus::GdiplusStartup(&token, &si, &so);
        if (noBgThread) {
            so.NotificationHook(&hookToken);
        }
    }
    ~ScopedGdiPlus() {
        if (noBgThread) {
            so.NotificationUnhook(hookToken);
        }
        Gdiplus::GdiplusShutdown(token);
    }
};
