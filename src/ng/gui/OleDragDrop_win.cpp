/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// orig's OLE drag and drop (Canvas.cpp): dragging the selected text or an
// image out of the canvas, and taking dropped files / image URLs.
// ng: gpui has no drag out of a window and only surfaces dropped files, so
// this talks to OLE directly on the frame's HWND (the one gui/NativeWindow.cpp
// subclasses). Windows only.

#include "base/Base.h"
#include "gui/Dpi.h"
#include "base/File.h"
#include "base/UITask.h"
#include "base/Win.h"
#include "base/Http.h"
#include "base/Pixmap.h"
#include "base/GdiPlusUtil.h"
#include "base/GuessFileType.h"

#include <shlobj.h>
#include <commctrl.h>

#include "gui/UIModels.h"
#include "Settings.h"
#include "DisplayMode.h"
#include "DocController.h"
#include "EngineBase.h"
#include "EngineAll.h"
#include "DisplayModel.h"
#include "ProgressUpdateUI.h"
#include "TextSelection.h"
#include "TextSearch.h"
#include "WindowTab.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "Selection.h"
#include "PdfTools.h"
#include "gui/AppShell.h"
#include "gui/ToolWindow.h"
#include "gui/OleDragDrop.h"

#include "SumatraLog.h"

// ng: for the automation channel, which must never drop anything on whatever
// window the real cursor is over: the drag is cancelled by its drop source as
// soon as OLE asks, as if Esc was pressed
static bool gTestCancelDrag = false;
static HRESULT gLastDragResult = E_FAIL;
static int gLastDragTextLen = 0;

// OLE drag-drop support for dragging selected text out of the window
class TextDropSource : public IDropSource {
    AtomicInt refCount = 1;

  public:
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (riid == IID_IUnknown || riid == IID_IDropSource) {
            *ppv = this;
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return AtomicIntInc(&refCount); }
    ULONG STDMETHODCALLTYPE Release() override {
        LONG r = InterlockedDecrement(&refCount);
        if (r == 0) {
            delete this;
        }
        return r;
    }
    STDMETHODIMP QueryContinueDrag(BOOL fEscapePressed, DWORD grfKeyState) override {
        if (fEscapePressed || gTestCancelDrag) {
            return DRAGDROP_S_CANCEL;
        }
        if (!(grfKeyState & MK_LBUTTON)) {
            return DRAGDROP_S_DROP;
        }
        return S_OK;
    }
    STDMETHODIMP GiveFeedback(__unused DWORD dwEffect) override { return DRAGDROP_S_USEDEFAULTCURSORS; }
};

// Drop source that paints a proportional thumbnail via ImageList_BeginDrag
// (IDragSourceHelper does not show a drag image for our custom IDataObject).
class ImageDropSource : public IDropSource {
    AtomicInt refCount = 1;
    HIMAGELIST himl = nullptr;
    bool dragStarted = false;

  public:
    explicit ImageDropSource(HIMAGELIST list) : himl(list) {}
    ~ImageDropSource() {
        EndImageListDrag();
        if (himl) {
            ImageList_Destroy(himl);
            himl = nullptr;
        }
    }

    bool BeginImageListDrag(int hotX, int hotY) {
        if (!himl) {
            return false;
        }
        if (!ImageList_BeginDrag(himl, 0, hotX, hotY)) {
            return false;
        }
        Point pt = GetCursorPosition();
        // Desktop HWND so the drag image is not clipped to our canvas
        if (!ImageList_DragEnter(GetDesktopWindow(), pt.x, pt.y)) {
            ImageList_EndDrag();
            return false;
        }
        dragStarted = true;
        return true;
    }

    void EndImageListDrag() {
        if (!dragStarted) {
            return;
        }
        ImageList_DragLeave(GetDesktopWindow());
        ImageList_EndDrag();
        dragStarted = false;
    }

    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (riid == IID_IUnknown || riid == IID_IDropSource) {
            *ppv = this;
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return AtomicIntInc(&refCount); }
    ULONG STDMETHODCALLTYPE Release() override {
        LONG r = InterlockedDecrement(&refCount);
        if (r == 0) {
            delete this;
        }
        return r;
    }
    STDMETHODIMP QueryContinueDrag(BOOL fEscapePressed, DWORD grfKeyState) override {
        if (fEscapePressed || gTestCancelDrag) {
            return DRAGDROP_S_CANCEL;
        }
        if (!(grfKeyState & MK_LBUTTON)) {
            return DRAGDROP_S_DROP;
        }
        return S_OK;
    }
    STDMETHODIMP GiveFeedback(__unused DWORD dwEffect) override {
        if (dragStarted) {
            Point pt = GetCursorPosition();
            ImageList_DragMove(pt.x, pt.y);
            // S_OK: we supply the drag visual via ImageList (not the OLE default cursor)
            return S_OK;
        }
        return DRAGDROP_S_USEDEFAULTCURSORS;
    }
};

class SimpleEnumFormatEtc : public IEnumFORMATETC {
    AtomicInt refCount = 1;
    const FORMATETC* formats = nullptr;
    ULONG count = 0;
    ULONG index = 0;

  public:
    SimpleEnumFormatEtc(const FORMATETC* fmts, ULONG n) : formats(fmts), count(n) {}

    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (riid == IID_IUnknown || riid == IID_IEnumFORMATETC) {
            *ppv = this;
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return AtomicIntInc(&refCount); }
    ULONG STDMETHODCALLTYPE Release() override {
        LONG r = InterlockedDecrement(&refCount);
        if (r == 0) {
            delete this;
        }
        return r;
    }

    STDMETHODIMP Next(ULONG celt, FORMATETC* rgelt, ULONG* pceltFetched) override {
        if (!rgelt) {
            return E_POINTER;
        }
        ULONG fetched = 0;
        while (fetched < celt && index < count) {
            rgelt[fetched++] = formats[index++];
        }
        if (pceltFetched) {
            *pceltFetched = fetched;
        }
        return fetched == celt ? S_OK : S_FALSE;
    }
    STDMETHODIMP Skip(ULONG celt) override {
        if (index + celt < count) {
            index += celt;
            return S_OK;
        }
        index = count;
        return S_FALSE;
    }
    STDMETHODIMP Reset() override {
        index = 0;
        return S_OK;
    }
    STDMETHODIMP Clone(IEnumFORMATETC** ppEnum) override {
        if (!ppEnum) {
            return E_POINTER;
        }
        auto* e = new SimpleEnumFormatEtc(formats, count);
        e->index = index;
        *ppEnum = e;
        return S_OK;
    }
};

class TextDataObject : public IDataObject {
    AtomicInt refCount = 1;
    HGLOBAL hText = nullptr;

  public:
    explicit TextDataObject(WStr text) {
        if (len(text) == 0) {
            return;
        }
        size_t cb = (size_t)(text.len + 1) * sizeof(WCHAR);
        hText = GlobalAlloc(GMEM_MOVEABLE, cb);
        if (hText) {
            void* p = GlobalLock(hText);
            if (p) {
                memcpy(p, text.s, text.len * sizeof(WCHAR));
                ((WCHAR*)p)[text.len] = 0;
                GlobalUnlock(hText);
            } else {
                GlobalFree(hText);
                hText = nullptr;
            }
        }
    }
    ~TextDataObject() {
        if (hText) {
            GlobalFree(hText);
        }
    }

    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (riid == IID_IUnknown || riid == IID_IDataObject) {
            *ppv = this;
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return AtomicIntInc(&refCount); }
    ULONG STDMETHODCALLTYPE Release() override {
        LONG r = InterlockedDecrement(&refCount);
        if (r == 0) {
            delete this;
        }
        return r;
    }

    STDMETHODIMP GetData(FORMATETC* pFE, STGMEDIUM* pMedium) override {
        if (!hText) {
            return E_UNEXPECTED;
        }
        if (pFE->cfFormat != CF_UNICODETEXT || !(pFE->tymed & TYMED_HGLOBAL)) {
            return DV_E_FORMATETC;
        }
        size_t cb = GlobalSize(hText);
        HGLOBAL hCopy = GlobalAlloc(GMEM_MOVEABLE, cb);
        if (!hCopy) {
            return E_OUTOFMEMORY;
        }
        void* src = GlobalLock(hText);
        void* dst = GlobalLock(hCopy);
        if (!src || !dst) {
            if (src) {
                GlobalUnlock(hText);
            }
            if (dst) {
                GlobalUnlock(hCopy);
            }
            GlobalFree(hCopy);
            return E_OUTOFMEMORY;
        }
        memcpy(dst, src, cb);
        GlobalUnlock(hCopy);
        GlobalUnlock(hText);
        pMedium->tymed = TYMED_HGLOBAL;
        pMedium->hGlobal = hCopy;
        pMedium->pUnkForRelease = nullptr;
        return S_OK;
    }
    STDMETHODIMP GetDataHere(__unused FORMATETC* pFE, __unused STGMEDIUM* pMed) override { return E_NOTIMPL; }
    STDMETHODIMP QueryGetData(FORMATETC* pFE) override {
        if (pFE->cfFormat == CF_UNICODETEXT && (pFE->tymed & TYMED_HGLOBAL)) {
            return S_OK;
        }
        return DV_E_FORMATETC;
    }
    STDMETHODIMP GetCanonicalFormatEtc(__unused FORMATETC* pIn, FORMATETC* pOut) override {
        pOut->ptd = nullptr;
        return E_NOTIMPL;
    }
    STDMETHODIMP SetData(__unused FORMATETC* pFE, __unused STGMEDIUM* pMed, __unused BOOL fRelease) override {
        return E_NOTIMPL;
    }
    STDMETHODIMP EnumFormatEtc(__unused DWORD dwDirection, __unused IEnumFORMATETC** ppEnum) override {
        return E_NOTIMPL;
    }
    STDMETHODIMP DAdvise(__unused FORMATETC* pFE, __unused DWORD advf, __unused IAdviseSink* pAdvSink,
                         __unused DWORD* pdwConn) override {
        return E_NOTIMPL;
    }
    STDMETHODIMP DUnadvise(__unused DWORD dwConn) override { return E_NOTIMPL; }
    STDMETHODIMP EnumDAdvise(__unused IEnumSTATDATA** ppEnumAdvise) override { return E_NOTIMPL; }
};

// data object of the most recent drag-out, kept connected because cross-process
// drop targets can still extract data after DoDragDrop returns (e.g. Explorer
// fetches CFSTR_FILECONTENTS after IDropTarget::Drop returns)
static IDataObject* gLastDragDataObj = nullptr;

// DoDragDrop marshals the data object (CoMarshalInterface) for cross-process
// targets and never releases the stub's references, not even in
// OleUninitialize, which would leak the object. Disconnecting right after
// DoDragDrop returns breaks targets that extract data after Drop() returns,
// so we keep the object connected until the next drag-out or app exit.
void DisconnectLastDragDataObject() {
    if (!gLastDragDataObj) {
        return;
    }
    CoDisconnectObject(gLastDragDataObj, 0);
    gLastDragDataObj->Release();
    gLastDragDataObj = nullptr;
}

static void FinishDragDrop(IDataObject* dataObj) {
    DisconnectLastDragDataObject();
    gLastDragDataObj = dataObj; // transfers our reference
}

// ng: orig calls DoDragDrop from the canvas' WM_MOUSEMOVE. Here the mouse move
// is in the middle of gpui's dispatch, and DoDragDrop runs a message loop of
// its own, so the drag starts from the ui task queue once gpui has unwound.
static bool CanStartDrag(MainWindow* win) {
    if (gTestCancelDrag) {
        // OLE asks the drop source when there is mouse or keyboard input:
        // give it some, so the source gets to cancel
        HWND hwnd = AppShellNativeHwnd(win);
        POINT p{};
        GetCursorPos(&p);
        ScreenToClient(hwnd, &p);
        PostMessageW(hwnd, WM_MOUSEMOVE, 0, MAKELPARAM(p.x, p.y));
        return true;
    }
    // the button came up between the mouse move and now: nothing to drag, and
    // OLE would sit in its loop until the next input to notice
    int vk = GetSystemMetrics(SM_SWAPBUTTON) ? VK_RBUTTON : VK_LBUTTON;
    return (GetAsyncKeyState(vk) & 0x8000) != 0;
}

static void TextDragDropNow(MainWindow* win) {
    if (!IsMainWindowValidAndNotClosing(win)) {
        return;
    }
    if (!CanStartDrag(win)) {
        AppShellAfterNativeDrag(win);
        return;
    }
    WindowTab* tab = win->CurrentTab();
    bool isTextOnly = false;
    TempStr text = GetSelectedTextTemp(tab, StrL("\r\n"), isTextOnly);
    if (len(text) == 0) {
        AppShellAfterNativeDrag(win);
        return;
    }
    TempWStr wtext = ToWStrTemp(text);
    TextDataObject* dataObj = new TextDataObject(wtext);
    TextDropSource* dropSrc = new TextDropSource();
    DWORD dwEffect = 0;
    gLastDragTextLen = len(wtext);
    gLastDragResult = DoDragDrop(dataObj, dropSrc, DROPEFFECT_COPY, &dwEffect);
    logf("StartTextDragDrop: %d chars, hr 0x%x, effect %d\n", gLastDragTextLen, (int)gLastDragResult, (int)dwEffect);
    dropSrc->Release();
    FinishDragDrop(dataObj);
    AppShellAfterNativeDrag(win);
}

void StartTextDragDrop(MainWindow* win) {
    uitask::Post(MkFunc0(TextDragDropNow, win), "StartTextDragDrop");
}

// encode HBITMAP to PNG in memory using GDI+ IStream
static HGLOBAL EncodeBitmapToPngGlobal(HBITMAP hbmp) {
    Gdiplus::Bitmap gdipBmp(hbmp, nullptr);
    if (gdipBmp.GetLastStatus() != Gdiplus::Ok) {
        return nullptr;
    }
    CLSID pngClsid = GetGdiPlusEncoderClsid(L"image/png");
    IStream* stream = nullptr;
    HRESULT hr = CreateStreamOnHGlobal(nullptr, FALSE, &stream);
    if (FAILED(hr) || !stream) {
        return nullptr;
    }
    Gdiplus::Status status = gdipBmp.Save(stream, &pngClsid, nullptr);
    HGLOBAL hMem = nullptr;
    if (status == Gdiplus::Ok) {
        GetHGlobalFromStream(stream, &hMem);
    }
    stream->Release();
    if (status != Gdiplus::Ok) {
        return nullptr;
    }
    return hMem;
}

// IDataObject that provides an image as a virtual file (CFSTR_FILEDESCRIPTOR + CFSTR_FILECONTENTS)
// without creating any temporary files on disk.
class ImageDataObject : public IDataObject {
    AtomicInt refCount = 1;
    HGLOBAL hPngData = nullptr; // PNG-encoded image data
    size_t pngSize = 0;
    UINT cfFileDescriptor = 0;
    UINT cfFileContents = 0;
    UINT cfPreferredDropEffect = 0;
    FORMATETC fmts[3]{};
    ULONG fmtCount = 0;

    bool QueryFormatSupported(FORMATETC* pFE) const {
        for (ULONG i = 0; i < fmtCount; i++) {
            const FORMATETC& fmt = fmts[i];
            if (pFE->cfFormat != fmt.cfFormat) {
                continue;
            }
            if (fmt.lindex >= 0 && pFE->lindex != fmt.lindex) {
                continue;
            }
            if (pFE->tymed & fmt.tymed) {
                return true;
            }
        }
        return false;
    }

  public:
    explicit ImageDataObject(HGLOBAL hPng) {
        hPngData = hPng;
        pngSize = GlobalSize(hPng);
        cfFileDescriptor = RegisterClipboardFormatW(CFSTR_FILEDESCRIPTORW);
        cfFileContents = RegisterClipboardFormatW(CFSTR_FILECONTENTS);
        cfPreferredDropEffect = RegisterClipboardFormatW(L"Preferred DropEffect");

        fmts[0] = {(CLIPFORMAT)cfFileDescriptor, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
        fmts[1] = {(CLIPFORMAT)cfFileContents, nullptr, DVASPECT_CONTENT, 0, TYMED_ISTREAM | TYMED_HGLOBAL};
        fmts[2] = {(CLIPFORMAT)cfPreferredDropEffect, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
        fmtCount = 3;
    }
    ~ImageDataObject() {
        if (hPngData) {
            GlobalFree(hPngData);
        }
    }

    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (riid == IID_IUnknown || riid == IID_IDataObject) {
            *ppv = this;
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return AtomicIntInc(&refCount); }
    ULONG STDMETHODCALLTYPE Release() override {
        LONG r = InterlockedDecrement(&refCount);
        if (r == 0) {
            delete this;
        }
        return r;
    }

    STDMETHODIMP GetData(FORMATETC* pFE, STGMEDIUM* pMedium) override {
        if (!hPngData) {
            return E_UNEXPECTED;
        }

        // CFSTR_FILEDESCRIPTORW: describe one virtual file "image.png"
        if (pFE->cfFormat == cfFileDescriptor && (pFE->tymed & TYMED_HGLOBAL)) {
            size_t cb = offsetof(FILEGROUPDESCRIPTORW, fgd) + sizeof(FILEDESCRIPTORW);
            HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, cb);
            if (!h) {
                return E_OUTOFMEMORY;
            }
            auto* fgd = (FILEGROUPDESCRIPTORW*)GlobalLock(h);
            if (!fgd) {
                GlobalFree(h);
                return E_OUTOFMEMORY;
            }
            fgd->cItems = 1;
            fgd->fgd[0].dwFlags = FD_FILESIZE | FD_ATTRIBUTES;
            fgd->fgd[0].dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
            fgd->fgd[0].nFileSizeLow = (DWORD)pngSize;
            fgd->fgd[0].nFileSizeHigh = 0;
            wstr::BufSet(WStr(fgd->fgd[0].cFileName, MAX_PATH), WStrL(L"image.png"));
            GlobalUnlock(h);
            pMedium->tymed = TYMED_HGLOBAL;
            pMedium->hGlobal = h;
            pMedium->pUnkForRelease = nullptr;
            return S_OK;
        }

        // CFSTR_FILECONTENTS: provide the PNG data as an IStream or HGLOBAL
        if (pFE->cfFormat == cfFileContents && pFE->lindex == 0) {
            if (pFE->tymed & TYMED_HGLOBAL) {
                HGLOBAL hCopy = GlobalAlloc(GMEM_MOVEABLE, pngSize);
                if (!hCopy) {
                    return E_OUTOFMEMORY;
                }
                void* src = GlobalLock(hPngData);
                void* dst = GlobalLock(hCopy);
                if (!src || !dst) {
                    if (src) {
                        GlobalUnlock(hPngData);
                    }
                    if (dst) {
                        GlobalUnlock(hCopy);
                    }
                    GlobalFree(hCopy);
                    return E_OUTOFMEMORY;
                }
                memcpy(dst, src, pngSize);
                GlobalUnlock(hCopy);
                GlobalUnlock(hPngData);
                pMedium->tymed = TYMED_HGLOBAL;
                pMedium->hGlobal = hCopy;
                pMedium->pUnkForRelease = nullptr;
                return S_OK;
            }
            if (pFE->tymed & TYMED_ISTREAM) {
                IStream* stream = nullptr;
                HRESULT hr = CreateStreamOnHGlobal(nullptr, TRUE, &stream);
                if (FAILED(hr) || !stream) {
                    return E_OUTOFMEMORY;
                }
                void* src = GlobalLock(hPngData);
                if (!src) {
                    stream->Release();
                    return E_OUTOFMEMORY;
                }
                ULONG written = 0;
                stream->Write(src, (ULONG)pngSize, &written);
                GlobalUnlock(hPngData);
                LARGE_INTEGER zero{};
                stream->Seek(zero, STREAM_SEEK_SET, nullptr);
                pMedium->tymed = TYMED_ISTREAM;
                pMedium->pstm = stream;
                pMedium->pUnkForRelease = nullptr;
                return S_OK;
            }
        }

        if (pFE->cfFormat == cfPreferredDropEffect && (pFE->tymed & TYMED_HGLOBAL)) {
            HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, sizeof(DWORD));
            if (!h) {
                return E_OUTOFMEMORY;
            }
            auto* effect = (DWORD*)GlobalLock(h);
            if (!effect) {
                GlobalFree(h);
                return E_OUTOFMEMORY;
            }
            *effect = DROPEFFECT_COPY;
            GlobalUnlock(h);
            pMedium->tymed = TYMED_HGLOBAL;
            pMedium->hGlobal = h;
            pMedium->pUnkForRelease = nullptr;
            return S_OK;
        }

        return DV_E_FORMATETC;
    }
    STDMETHODIMP GetDataHere(__unused FORMATETC* pFE, __unused STGMEDIUM* pMed) override { return E_NOTIMPL; }
    STDMETHODIMP QueryGetData(FORMATETC* pFE) override { return QueryFormatSupported(pFE) ? S_OK : DV_E_FORMATETC; }
    STDMETHODIMP GetCanonicalFormatEtc(__unused FORMATETC* pIn, FORMATETC* pOut) override {
        pOut->ptd = nullptr;
        return E_NOTIMPL;
    }
    STDMETHODIMP SetData(__unused FORMATETC* pFE, __unused STGMEDIUM* pMed, __unused BOOL fRelease) override {
        return E_NOTIMPL;
    }
    STDMETHODIMP EnumFormatEtc(DWORD dwDirection, IEnumFORMATETC** ppEnum) override {
        if (!ppEnum) {
            return E_POINTER;
        }
        if (dwDirection != DATADIR_GET) {
            return E_NOTIMPL;
        }
        *ppEnum = new SimpleEnumFormatEtc(fmts, fmtCount);
        return S_OK;
    }
    STDMETHODIMP DAdvise(__unused FORMATETC* pFE, __unused DWORD advf, __unused IAdviseSink* pAdvSink,
                         __unused DWORD* pdwConn) override {
        return E_NOTIMPL;
    }
    STDMETHODIMP DUnadvise(__unused DWORD dwConn) override { return E_NOTIMPL; }
    STDMETHODIMP EnumDAdvise(__unused IEnumSTATDATA** ppEnumAdvise) override { return E_NOTIMPL; }
};

// Longest edge of the proportional drag-out thumbnail (logical px; DPI-scaled).
constexpr int kDragImageThumbnailSize = 220;

// Proportional drag thumbnail (longest edge capped), Chrome-like.
// GDI+ scales the source (StretchBlt on some DIB/mapped bitmaps leaves pure white).
// Top-down 32bpp DIB with a 1px border so light pages stay visible. Caller owns HBITMAP.
static HBITMAP CreateProportionalDragThumbnail(HBITMAP src, int maxEdge) {
    if (!src || maxEdge < 16) {
        return nullptr;
    }
    BITMAP bm{};
    if (!GetObject(src, sizeof(bm), &bm) || bm.bmWidth <= 0 || bm.bmHeight <= 0) {
        return nullptr;
    }
    int sw = bm.bmWidth;
    int sh = bm.bmHeight;
    int maxDim = sw > sh ? sw : sh;
    int dw = sw;
    int dh = sh;
    if (maxDim > maxEdge) {
        dw = (int)((i64)sw * maxEdge / maxDim);
        dh = (int)((i64)sh * maxEdge / maxDim);
        dw = std::max(dw, 1);
        dh = std::max(dh, 1);
    }

    Gdiplus::Bitmap srcGdip(src, nullptr);
    if (srcGdip.GetLastStatus() != Gdiplus::Ok) {
        return nullptr;
    }
    Gdiplus::Bitmap scaled(dw, dh, PixelFormat32bppARGB);
    if (scaled.GetLastStatus() != Gdiplus::Ok) {
        return nullptr;
    }
    {
        Gdiplus::Graphics g(&scaled);
        g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
        g.Clear(Gdiplus::Color(255, 255, 255, 255));
        g.DrawImage(&srcGdip, 0, 0, dw, dh);
        Gdiplus::Pen border(Gdiplus::Color(255, 60, 60, 60), 1.0f);
        g.DrawRectangle(&border, 0, 0, dw - 1, dh - 1);
    }

    Gdiplus::BitmapData bd{};
    Gdiplus::Rect lockRc(0, 0, dw, dh);
    if (scaled.LockBits(&lockRc, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &bd) != Gdiplus::Ok) {
        return nullptr;
    }

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = dw;
    bmi.bmiHeader.biHeight = -dh; // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HDC screenDc = GetDC(nullptr);
    HBITMAP dib = screenDc ? CreateDIBSection(screenDc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0) : nullptr;
    if (screenDc) {
        ReleaseDC(nullptr, screenDc);
    }
    if (!dib || !bits) {
        scaled.UnlockBits(&bd);
        if (dib) {
            DeleteObject(dib);
        }
        return nullptr;
    }

    auto* dst = (BYTE*)bits;
    const auto* srcRow = (const BYTE*)bd.Scan0;
    for (int y = 0; y < dh; y++) {
        const auto* s = srcRow + ((size_t)y * bd.Stride);
        auto* d = dst + ((size_t)y * dw * 4);
        for (int x = 0; x < dw; x++) {
            // GDI+ 32bppARGB is B,G,R,A in memory on Windows; full opacity
            d[0] = s[0];
            d[1] = s[1];
            d[2] = s[2];
            d[3] = 0xFF;
            s += 4;
            d += 4;
        }
    }
    scaled.UnlockBits(&bd);
    return dib;
}

// Build an imagelist from the thumbnail for ImageList_BeginDrag.
// Takes ownership of hbmp (always destroyed before return).
static HIMAGELIST CreateDragImageList(HBITMAP hbmp) {
    if (!hbmp) {
        return nullptr;
    }
    BITMAP bm{};
    if (!GetObject(hbmp, sizeof(bm), &bm) || bm.bmWidth <= 0 || bm.bmHeight <= 0) {
        DeleteObject(hbmp);
        return nullptr;
    }
    HIMAGELIST himl = ImageList_Create(bm.bmWidth, bm.bmHeight, ILC_COLOR32, 1, 1);
    if (!himl) {
        DeleteObject(hbmp);
        return nullptr;
    }
    int idx = ImageList_Add(himl, hbmp, nullptr);
    DeleteObject(hbmp);
    if (idx < 0) {
        ImageList_Destroy(himl);
        return nullptr;
    }
    return himl;
}

static void ImageDragDropNow(MainWindow* win) {
    if (!IsMainWindowValidAndNotClosing(win)) {
        return;
    }
    DisplayModel* dm = win->AsFixed();
    if (!dm) {
        return;
    }
    IPageElement* el = win->imageDragElement;
    if (!el || !CanStartDrag(win)) {
        return;
    }
    RenderedBitmap* rb = dm->GetEngine()->GetImageForPageElement(el);
    if (!rb) {
        return;
    }
    HBITMAP srcBmp = rb->GetBitmap();
    HGLOBAL hPng = EncodeBitmapToPngGlobal(srcBmp);
    if (!hPng) {
        delete rb;
        return;
    }

    ImageDataObject* dataObj = new ImageDataObject(hPng);

    int maxEdge = DpiScale(kDragImageThumbnailSize);
    POINT hot{0, 0};
    HIMAGELIST himl = nullptr;
    HBITMAP thumb = CreateProportionalDragThumbnail(srcBmp, maxEdge);
    if (thumb) {
        BITMAP tbm{};
        GetObject(thumb, sizeof(tbm), &tbm);
        hot.x = tbm.bmWidth / 2;
        hot.y = tbm.bmHeight / 2;
        if (win->imageDragPageNo > 0 && tbm.bmWidth > 0 && tbm.bmHeight > 0) {
            Rect screenRc = dm->CvtToScreen(win->imageDragPageNo, el->GetRect());
            if (screenRc.dx > 0 && screenRc.dy > 0) {
                int relX = win->dragStart.x - screenRc.x;
                int relY = win->dragStart.y - screenRc.y;
                relX = limitValue(relX, 0, screenRc.dx);
                relY = limitValue(relY, 0, screenRc.dy);
                hot.x = (int)((i64)relX * tbm.bmWidth / screenRc.dx);
                hot.y = (int)((i64)relY * tbm.bmHeight / screenRc.dy);
            }
        }
        himl = CreateDragImageList(thumb); // takes ownership of thumb
    }
    delete rb;

    ImageDropSource* dropSrc = himl ? new ImageDropSource(himl) : nullptr;
    TextDropSource* plainSrc = dropSrc ? nullptr : new TextDropSource();
    IDropSource* src = dropSrc ? (IDropSource*)dropSrc : (IDropSource*)plainSrc;

    if (dropSrc) {
        dropSrc->BeginImageListDrag(hot.x, hot.y);
    }

    DWORD dwEffect = 0;
    gLastDragTextLen = 0;
    gLastDragResult = DoDragDrop(dataObj, src, DROPEFFECT_COPY, &dwEffect);
    logf("StartImageDragDrop: hr 0x%x, effect %d\n", (int)gLastDragResult, (int)dwEffect);

    if (dropSrc) {
        dropSrc->EndImageListDrag();
        dropSrc->Release();
    } else {
        plainSrc->Release();
    }
    FinishDragDrop(dataObj);
}

// ng: posted for the same reason as the text drag; the element is the
// window's until the drag is over
static void ImageDragDropPosted(MainWindow* win) {
    ImageDragDropNow(win);
    if (!IsMainWindowValid(win)) {
        return;
    }
    win->imageDragElement = nullptr;
    win->imageDragPageNo = -1;
    AppShellAfterNativeDrag(win);
}

void StartImageDragDrop(MainWindow* win) {
    uitask::Post(MkFunc0(ImageDragDropPosted, win), "StartImageDragDrop");
}

// returns true if url looks like it could be an image URL
static bool IsImageUrl(Str url) {
    // strip query string / fragment for extension check
    int qIdx = str::IndexOfChar(url, '?');
    int hIdx = str::IndexOfChar(url, '#');
    int n = url.len;
    if (qIdx >= 0 && qIdx < n) {
        n = qIdx;
    }
    if (hIdx >= 0 && hIdx < n) {
        n = hIdx;
    }
    // check for common image extensions
    Str exts[] = {StrL(".png"),  StrL(".jpg"),  StrL(".jpeg"), StrL(".gif"), StrL(".bmp"), StrL(".tiff"), StrL(".tif"),
                  StrL(".webp"), StrL(".avif"), StrL(".heic"), StrL(".jxr"), StrL(".jp2"), StrL(".tga"),  StrL(".ico")};
    for (Str ext : exts) {
        if (n >= ext.len) {
            Str ending(url.s + n - ext.len, ext.len);
            if (str::EqI(ending, ext)) {
                return true;
            }
        }
    }
    return false;
}

// Get the user's Downloads folder path
static TempStr GetDownloadsDirTemp() {
    WCHAR* pathW = nullptr;
    HRESULT hr = SHGetKnownFolderPath(FOLDERID_Downloads, 0, nullptr, &pathW);
    if (FAILED(hr) || !pathW) {
        CoTaskMemFree(pathW);
        return {};
    }
    TempStr res = ToUtf8Temp(pathW);
    CoTaskMemFree(pathW);
    return res;
}

static void AdvanceUrlPathUntilSuffix(Str& p, Str& lastSlash) {
    while (len(p) > 0 && p.s[0] != '?' && p.s[0] != '#') {
        if (p.s[0] == '/') {
            lastSlash = p;
        }
        p.s++;
        p.len--;
    }
}

// Extract a file name from a URL (last path component, without query/fragment)
static TempStr FileNameFromUrlTemp(Str url) {
    // skip past scheme
    Str path = url;
    Str slash = str::SliceFromChar(url, '/');
    if (slash) {
        path = slash;
        if (path.len >= 2 && path.s[0] == '/' && path.s[1] == '/') {
            path.s += 2;
            path.len -= 2;
        }
    }
    // find last '/' before any '?' or '#'
    Str lastSlash;
    Str p = path;
    AdvanceUrlPathUntilSuffix(p, lastSlash);
    if (len(lastSlash) == 0) {
        return {};
    }
    int nameLen = (int)(p.s - lastSlash.s - 1);
    if (nameLen <= 0) {
        return {};
    }
    return str::DupTemp(Str(lastSlash.s + 1, nameLen));
}

struct DownloadAndOpenUrlData {
    Str url;
    HWND hwndCanvas;
};

static void OpenDownloadedPath(Str* path) {
    MainWindow* win = AppShellWindowFromHwnd(GetForegroundWindow());
    if (!win && len(gWindows) > 0) {
        win = gWindows[0];
    }
    if (IsMainWindowValidAndNotClosing(win)) {
        LoadDocument(win, *path);
    }
    str::Free(*path);
    delete path;
}

static void DownloadAndOpenUrl(DownloadAndOpenUrlData* data) {
    Str url = data->url;

    TempStr downloadsDir = GetDownloadsDirTemp();
    if (len(downloadsDir) == 0) {
        logf("DownloadAndOpenUrl: failed to get Downloads folder\n");
        str::Free(data->url);
        delete data;
        return;
    }

    TempStr fileName = FileNameFromUrlTemp(url);
    if (len(fileName) == 0 || str::Eq(fileName, StrL(".")) || str::Eq(fileName, StrL("..")) ||
        str::Contains(fileName, StrL("/")) || str::Contains(fileName, StrL("\\")) ||
        str::Contains(fileName, StrL(":"))) {
        // generate a fallback name
        fileName = str::DupTemp(StrL("dropped_image.png"));
    }

    TempStr destPath = path::JoinTemp(downloadsDir, fileName);

    // avoid overwriting: if file exists, add a numeric suffix
    if (file::Exists(destPath)) {
        TempStr ext = path::GetExtTemp(destPath);
        TempStr base = str::DupTemp(Str(fileName.s, len(fileName) - len(ext)));
        for (int i = 1; i < 1000; i++) {
            TempStr newName = fmt("%s_%d%s", base, i, ext);
            destPath = path::JoinTemp(downloadsDir, newName);
            if (!file::Exists(destPath)) {
                break;
            }
        }
    }

    logf("DownloadAndOpenUrl: downloading '%s' to '%s'\n", url, destPath);

    Func1<HttpProgress*> emptyProgress;
    bool ok = HttpGetToFile(url, destPath, emptyProgress);
    if (!ok) {
        logf("DownloadAndOpenUrl: download failed for '%s'\n", url);
        str::Free(data->url);
        delete data;
        return;
    }

    // verify the downloaded file is a supported image type
    FileType kind = GuessFileTypeFromFile(destPath);
    if (!IsEngineImageSupportedFileType(kind)) {
        logf("DownloadAndOpenUrl: downloaded file is not a supported image type: '%s'\n", destPath);
        file::Delete(destPath);
        str::Free(data->url);
        delete data;
        return;
    }

    // ensure it has a good extension, some urls are like:
    // https://pbs.twimg.com/media/HEwit7bbQAAWiIO?format=jpg&name=large
    TempStr ext = GetExtForFileTypeTemp(kind);
    if (!str::EndsWithI(destPath, ext)) {
        TempStr newDest = str::JoinTemp(destPath, ext);
        ok = file::Rename(newDest, destPath);
        if (ok) {
            destPath = newDest;
        }
    }

    // open the file on the UI thread
    auto* pathDup = new Str(str::Dup(destPath));
    auto fn = MkFunc0<Str>(OpenDownloadedPath, pathDup);
    uitask::Post(fn, "DownloadAndOpenUrl");

    str::Free(data->url);
    delete data;
}

// Extract text from IDataObject (tries CF_UNICODETEXT, then CF_TEXT)
static TempStr GetTextFromDataObject(IDataObject* dataObj) {
    FORMATETC fmtUnicode = {CF_UNICODETEXT, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
    FORMATETC fmtAnsi = {CF_TEXT, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
    STGMEDIUM medium{};
    HRESULT hr = dataObj->GetData(&fmtUnicode, &medium);
    TempStr res;
    if (SUCCEEDED(hr) && medium.hGlobal) {
        WCHAR* w = (WCHAR*)GlobalLock(medium.hGlobal);
        res = w ? ToUtf8Temp(w) : TempStr();
        goto Cleanup;
    }
    hr = dataObj->GetData(&fmtAnsi, &medium);
    if (SUCCEEDED(hr) && medium.hGlobal) {
        char* s = (char*)GlobalLock(medium.hGlobal);
        res = s ? str::DupTemp(Str(s)) : TempStr();
        goto Cleanup;
    }
    return {};
Cleanup:
    GlobalUnlock(medium.hGlobal);
    ReleaseStgMedium(&medium);
    return res;
}

// Check if IDataObject contains a URL (registered format "UniformResourceLocatorW" or "UniformResourceLocator")
static TempStr GetUrlFromDataObject(IDataObject* dataObj) {
    // try wide URL format first
    static CLIPFORMAT cfUrlW = (CLIPFORMAT)RegisterClipboardFormatW(L"UniformResourceLocatorW");
    if (cfUrlW) {
        FORMATETC fmt = {cfUrlW, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
        STGMEDIUM medium{};
        HRESULT hr = dataObj->GetData(&fmt, &medium);
        if (SUCCEEDED(hr) && medium.hGlobal) {
            WCHAR* w = (WCHAR*)GlobalLock(medium.hGlobal);
            TempStr res = w ? ToUtf8Temp(w) : TempStr();
            GlobalUnlock(medium.hGlobal);
            ReleaseStgMedium(&medium);
            if (res && (str::StartsWithI(res, StrL("http://")) || str::StartsWithI(res, StrL("https://")))) {
                return res;
            }
        }
    }
    // try ANSI URL format
    static CLIPFORMAT cfUrl = (CLIPFORMAT)RegisterClipboardFormatW(L"UniformResourceLocator");
    if (cfUrl) {
        FORMATETC fmt = {cfUrl, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
        STGMEDIUM medium{};
        HRESULT hr = dataObj->GetData(&fmt, &medium);
        if (SUCCEEDED(hr) && medium.hGlobal) {
            char* s = (char*)GlobalLock(medium.hGlobal);
            TempStr res = s ? str::DupTemp(Str(s)) : TempStr();
            GlobalUnlock(medium.hGlobal);
            ReleaseStgMedium(&medium);
            if (res && (str::StartsWithI(res, StrL("http://")) || str::StartsWithI(res, StrL("https://")))) {
                return res;
            }
        }
    }
    return {};
}

static bool DataObjectHasFiles(IDataObject* dataObj) {
    FORMATETC fmt = {CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
    return dataObj->QueryGetData(&fmt) == S_OK;
}

static bool DataObjectHasUrl(IDataObject* dataObj) {
    TempStr url = GetUrlFromDataObject(dataObj);
    if (url && IsImageUrl(url)) {
        return true;
    }
    // also check plain text that looks like an image URL
    TempStr text = GetTextFromDataObject(dataObj);
    if (text && (str::StartsWithI(text, StrL("http://")) || str::StartsWithI(text, StrL("https://"))) &&
        IsImageUrl(text)) {
        return true;
    }
    return false;
}

// orig's PdfPathsFromDataObject (MergePdf.cpp)
static bool DataObjectHasPdf(IDataObject* dataObj) {
    FORMATETC fmt = {CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
    STGMEDIUM medium{};
    if (FAILED(dataObj->GetData(&fmt, &medium)) || !medium.hGlobal) {
        return false;
    }
    HDROP hDrop = (HDROP)medium.hGlobal;
    int nFiles = DragQueryFileW(hDrop, DRAGQUERY_NUMFILES, nullptr, 0);
    WCHAR pathW[MAX_PATH]{};
    bool hasPdf = false;
    for (int i = 0; i < nFiles; i++) {
        DragQueryFileW(hDrop, i, pathW, dimof(pathW));
        TempStr path = ToUtf8Temp(pathW);
        hasPdf = hasPdf || str::EndsWithI(path, StrL(".pdf"));
    }
    ReleaseStgMedium(&medium);
    return hasPdf;
}

// ng: orig registers one target on the canvas and another on the Merge PDF
// window (MergeDropTarget). Here the frame is the canvas' target: it sends
// the drag to the Merge PDF grid while that is a dialog in the frame and to
// the canvas otherwise, and refuses a drop anywhere else. The same class is
// the target of the Merge PDF window where that is a window of its own.
class CanvasDropTarget : public IDropTarget {
    AtomicInt refCount = 1;
    HWND hwnd = nullptr;
    bool hasFiles = false;
    bool hasUrl = false;
    bool hasPdf = false;

    DropHost Host() { return ToolWindowOwnsHwnd(hwnd) ? DropHost::ToolWindow : DropHost::Frame; }

    MainWindow* Win() {
        MainWindow* win = AppShellWindowFromHwnd(hwnd);
        if (!win) {
            win = ToolWindowOwnerFromHwnd(hwnd);
        }
        return IsMainWindowValidAndNotClosing(win) ? win : nullptr;
    }

    // screen pixels -> window dips
    PointF ToDips(POINTL ptScreen) {
        POINT p{ptScreen.x, ptScreen.y};
        ScreenToClient(hwnd, &p);
        float scale = (float)DpiGetForHwnd(hwnd) / 96.f;
        return PointF{(float)p.x / scale, (float)p.y / scale};
    }

    static bool OnCanvas(MainWindow* win, PointF pt) {
        Rect rc = win->canvasRc;
        return pt.x >= (float)rc.x && pt.x < (float)(rc.x + rc.dx) && pt.y >= (float)rc.y &&
               pt.y < (float)(rc.y + rc.dy);
    }

  public:
    explicit CanvasDropTarget(HWND h) : hwnd(h) {}

    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (riid == IID_IUnknown || riid == IID_IDropTarget) {
            *ppv = this;
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return AtomicIntInc(&refCount); }
    ULONG STDMETHODCALLTYPE Release() override {
        LONG r = InterlockedDecrement(&refCount);
        if (r == 0) {
            delete this;
        }
        return r;
    }

    STDMETHODIMP DragEnter(IDataObject* dataObj, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) override {
        hasFiles = DataObjectHasFiles(dataObj);
        hasUrl = !hasFiles && DataObjectHasUrl(dataObj);
        hasPdf = hasFiles && DataObjectHasPdf(dataObj);
        return DragOver(grfKeyState, pt, pdwEffect);
    }

    STDMETHODIMP DragOver(__unused DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) override {
        *pdwEffect = DROPEFFECT_NONE;
        MainWindow* win = Win();
        if (!win) {
            return S_OK;
        }
        PointF ptDips = ToDips(pt);
        // orig's MergeDropTarget::DragOver: the bar shows where the PDFs go
        bool accept = false;
        if (PdfToolDialogOnDragOver(win, &ptDips, hasPdf, &accept, Host())) {
            *pdwEffect = accept ? DROPEFFECT_COPY : DROPEFFECT_NONE;
            return S_OK;
        }
        if (Host() == DropHost::ToolWindow) {
            return S_OK;
        }
        if ((hasFiles || hasUrl) && OnCanvas(win, ptDips)) {
            *pdwEffect = DROPEFFECT_COPY;
        }
        return S_OK;
    }

    STDMETHODIMP DragLeave() override {
        if (MainWindow* win = Win()) {
            bool accept = false;
            PdfToolDialogOnDragOver(win, nullptr, false, &accept, Host());
        }
        return S_OK;
    }

    STDMETHODIMP Drop(IDataObject* dataObj, DWORD /*grfKeyState*/, POINTL pt, DWORD* pdwEffect) override {
        *pdwEffect = DROPEFFECT_NONE;
        MainWindow* win = Win();
        if (!win) {
            return S_OK;
        }
        PointF ptDips = ToDips(pt);
        bool accept = false;
        bool mergeUp = PdfToolDialogOnDragOver(win, nullptr, false, &accept, Host());

        // first try file drops (CF_HDROP)
        FORMATETC fmtHDrop = {CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
        STGMEDIUM medium{};
        HRESULT hr = dataObj->GetData(&fmtHDrop, &medium);
        if (SUCCEEDED(hr) && medium.hGlobal) {
            HDROP hDrop = (HDROP)medium.hGlobal;
            POINT ptClient{pt.x, pt.y};
            ScreenToClient(hwnd, &ptClient);
            if (AppShellOnDropFiles(hwnd, hDrop, ptClient, true, false)) {
                *pdwEffect = DROPEFFECT_COPY;
            }
            ReleaseStgMedium(&medium);
            return S_OK;
        }
        if (mergeUp || Host() == DropHost::ToolWindow || !OnCanvas(win, ptDips)) {
            return S_OK;
        }

        // try URL drop
        TempStr url = GetUrlFromDataObject(dataObj);
        if (len(url) == 0) {
            // fall back to plain text
            TempStr text = GetTextFromDataObject(dataObj);
            if (text && (str::StartsWithI(text, StrL("http://")) || str::StartsWithI(text, StrL("https://")))) {
                url = text;
            }
        }

        if (url) {
            *pdwEffect = DROPEFFECT_COPY;
            auto* data = new DownloadAndOpenUrlData();
            data->url = str::Dup(url);
            data->hwndCanvas = hwnd;
            auto fn = MkFunc0<DownloadAndOpenUrlData>(DownloadAndOpenUrl, data);
            RunAsync(fn, StrL("DownloadAndOpenUrl"));
        }

        return S_OK;
    }
};

struct RegisteredTarget {
    HWND hwnd = nullptr;
    CanvasDropTarget* target = nullptr;
};
static Vec<RegisteredTarget> gDropTargets;

void RegisterCanvasDropTarget(HWND hwndCanvas) {
    // RegisterDragDrop needs OLE, not just COM, on this thread
    static bool oleInited = SUCCEEDED(OleInitialize(nullptr));
    auto* dt = new CanvasDropTarget(hwndCanvas);
    HRESULT hr = RegisterDragDrop(hwndCanvas, dt);
    logf("RegisterCanvasDropTarget: ole %d, hr 0x%x\n", oleInited ? 1 : 0, (int)hr);
    if (SUCCEEDED(hr)) {
        VecAppend(gDropTargets, RegisteredTarget{hwndCanvas, dt});
    }
    dt->Release(); // RegisterDragDrop AddRef'd it
}

void RevokeCanvasDropTarget(HWND hwndCanvas) {
    for (int i = len(gDropTargets) - 1; i >= 0; i--) {
        if (gDropTargets[i].hwnd == hwndCanvas) {
            VecRemoveAt(gDropTargets, i);
            RevokeDragDrop(hwndCanvas);
        }
    }
}

// ng: the automation channel's way to exercise the code above without moving
// the real cursor or dropping on another program. `what`:
//   cancel <on|off>     drags are cancelled by their drop source at once
//   last                the result of the last drag
//   text <text>         DragEnter + DragLeave with CF_UNICODETEXT at (x, y)
//   over <path>         DragEnter + DragOver with the file, left hovering
//   leave               DragLeave
//   drop <path>         DragEnter + DragOver + Drop with the file
// (x, y) are window dips.
TempStr OleDragDropTestTemp(MainWindow* win, Str what, Str arg, int x, int y, Str toolWindow) {
    if (str::Eq(what, StrL("cancel"))) {
        gTestCancelDrag = str::Eq(arg, StrL("on"));
        return fmt("OK cancel=%d", gTestCancelDrag ? 1 : 0);
    }
    if (str::Eq(what, StrL("last"))) {
        Str res = StrL("other");
        if (gLastDragResult == DRAGDROP_S_CANCEL) {
            res = StrL("cancel");
        } else if (gLastDragResult == DRAGDROP_S_DROP) {
            res = StrL("drop");
        } else if (gLastDragResult == E_FAIL) {
            res = StrL("none");
        }
        return fmt("OK drag=%s chars=%d hr=0x%x", res, gLastDragTextLen, (int)gLastDragResult);
    }
    HWND hwnd = AppShellNativeHwnd(win);
    if (len(toolWindow) > 0) {
        hwnd = ToolWindowHwnd(ToolWindowFind(toolWindow));
    }
    CanvasDropTarget* target = nullptr;
    for (const RegisteredTarget& rt : gDropTargets) {
        if (rt.hwnd == hwnd) {
            target = rt.target;
        }
    }
    if (!target) {
        return StrL("ERR no-drop-target");
    }
    float scale = (float)DpiGetForHwnd(hwnd) / 96.f;
    POINT p{(LONG)((float)x * scale), (LONG)((float)y * scale)};
    ClientToScreen(hwnd, &p);
    POINTL pt{p.x, p.y};
    DWORD effect = DROPEFFECT_COPY;
    if (str::Eq(what, StrL("leave"))) {
        target->DragLeave();
        return StrL("OK");
    }
    IDataObject* dataObj = nullptr;
    if (str::Eq(what, StrL("text"))) {
        dataObj = new TextDataObject(ToWStrTemp(arg));
    } else {
        dataObj = GetDataObjectForFile(arg, hwnd);
    }
    if (!dataObj) {
        return StrL("ERR no-data-object");
    }
    target->DragEnter(dataObj, MK_LBUTTON, pt, &effect);
    DWORD enterEffect = effect;
    if (str::Eq(what, StrL("text"))) {
        target->DragLeave();
    } else if (str::Eq(what, StrL("drop"))) {
        effect = DROPEFFECT_COPY;
        target->Drop(dataObj, 0, pt, &effect);
    }
    dataObj->Release();
    return fmt("OK enter=%d effect=%d", (int)enterEffect, (int)effect);
}
