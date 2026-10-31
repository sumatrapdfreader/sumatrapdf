/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/GdiPlusUtil.h"
#include "base/WinDynCalls.h"

#include "gui/Dpi.h"

#include "WebView2.h"

#include "WebViewCapture.h"

// png from a png stream, less cropDx pixels on the right; {} on failure
static TempStr CropPngTemp(IStream* src, int cropDx) {
    Gdiplus::Bitmap bmp(src);
    int dx = (int)bmp.GetWidth() - cropDx;
    int dy = (int)bmp.GetHeight();
    if (bmp.GetLastStatus() != Gdiplus::Ok || dx <= 0 || dy <= 0) {
        return {};
    }
    Gdiplus::Bitmap* cropped = bmp.Clone(0, 0, dx, dy, PixelFormat32bppARGB);
    IStream* dst = nullptr;
    TempStr res;
    if (cropped && SUCCEEDED(CreateStreamOnHGlobal(nullptr, TRUE, &dst))) {
        CLSID png = GetGdiPlusEncoderClsid(WStrL(L"image/png"));
        HGLOBAL mem = nullptr;
        STATSTG st{};
        bool ok = cropped->Save(dst, &png, nullptr) == Gdiplus::Ok && SUCCEEDED(GetHGlobalFromStream(dst, &mem)) &&
                  SUCCEEDED(dst->Stat(&st, STATFLAG_NONAME));
        void* d = ok ? GlobalLock(mem) : nullptr;
        if (d) {
            res = str::DupTemp(Str((char*)d, (int)st.cbSize.QuadPart));
            GlobalUnlock(mem);
        }
        dst->Release();
    }
    delete cropped;
    return res;
}

// owns the stream CapturePreview writes the png into; done gets the png without
// the vertical scrollbar (always cropped: we don't know if the page has one)
class webview2_capture_handler : public ICoreWebView2CapturePreviewCompletedHandler {
  public:
    webview2_capture_handler(IStream* stream, int scrollbarDx, const Func1<Str>& done)
        : m_stream(stream), m_scrollbarDx(scrollbarDx), m_done(done) {}
    ~webview2_capture_handler() { m_stream->Release(); }
    ULONG STDMETHODCALLTYPE AddRef() { return ++m_refCount; }
    ULONG STDMETHODCALLTYPE Release() {
        ULONG n = --m_refCount;
        if (n == 0) {
            delete this;
        }
        return n;
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, LPVOID* ppv) {
        if (!ppv) {
            return E_POINTER;
        }
        *ppv = nullptr;
        if (riid == IID_IUnknown || riid == __uuidof(ICoreWebView2CapturePreviewCompletedHandler)) {
            *ppv = static_cast<ICoreWebView2CapturePreviewCompletedHandler*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }
    HRESULT STDMETHODCALLTYPE Invoke(HRESULT errorCode) {
        TempStr png;
        LARGE_INTEGER start{};
        if (SUCCEEDED(errorCode) && SUCCEEDED(m_stream->Seek(start, STREAM_SEEK_SET, nullptr))) {
            png = CropPngTemp(m_stream, m_scrollbarDx);
        }
        m_done.Call(png);
        return S_OK;
    }

  private:
    IStream* m_stream = nullptr;
    int m_scrollbarDx = 0;
    Func1<Str> m_done;
    ULONG m_refCount = 1;
};

// a png of what the webview in hwnd shows, given to done asynchronously ({} on
// failure). Returns false if it can't be started (done is then never called)
bool WebView2CapturePng(ICoreWebView2* webview, HWND hwnd, const Func1<Str>& done) {
    if (!webview) {
        return false;
    }
    IStream* stream = nullptr;
    if (FAILED(CreateStreamOnHGlobal(nullptr, TRUE, &stream))) {
        return false;
    }
    int scrollbarDx = DynGetSystemMetricsForDpi ? DynGetSystemMetricsForDpi(SM_CXVSCROLL, DpiGetForHwnd(hwnd))
                                                : GetSystemMetrics(SM_CXVSCROLL);
    auto* handler = new webview2_capture_handler(stream, scrollbarDx, done);
    HRESULT hr = webview->CapturePreview(COREWEBVIEW2_CAPTURE_PREVIEW_IMAGE_FORMAT_PNG, stream, handler);
    handler->Release();
    return SUCCEEDED(hr);
}
