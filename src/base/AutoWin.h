/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

class AutoCloseHandle {
    HANDLE handle = nullptr;

  public:
    AutoCloseHandle() = default;

    AutoCloseHandle(HANDLE h) : handle(h) {}

    ~AutoCloseHandle() {
        if (IsValid()) {
            CloseHandle(handle);
        }
    }

    AutoCloseHandle& operator=(HANDLE h) {
        ReportIf(handle != nullptr);
        ReportIf(h == nullptr);
        handle = h;
        return *this;
    }

    operator HANDLE() const { // NOLINT
        return handle;
    }

    bool IsValid() const { return handle != nullptr && handle != INVALID_HANDLE_VALUE; }
};

template <class T>
class AutoReleaseComPtr {
  protected:
    T* ptr = nullptr;

  public:
    AutoReleaseComPtr() = default;

    explicit AutoReleaseComPtr(T* ptr) : ptr(ptr) {}
    ~AutoReleaseComPtr() {
        if (ptr) {
            ptr->Release();
        }
    }
    bool Create(const CLSID clsid) {
        ReportIf(ptr);
        if (ptr) {
            return false;
        }
        HRESULT hr = CoCreateInstance(clsid, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&ptr));
        return SUCCEEDED(hr);
    }
    T* Get() const { return ptr; }
    operator T*() const { // NOLINT
        return ptr;
    }
    T** operator&() { return &ptr; }
    T* operator->() const { return ptr; }
    AutoReleaseComPtr<T>& operator=(T* newPtr) {
        if (ptr) {
            ptr->Release();
        }
        ptr = newPtr;
        return *this;
    }
};

template <class T>
class AutoReleaseComQIPtr {
  protected:
    T* ptr = nullptr;

  public:
    AutoReleaseComQIPtr() = default;

    explicit AutoReleaseComQIPtr(IUnknown* unk) {
        HRESULT hr = unk->QueryInterface(&ptr);
        if (FAILED(hr)) {
            ptr = nullptr;
        }
    }
    ~AutoReleaseComQIPtr() {
        if (ptr) {
            ptr->Release();
        }
    }
    bool Create(const CLSID clsid) {
        ReportIf(ptr);
        if (ptr) return false;
        HRESULT hr = CoCreateInstance(clsid, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&ptr));
        return SUCCEEDED(hr);
    }
    AutoReleaseComQIPtr<T>& operator=(IUnknown* newUnk) {
        if (ptr) {
            ptr->Release();
        }
        HRESULT hr = newUnk->QueryInterface(&ptr);
        if (FAILED(hr)) {
            ptr = nullptr;
        }
        return *this;
    }
    operator T*() const { // NOLINT
        return ptr;
    }
    T** operator&() { return &ptr; }
    T* operator->() const { return ptr; }
    AutoReleaseComQIPtr<T>& operator=(T* newPtr) {
        if (ptr) {
            ptr->Release();
        }
        ptr = newPtr;
        return *this;
    }
};

struct AutoDeleteDC {
    HDC hdc = nullptr;

    explicit AutoDeleteDC(HDC hdc) { this->hdc = hdc; }
    AutoDeleteDC() = default;

    ~AutoDeleteDC() { DeleteDC(hdc); }
    operator HDC() const { // NOLINT
        return hdc;
    }
};

template <typename T>
class AutoDeleteGdiObj {
    T obj;

  public:
    AutoDeleteGdiObj(T obj) { // NOLINT
        this->obj = obj;
    }
    ~AutoDeleteGdiObj() { DeleteObject(obj); }
    operator T() const { // NOLINT
        return obj;
    }
};
using AutoDeletePen = AutoDeleteGdiObj<HPEN>;
using AutoDeleteBrush = AutoDeleteGdiObj<HBRUSH>;
using AutoDeleteObject = AutoDeleteGdiObj<HGDIOBJ>;

class AutoReleaseDC {
    HDC hdc = nullptr;
    HWND hwnd = nullptr;

  public:
    explicit AutoReleaseDC(HWND hwnd) {
        this->hwnd = hwnd;
        this->hdc = GetDC(hwnd);
    }
    ~AutoReleaseDC() { ReleaseDC(hwnd, hdc); }
    operator HDC() const { // NOLINT
        return hdc;
    }
};

class AutoRestoreGdiObject {
    HDC hdc = nullptr;
    HGDIOBJ obj = nullptr;
    HGDIOBJ prev = nullptr;

  public:
    AutoRestoreGdiObject(HDC hdc, HGDIOBJ obj, bool alsoDelete = false) {
        this->hdc = hdc;
        this->prev = SelectObject(hdc, obj);
        if (alsoDelete) {
            this->obj = obj;
        }
    }

    ~AutoRestoreGdiObject() {
        SelectObject(hdc, prev);
        if (obj) {
            DeleteObject(obj);
        }
    }
};

class AutoRestoreFont {
    HDC hdc = nullptr;
    HGDIOBJ prev = nullptr;

  public:
    // font can be nullptr
    explicit AutoRestoreFont(HDC hdc, HFONT font) {
        this->hdc = hdc;
        if (font) {
            prev = SelectObject(hdc, font);
        }
    }

    ~AutoRestoreFont() {
        if (prev) {
            SelectObject(hdc, prev);
        }
    }
};

struct AutoRestorePen {
    HDC hdc = nullptr;
    HPEN prevPen = nullptr;

    explicit AutoRestorePen(HDC hdc, HPEN pen) : hdc(hdc) { this->prevPen = (HPEN)SelectObject(hdc, pen); }

    ~AutoRestorePen() { SelectObject(hdc, prevPen); }
};

class AutoRestoreBrush {
    HDC hdc = nullptr;
    HBRUSH prevBrush = nullptr;

  public:
    explicit AutoRestoreBrush(HDC hdc, HBRUSH brush) : hdc(hdc) { prevBrush = (HBRUSH)SelectObject(hdc, brush); }

    ~AutoRestoreBrush() { SelectObject(hdc, prevBrush); }
};
// CoUninitialize() / OleUninitialize() must only be called when the matching
// Initialize succeeded. On failure (RPC_E_CHANGED_MODE when the thread is
// already in the other apartment kind) it would decrement an apartment count
// we never incremented, tearing COM down for the whole thread while other code
// still expects it. S_FALSE ("already initialized") is a success and does need
// the matching Uninitialize, so test with SUCCEEDED, not == S_OK.
class AutoCoUninitialize {
  public:
    HRESULT hr;
    AutoCoUninitialize() { hr = CoInitialize(nullptr); }
    ~AutoCoUninitialize() {
        if (SUCCEEDED(hr)) {
            CoUninitialize();
        }
    }
};

class AutoOleUninitialize {
  public:
    HRESULT hr;
    AutoOleUninitialize() { hr = OleInitialize(nullptr); }
    ~AutoOleUninitialize() {
        if (SUCCEEDED(hr)) {
            OleUninitialize();
        }
    }
};

class AutoGdiPlusShutdown {
  protected:
    Gdiplus::GdiplusStartupInput si;
    Gdiplus::GdiplusStartupOutput so;
    ULONG_PTR token = 0;
    ULONG_PTR hookToken = 0;
    bool noBgThread = false;

  public:
    // suppress the GDI+ background thread when initiating in WinMain,
    // as that thread causes DDE messages to be sent too early and
    // thus causes unexpected timeouts
    explicit AutoGdiPlusShutdown(bool inWinMain = false) : noBgThread(inWinMain) {
        si.SuppressBackgroundThread = noBgThread;
        Gdiplus::GdiplusStartup(&token, &si, &so);
        if (noBgThread) {
            so.NotificationHook(&hookToken);
        }
    }
    ~AutoGdiPlusShutdown() {
        if (noBgThread) {
            so.NotificationUnhook(hookToken);
        }
        Gdiplus::GdiplusShutdown(token);
    }
};
