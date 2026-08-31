/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/BitManip.h"
#include "base/WinDynCalls.h"
#include "gui/Dpi.h"
#include "base/File.h"
#include "base/Timer.h"
#include "base/UITask.h"
#include "base/Win.h"
#include "base/ScopedWin.h"
#include "base/Http.h"
#include "base/Pixmap.h"
#include "base/GdiPlusUtil.h"
#include "base/GuessFileType.h"

#include <mmsystem.h> // timeBeginPeriod / timeEndPeriod for smooth-scroll timer
#include <shlobj.h>   // IDragSourceHelper for image drag thumbnails
#pragma comment(lib, "winmm.lib")

#include "gui/UIModels.h"
#include "gui/Gfx.h"
#include "gui/Layout.h"
#include "gui/PlatformFont.h"
#include "gui/win/WinGui.h"

#include "Settings.h"
#include "DisplayMode.h"
#include "Annotation.h"
#include "FormFields.h"
#include "SumatraDialogs.h"
#include "DocController.h"
#include "EngineBase.h"
#include "EngineAll.h"

#include "DisplayModel.h"
#include "Theme.h"
#include "AppSettings.h"
#include "RenderCache.h"
#include "ProgressUpdateUI.h"
#include "TextSelection.h"
#include "TextSearch.h"
#include "SumatraConfig.h"
#include "WindowTab.h"
#include "SumatraPDF.h"
#include "AnnotFilterToolbar.h"
#include "Notifications.h"
#include "MainWindow.h"
#include "AnnotPlacement.h"
#include "Menu.h"
#include "uia/Provider.h"
#include "SearchAndDDE.h"
#include "Selection.h"
#include "LinkFollow.h"
#include "SelectTextKeyboard.h"
#include "SelectionToolbar.h"
#include "AnnotEditToolbar.h"
#include "AnnotTextPopup.h"
#include "ReadAloudHighlight.h"
#include "ReadAloudPlaybackBar.h"
#include "TextToSpeech.h"
#include "HomePage.h"
#include "Commands.h"
#include "Toolbar.h"
#include "Translations.h"

#include "RefHover.h"
#include "Canvas.h"
#include "SumatraLog.h"

// if set instead of trying to render pages we don't have, we simply do nothing
// this reduces the flickering when going quickly through pages but creates
// impression of lag
static bool gNoFlickerRender = true;

void CancelAnnotationResizeRerender(MainWindow* win) {
    if (!win) {
        return;
    }
    if (win->hwndCanvas) {
        KillTimer(win->hwndCanvas, kAnnotationResizeRerenderTimerID);
    }
    win->annotationResizeRerenderTimer = 0;
}

Kind kNotifAnnotation = "notifAnnotation";

constexpr int kRenderDelayShowNotif = 500;

//--- laser pointer (CmdToggleLaserPointer)

// A laser pointer is a session mode, not a setting: it's turned on to point
// things out during a presentation and off again afterwards, and an app that
// started up with the mouse cursor replaced by a red dot would look broken.
static bool gLaserPointer = false;

// logical size of the cursor bitmap. The dot itself is a small part of it,
// the rest is the glow fading out to fully transparent
constexpr int kLaserPointerCursorSize = 32;

static HCURSOR gCursorLaserPointer = nullptr;
static int gCursorLaserPointerSize = 0;

// A laser dot: a white-hot center inside a saturated red core, surrounded by a
// glow that fades to transparent so the dot is visible on light and dark pages
// alike. The hotspot is the center of the dot, unlike an arrow's tip.
static HCURSOR CreateLaserPointerCursor(int size) {
    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = size;
    bmi.bmiHeader.biHeight = -size; // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP hbmpColor = CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!hbmpColor || !bits) {
        DeleteObject(hbmpColor);
        return nullptr;
    }

    float center = (float)size / 2.f;
    float glowR = center;
    float coreR = (float)size * 0.16f;
    float hotR = (float)size * 0.07f;
    DWORD* pixels = (DWORD*)bits;
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            float dx = ((float)x + 0.5f) - center;
            float dy = ((float)y + 0.5f) - center;
            float d = sqrtf((dx * dx) + (dy * dy));

            // the glow, densest where it meets the core and quadratically
            // fading out; it also fills the core, so that anti-aliasing the
            // core's edge blends it into the glow rather than into a gap
            float t = limitValue((d - coreR) / (glowR - coreR), 0.f, 1.f);
            float glowA = (d < glowR) ? 0.45f * (1.f - t) * (1.f - t) : 0.f;
            // the dot itself, white-hot in the middle, saturated red at the edge
            float coreA = limitValue(coreR + 0.5f - d, 0.f, 1.f);
            float hot = limitValue(d / hotR, 0.f, 1.f);
            float coreG = 255.f - (200.f * hot);

            // core over glow, written out premultiplied (as 32bpp cursors want)
            float glowW = glowA * (1.f - coreA);
            float alpha = coreA + glowW;
            if (alpha <= 0.f) {
                pixels[(y * size) + x] = 0;
                continue;
            }
            u8 a = (u8)(alpha * 255.f);
            u8 r = (u8)((255.f * coreA) + (255.f * glowW));
            u8 g = (u8)((coreG * coreA) + (16.f * glowW));
            u8 b = g;
            pixels[(y * size) + x] = ((DWORD)a << 24) | ((DWORD)r << 16) | ((DWORD)g << 8) | b;
        }
    }

    // 32bpp cursors carry their own alpha, but CreateIconIndirect still wants a
    // mask bitmap; an all-zero AND mask leaves the color bitmap in charge
    int maskBytesPerRow = ((size + 15) / 16) * 2;
    u8* maskBits = AllocArray<u8>(maskBytesPerRow * size);
    HBITMAP hbmpMask = CreateBitmap(size, size, 1, 1, maskBits);
    HCURSOR res = nullptr;
    if (hbmpMask) {
        ICONINFO ii{};
        ii.fIcon = FALSE;
        ii.xHotspot = (DWORD)(size / 2);
        ii.yHotspot = (DWORD)(size / 2);
        ii.hbmMask = hbmpMask;
        ii.hbmColor = hbmpColor;
        res = (HCURSOR)CreateIconIndirect(&ii);
        DeleteObject(hbmpMask);
    }
    DeleteObject(hbmpColor);
    free(maskBits);
    return res;
}

// the cursor is sized for the DPI of the window it's shown in, so it's
// re-created when the canvas moves to a monitor with a different scaling
static HCURSOR GetLaserPointerCursor() {
    int size = DpiScale(kLaserPointerCursorSize);
    if (gCursorLaserPointer && (gCursorLaserPointerSize == size)) {
        return gCursorLaserPointer;
    }
    HCURSOR cur = CreateLaserPointerCursor(size);
    if (!cur) {
        // a cursor of the wrong size beats no cursor at all
        return gCursorLaserPointer;
    }
    if (gCursorLaserPointer) {
        DestroyCursor(gCursorLaserPointer);
    }
    gCursorLaserPointer = cur;
    gCursorLaserPointerSize = size;
    return cur;
}

void DeleteLaserPointerCursor() {
    if (gCursorLaserPointer) {
        DestroyCursor(gCursorLaserPointer);
        gCursorLaserPointer = nullptr;
        gCursorLaserPointerSize = 0;
    }
}

bool IsLaserPointerActive() {
    return gLaserPointer;
}

// while on, the canvas cursor is the laser dot no matter what is under it:
// links, text and annotations still work, they just don't change the cursor
static bool SetLaserPointerCursor(MainWindow* win) {
    if (!gLaserPointer) {
        return false;
    }
    if (PM_BLACK_SCREEN == win->presentation || PM_WHITE_SCREEN == win->presentation) {
        // the presenter blanked the screen on purpose, don't put a dot on it
        return false;
    }
    HCURSOR cur = GetLaserPointerCursor();
    if (!cur) {
        return false;
    }
    SetCursor(cur);
    return true;
}

// canvas code sets its cursor through this instead of SetCursorCached() so
// that the laser pointer can take over
static void SetCanvasCursor(MainWindow* win, LPWSTR cursorId) {
    if (SetLaserPointerCursor(win)) {
        return;
    }
    SetCursorCached(cursorId);
}

void ToggleLaserPointer(MainWindow* win) {
    gLaserPointer = !gLaserPointer;
    // change the cursor now rather than on the next mouse move
    SendMessageW(win->hwndCanvas, WM_SETCURSOR, 0, 0);
}

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
        if (fEscapePressed) {
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
        if (fEscapePressed) {
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
        if (!text) {
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

static bool IsPointInSelection(MainWindow* win, Point pt) {
    WindowTab* tab = win->CurrentTab();
    if (!tab || !tab->selectionOnPage) {
        return false;
    }
    DisplayModel* dm = win->AsFixed();
    if (!dm) {
        return false;
    }
    for (SelectionOnPage& sel : *tab->selectionOnPage) {
        Rect r = sel.GetRect(dm);
        if (r.Contains(pt)) {
            return true;
        }
    }
    return false;
}

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

static void StartTextDragDrop(MainWindow* win) {
    WindowTab* tab = win->CurrentTab();
    bool isTextOnly = false;
    TempStr text = GetSelectedTextTemp(tab, StrL("\r\n"), isTextOnly);
    if (len(text) == 0) {
        return;
    }
    TempWStr wtext = ToWStrTemp(text);
    TextDataObject* dataObj = new TextDataObject(wtext);
    TextDropSource* dropSrc = new TextDropSource();
    DWORD dwEffect = 0;
    DoDragDrop(dataObj, dropSrc, DROPEFFECT_COPY, &dwEffect);
    dropSrc->Release();
    FinishDragDrop(dataObj);
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

static void StartImageDragDrop(MainWindow* win) {
    DisplayModel* dm = win->AsFixed();
    if (!dm) {
        return;
    }
    IPageElement* el = win->imageDragElement;
    if (!el) {
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
    DoDragDrop(dataObj, src, DROPEFFECT_COPY, &dwEffect);

    if (dropSrc) {
        dropSrc->EndImageListDrag();
        dropSrc->Release();
    } else {
        plainSrc->Release();
    }
    FinishDragDrop(dataObj);
}

// Resize handle positions that used in resizing annotations
enum class ResizeHandle {
    None = 0,
    TopLeft,
    Top,
    TopRight,
    Right,
    BottomRight,
    Bottom,
    BottomLeft,
    Left,
    LineStart,
    LineEnd,
    Vertex,
};

// Size of resize handle hit area (in pixels)
constexpr int kResizeHandleSize = 8;

// Smooth wheel scrolling: frame-rate–independent exponential chase of the
// target offset (common browser-style "lerp toward destination").
//
// Why not duration + ease-out restarted each tick? Restarting ease-out on every
// WM_MOUSEWHEEL re-peaks velocity each notch → visible stutter/pumping while
// spinning the wheel. Updating only the target keeps velocity continuous.
//
// Rate k (1/s): after ~200 ms we close ~95% of remaining (1-e^(-k*0.2)≈0.95).
static const double kSmoothScrollRate = 15.0;
// Snap when this close (pixels) so we do not crawl forever.
static const double kSmoothScrollSnapPx = 0.5;

// these can be global, as the mouse wheel can't affect more than one window at once
static int gDeltaPerLine = 0;
// set when WM_MOUSEWHEEL has been passed on (to prevent recursion)
static bool gWheelMsgRedirect = false;
static bool gInMouseWheelScroll = false;

static int ScrollLineAmount(int configuredAmount) {
    return configuredAmount > 0 ? configuredAmount : 16;
}

#ifdef DEBUG
bool Canvas_UnitTestScrollLineAmount() {
    return ScrollLineAmount(16) == 16 && ScrollLineAmount(30) == 30 && ScrollLineAmount(1) == 1 &&
           ScrollLineAmount(0) == 16 && ScrollLineAmount(-1) == 16;
}
#endif

static void StopSmoothScroll(MainWindow* win) {
    if (!win) {
        return;
    }
    KillTimer(win->hwndCanvas, kSmoothScrollTimerID);
    win->scrollAnimActive = false;
    if (win->scrollAnimHiResTimer) {
        timeEndPeriod(1);
        win->scrollAnimHiResTimer = false;
    }
}

// Set/update destination for smooth vertical scroll. Does not restart motion
// from scratch — mid-flight target changes keep continuous velocity.
static void StartOrUpdateSmoothScrollY(MainWindow* win, int targetY) {
    DisplayModel* dm = win->AsFixed();
    if (!dm) {
        return;
    }
    int current = dm->yOffset();
    if (current == targetY && !win->scrollAnimActive) {
        return;
    }
    if (current == targetY && win->scrollAnimActive && fabs(win->scrollAnimY - (double)targetY) < kSmoothScrollSnapPx) {
        StopSmoothScroll(win);
        return;
    }

    win->scrollTargetY = targetY;
    if (!win->scrollAnimActive) {
        win->scrollAnimY = (double)current;
        win->scrollAnimLastTime = TimeGet();
        win->scrollAnimActive = true;
        // 1 ms timer resolution while animating so WM_TIMER is less jumpy
        // (default ~15.6 ms is a common source of stutter).
        if (!win->scrollAnimHiResTimer) {
            timeBeginPeriod(1);
            win->scrollAnimHiResTimer = true;
        }
        SetTimer(win->hwndCanvas, kSmoothScrollTimerID, 1, nullptr);
    }
    // If already active: only target changes; scrollAnimY keeps going.
}

void UpdateDeltaPerLine() {
    ULONG ulScrollLines;
    BOOL ok = SystemParametersInfo(SPI_GETWHEELSCROLLLINES, 0, &ulScrollLines, 0);
    if (!ok) {
        return;
    }
    // ulScrollLines usually equals 3 or 0 (for no scrolling) or -1 (for page scrolling)
    // WHEEL_DELTA equals 120, so gDeltaPerLine will be 40
    gDeltaPerLine = 0;
    if (ulScrollLines == (ULONG)-1) {
        gDeltaPerLine = -1;
    } else if (ulScrollLines != 0) {
        gDeltaPerLine = WHEEL_DELTA / ulScrollLines;
    }
    // logf("SPI_GETWHEELSCROLLLINES: ulScrollLines=%d, gDeltaPerLine=%d\n", (int)ulScrollLines, gDeltaPerLine);
}

///// methods needed for FixedPageUI canvases with document loaded /////

__unused static Str scrollMsgStr(USHORT msg) {
    switch (msg) {
        case SB_LINEDOWN:
            return StrL("SB_LINEDOWN");
        case SB_LINEUP:
            return StrL("SB_LINEUP");
        case kSbHalfPageDown:
            return StrL("kSbHalfPageDown");
        case kSbHalfPageUp:
            return StrL("kSbHalfPageUp");
        case SB_PAGEDOWN:
            return StrL("SB_PAGEDOWN");
        case SB_PAGEUP:
            return StrL("SB_PAGEUP");
    }
    return fmt("%d", (int)msg);
}

static void OnVScroll(MainWindow* win, WPARAM wp) {
    ReportIf(!win->AsFixed());

    // Use overlay state whenever overlay mode is on — including SmartInvisible
    // (auto-hidden). Requiring IsOverlayScrollbarVisible() left keyboard scroll
    // updating nPos with redraw=false, so the bar never reappeared (issue #5850).
    bool overlayMode = ScrollbarsUseOverlay();
    bool useOverlay = overlayMode && win->overlayScrollV;
    SCROLLINFO si{};
    si.cbSize = sizeof(si);
    si.fMask = SIF_ALL;
    if (useOverlay) {
        OverlayScrollbarGetInfo(win->overlayScrollV, &si);
    } else {
        GetScrollInfo(win->hwndCanvas, SB_VERT, &si);
    }

    USHORT msg = LOWORD(wp);
    // for next-file-in-folder tip: scroll intent after handling the action
    bool scrollDown = (msg == SB_LINEDOWN || msg == SB_PAGEDOWN || msg == kSbHalfPageDown || msg == SB_BOTTOM);
    bool scrollUp = (msg == SB_LINEUP || msg == SB_PAGEUP || msg == kSbHalfPageUp || msg == SB_TOP);
    auto* ctrl = win->ctrl;
    bool dmIsSinglePage = (ctrl->GetDisplayMode() == DisplayMode::SinglePage);
    // scrollbarInSinglePage is false by default
    // if true, we show scrollbar in single page mode and make its position correspond to page number, so user can
    // scroll through pages using scrollbar even in single page mode
    bool singlePageWithScrollbar = gSettings->scrollbarInSinglePage && dmIsSinglePage;

    int lineHeight = DpiScale(ScrollLineAmount(gSettings->scrollLineAmount));
    bool isFitPage = (kZoomFitPage == ctrl->GetZoomVirtual());
    if (!IsContinuous(ctrl->GetDisplayMode()) && isFitPage) {
        lineHeight = 1;
    }
    // logf("OnVscroll: msg=%s, min: %d, max: %d, nPage: %d, pos: %d, fit page: %d, lineHeight: %d,
    // singlePageWithScrollbar: %d\n", scrollMsgStr(msg), si.nMin,
    //      si.nMax, si.nPage, si.nPos, isFitPage ? 1 : 0, lineHeight, singlePageWithScrollbar);

    if (singlePageWithScrollbar) {
        // In SinglePage mode, scrollbar position directly corresponds to page number
        int targetPage = ctrl->CurrentPageNo();

        switch (msg) {
            case SB_TOP:
                targetPage = 1;
                break;
            case SB_BOTTOM:
                targetPage = ctrl->PageCount();
                break;
            case SB_LINEUP:
                targetPage = std::max(1, targetPage - 1);
                break;
            case SB_LINEDOWN:
                targetPage = std::min(ctrl->PageCount(), targetPage + 1);
                break;
            case kSbHalfPageUp:
                targetPage = std::max(1, targetPage - 1);
                break;
            case kSbHalfPageDown:
                targetPage = std::min(ctrl->PageCount(), targetPage + 1);
                break;
            case SB_PAGEUP:
                targetPage = std::max(1, targetPage - 1);
                break;
            case SB_PAGEDOWN:
                targetPage = std::min(ctrl->PageCount(), targetPage + 1);
                break;
            case SB_THUMBTRACK:
                targetPage = si.nTrackPos + 1;
                break;
        }

        // Navigate to the target page
        if (targetPage != ctrl->CurrentPageNo()) {
            ctrl->GoToPage(targetPage, true);
            ReadAloudOnUserViewChanged(win);
        }
        if (scrollDown || scrollUp) {
            OnDocumentVerticalScrollIntent(win, scrollDown);
        }
        return;
    }

    // Original logic for other display modes

    // SmoothScroll eases wheel input and arrow-key / scrollbar line steps
    // (issue #4662). Page-up/down and thumb stay instant.
    bool isLineScroll = (msg == SB_LINEUP || msg == SB_LINEDOWN);
    bool useSmoothScroll = gSettings->smoothScroll && (gInMouseWheelScroll || isLineScroll);
    // While a smooth scroll is in flight the animation moves the view a bit at a
    // time, and ScrollYTo -> UpdateScrollbars keeps nPos on that lagging
    // position. Stepping from it discards the distance still to be travelled, so
    // a fast stream of wheel / key-repeat events advances only a fraction of
    // what the same events do with SmoothScroll off (issue #5857). Step from
    // the pending target instead, so they accumulate the same total distance.
    if (useSmoothScroll && win->scrollAnimActive) {
        si.nPos = win->scrollTargetY;
    }

    int currPos = si.nPos;
    int halfPage = (int)si.nPage / 2;
    switch (msg) {
        case SB_TOP:
            si.nPos = si.nMin;
            break;
        case SB_BOTTOM:
            si.nPos = si.nMax;

            break;
        case SB_LINEUP:
            si.nPos -= lineHeight;
            break;
        case SB_LINEDOWN:
            si.nPos += lineHeight;
            break;
        case kSbHalfPageUp:
            si.nPos -= halfPage;
            break;
        case kSbHalfPageDown:
            si.nPos += halfPage;
            break;
        case SB_PAGEUP:
            si.nPos -= (int)si.nPage;
            break;
        case SB_PAGEDOWN:
            si.nPos += (int)si.nPage;
            break;
        case SB_THUMBTRACK:
            si.nPos = si.nTrackPos;
            break;
    }
    // logf("OnVScroll: nPos: %d\n", si.nPos);

    // Set the position and then retrieve it.  Due to adjustments
    // by Windows it may not be the same as the value set.
    si.fMask = SIF_POS;
    bool showScrollbar = !ScrollbarsAreHidden();
    BOOL showWinScrollbar = showScrollbar && !overlayMode;
    BOOL showOverScrollbar = showScrollbar && useOverlay;
    if (useSmoothScroll) {
        // Don't hand the target to the scrollbar: the thumb would jump ahead of
        // the view and be pulled back by the next animation tick (which updates
        // it via ScrollYTo -> UpdateScrollbars as the view actually moves).
        // Clamp the way SetScrollInfo would have, so the target stays in range.
        int maxPos = si.nMax - (int)si.nPage + 1;
        si.nPos = limitValue(si.nPos, si.nMin, std::max(si.nMin, maxPos));
        // Still reveal the thin smart bar on wheel / key input (without moving
        // the thumb to the pending target). Mouse-move tracking alone is not
        // enough when the user scrolls with the wheel while the cursor is still
        // (#5859).
        if (showOverScrollbar) {
            OverlayScrollbarNotifyScroll(win->overlayScrollV);
        }
    } else {
        SetScrollInfo(win->hwndCanvas, SB_VERT, &si, showWinScrollbar);
        GetScrollInfo(win->hwndCanvas, SB_VERT, &si);
        if (showOverScrollbar) {
            OverlayScrollbarSetInfo(win->overlayScrollV, &si, TRUE);
        }
    }

    // If the position has changed or we're dealing with a touchpad scroll event,
    // scroll the window and update it
    if (si.nPos != currPos || msg == SB_THUMBTRACK) {
        if (useSmoothScroll) {
            StartOrUpdateSmoothScrollY(win, si.nPos);
        } else {
            // Page / thumb / programmatic scroll, or SmoothScroll off: apply immediately.
            StopSmoothScroll(win);
            win->AsFixed()->ScrollYTo(si.nPos);
            ReadAloudOnUserViewChanged(win);
        }
    }
    if (scrollDown || scrollUp) {
        OnDocumentVerticalScrollIntent(win, scrollDown);
    }
}

static void OnHScroll(MainWindow* win, WPARAM wp) {
    ReportIf(!win->AsFixed());

    bool overlayMode = ScrollbarsUseOverlay();
    bool useOverlay = overlayMode && win->overlayScrollH;
    SCROLLINFO si{};
    si.cbSize = sizeof(si);
    si.fMask = SIF_ALL;
    if (useOverlay) {
        OverlayScrollbarGetInfo(win->overlayScrollH, &si);
    } else {
        GetScrollInfo(win->hwndCanvas, SB_HORZ, &si);
    }

    int currPos = si.nPos;
    USHORT msg = LOWORD(wp);
    int lineAmount = DpiScale(ScrollLineAmount(gSettings->scrollLineAmount));
    switch (msg) {
        case SB_LEFT:
            si.nPos = si.nMin;
            break;
        case SB_RIGHT:
            si.nPos = si.nMax;
            break;
        case SB_LINELEFT:
            si.nPos -= lineAmount;
            break;
        case SB_LINERIGHT:
            si.nPos += lineAmount;
            break;
        case SB_PAGELEFT:
            si.nPos -= (int)si.nPage;
            break;
        case SB_PAGERIGHT:
            si.nPos += (int)si.nPage;
            break;
        case SB_THUMBTRACK:
            si.nPos = si.nTrackPos;
            break;
    }

    // Set the position and then retrieve it.  Due to adjustments
    // by Windows it may not be the same as the value set.
    si.fMask = SIF_POS;
    SetScrollInfo(win->hwndCanvas, SB_HORZ, &si, !overlayMode);
    GetScrollInfo(win->hwndCanvas, SB_HORZ, &si);
    if (useOverlay) {
        OverlayScrollbarSetInfo(win->overlayScrollH, &si, TRUE);
    }

    // If the position has changed or we're dealing with a touchpad scroll event,
    // scroll the window and update it
    if (si.nPos != currPos || msg == SB_THUMBTRACK) {
        win->AsFixed()->ScrollXTo(si.nPos);
        ReadAloudOnUserViewChanged(win);
    }
}

static void DrawMovePattern(MainWindow* win, Point pt, Size size) {
    HWND hwnd = win->hwndCanvas;
    HDC hdc = GetDC(hwnd);
    auto [x, y] = pt;
    auto [dx, dy] = size;
    x += win->annotationBeingMovedOffset.x;
    y += win->annotationBeingMovedOffset.y;
    SetBrushOrgEx(hdc, x, y, nullptr);
    HBRUSH hbrushOld = (HBRUSH)SelectObject(hdc, win->brMovePattern);
    PatBlt(hdc, x, y, dx, dy, PATINVERT);
    SelectObject(hdc, hbrushOld);
    ReleaseDC(hwnd, hdc);
}

static void StartMouseDrag(MainWindow* win, int x, int y, bool right = false) {
    SetCapture(win->hwndCanvas);
    win->mouseAction = MouseAction::Dragging;
    win->dragRightClick = right;
    win->dragPrevPos = Point(x, y);
    if (GetCursor() && !SetLaserPointerCursor(win)) {
        SetCursor(gCursorDrag);
    }
}

static bool IsLineEndpointHandle(ResizeHandle handle) {
    return handle == ResizeHandle::LineStart || handle == ResizeHandle::LineEnd;
}

static bool IsVertexHandle(ResizeHandle handle) {
    return handle == ResizeHandle::Vertex;
}

static bool IsPolyVertexType(AnnotationType tp) {
    return tp == AnnotationType::PolyLine || tp == AnnotationType::Polygon;
}

// Line annotations: hit-test the two endpoints, not the bounding-box handles.
static ResizeHandle GetLineEndpointHandleAt(DisplayModel* dm, Point pt, Annotation* annot) {
    PointF start, end;
    if (!GetLinePoints(annot, start, end)) {
        return ResizeHandle::None;
    }
    Point startPt = dm->CvtToScreen(annot->pageNo, start);
    Point endPt = dm->CvtToScreen(annot->pageNo, end);
    int hs = kResizeHandleSize;
    auto dist = [&](Point p) { return std::max(abs(pt.x - p.x), abs(pt.y - p.y)); };
    int dStart = dist(startPt);
    int dEnd = dist(endPt);
    if (dStart <= hs && dStart <= dEnd) {
        return ResizeHandle::LineStart;
    }
    if (dEnd <= hs) {
        return ResizeHandle::LineEnd;
    }
    return ResizeHandle::None;
}

// PolyLine / Polygon: hit-test each vertex. Returns index, or -1.
static int GetPolyVertexAt(DisplayModel* dm, Point pt, Annotation* annot) {
    if (!annot || !IsPolyVertexType(annot->type)) {
        return -1;
    }
    Vec<PointF> pts = GetVertices(annot);
    int n = len(pts);
    if (n == 0) {
        return -1;
    }
    int hs = kResizeHandleSize;
    int best = -1;
    int bestDist = hs + 1;
    for (int i = 0; i < n; i++) {
        Point p = dm->CvtToScreen(annot->pageNo, pts[i]);
        int d = std::max(abs(pt.x - p.x), abs(pt.y - p.y));
        if (d <= hs && d < bestDist) {
            best = i;
            bestDist = d;
        }
    }
    return best;
}

// Get the resize handle at the given point for the selected annotation
static ResizeHandle GetResizeHandleAt(MainWindow* win, Point pt, Annotation* annot) {
    if (!annot) {
        return ResizeHandle::None;
    }

    DisplayModel* dm = win->AsFixed();
    if (!dm) {
        return ResizeHandle::None;
    }

    int pageNo = annot->pageNo;
    if (!dm->PageVisible(pageNo)) {
        return ResizeHandle::None;
    }

    if (annot->type == AnnotationType::Line) {
        return GetLineEndpointHandleAt(dm, pt, annot);
    }
    if (IsPolyVertexType(annot->type)) {
        return GetPolyVertexAt(dm, pt, annot) >= 0 ? ResizeHandle::Vertex : ResizeHandle::None;
    }
    if (annot->type == AnnotationType::Redact && len(GetQuadPointsAsRect(annot)) > 0) {
        // text-selection marks are a set of quads, not a stretchable rect
        return ResizeHandle::None;
    }

    Rect rect = dm->CvtToScreen(pageNo, GetRect(annot));
    int hs = kResizeHandleSize;

    bool nearLeft = pt.x >= rect.x - hs && pt.x <= rect.x + hs;
    bool nearRight = pt.x >= rect.x + rect.dx - hs && pt.x <= rect.x + rect.dx + hs;
    bool nearTop = pt.y >= rect.y - hs && pt.y <= rect.y + hs;
    bool nearBottom = pt.y >= rect.y + rect.dy - hs && pt.y <= rect.y + rect.dy + hs;
    bool betweenX = pt.x >= rect.x + hs && pt.x <= rect.x + rect.dx - hs;
    bool betweenY = pt.y >= rect.y + hs && pt.y <= rect.y + rect.dy - hs;

    // clang-format off
    // corners have priority over edges
    if (nearLeft  && nearTop)    return ResizeHandle::TopLeft;
    if (nearRight && nearTop)    return ResizeHandle::TopRight;
    if (nearRight && nearBottom) return ResizeHandle::BottomRight;
    if (nearLeft  && nearBottom) return ResizeHandle::BottomLeft;
    // edges
    if (betweenX  && nearTop)    return ResizeHandle::Top;
    if (nearRight && betweenY)   return ResizeHandle::Right;
    if (betweenX  && nearBottom) return ResizeHandle::Bottom;
    if (nearLeft  && betweenY)   return ResizeHandle::Left;
    // clang-format on

    return ResizeHandle::None;
}

// Get the appropriate cursor for a resize handle
static LPWSTR GetCursorForResizeHandle(ResizeHandle handle) {
    switch (handle) {
        case ResizeHandle::TopLeft:
        case ResizeHandle::BottomRight:
            return IDC_SIZENWSE;
        case ResizeHandle::TopRight:
        case ResizeHandle::BottomLeft:
            return IDC_SIZENESW;
        case ResizeHandle::Top:
        case ResizeHandle::Bottom:
            return IDC_SIZENS;
        case ResizeHandle::Left:
        case ResizeHandle::Right:
            return IDC_SIZEWE;
        case ResizeHandle::LineStart:
        case ResizeHandle::LineEnd:
        case ResizeHandle::Vertex:
            return IDC_SIZEALL;
        default:
            return IDC_ARROW;
    }
}

// return true if this was annotation dragging
static bool StopDraggingAnnotation(MainWindow* win, int x, int y, bool aborted) {
    Annotation* annot = win->annotationBeingDragged;
    if (!annot) {
        return false;
    }
    DrawMovePattern(win, win->dragPrevPos, win->annotationBeingMovedSize);

    win->annotationBeingDragged = nullptr;
    if (aborted) {
        return true;
    }

    DisplayModel* dm = win->AsFixed();
    x += win->annotationBeingMovedOffset.x;
    y += win->annotationBeingMovedOffset.y;
    Point pt{x, y};
    int pageNo = dm->GetPageNoByPoint(pt);
    // we can only move annotation within the same page
    if (pageNo == PageNo(annot)) {
        Rect rScreen{x, y, 1, 1};
        RectF r = dm->CvtFromScreen(rScreen, pageNo);
        RectF ar = GetRect(annot);
        r.dx = ar.dx;
        r.dy = ar.dy;
        // logf("prev rect: x=%.2f, y=%.2f, dx=%.2f, dy=%.2f\n", ar.x, ar.y, ar.dx, ar.dy);
        // logf(" new rect: x=%.2f, y=%.2f, dx=%.2f, dy=%.2f\n", r.x, r.y, r.dx, r.dy);
        SetRect(annot, r);
        NotifyAnnotationsChanged(win->CurrentTab());
        MainWindowRerender(win);
        ToolbarUpdateStateForWindow(win, true);
        UpdateAnnotFilterToolbar(win);
    }
    return true;
}

static void StopMouseDrag(MainWindow* win, int x, int y, bool aborted) {
    if (GetCapture() != win->hwndCanvas) {
        return;
    }

    if (GetCursor()) {
        SetCanvasCursor(win, IDC_ARROW);
    }
    ReleaseCapture();

    if (StopDraggingAnnotation(win, x, y, aborted)) {
        return;
    }

    if (aborted) {
        return;
    }

    Size drag(x - win->dragPrevPos.x, y - win->dragPrevPos.y);
    win->MoveDocBy(drag.dx, -2 * drag.dy);
}

static bool StopAnnotationResize(MainWindow* win, bool aborted);

void CancelDrag(MainWindow* win) {
    if (StopAnnotationResize(win, true)) {
        win->mouseAction = MouseAction::None;
        win->linkOnLastButtonDown = nullptr;
        SetCanvasCursor(win, IDC_ARROW);
        return;
    }
    auto pt = win->dragPrevPos;
    auto [x, y] = pt;
    StopMouseDrag(win, x, y, true);
    win->mouseAction = MouseAction::None;
    win->linkOnLastButtonDown = nullptr;
    win->annotationBeingDragged = nullptr;
    win->annotationBeingResized = false;
    SetCanvasCursor(win, IDC_ARROW);
}

bool IsDragDistance(int x1, int x2, int y1, int y2) {
    int dx = abs(x1 - x2);
    int dragDx = GetSystemMetrics(SM_CXDRAG);
    if (dx > dragDx) {
        return true;
    }

    int dy = abs(y1 - y2);
    int dragDy = GetSystemMetrics(SM_CYDRAG);
    return dy > dragDy;
}

// Forward declaration
static RectF CalculateResizedRect(MainWindow* win, int x, int y);

// --- touch text selection (issue #538) --------------------------------------
// Everything here logs under "touch:" so a session on a real touchscreen can be
// read back from the log (run with -log -log-to-file <path>).

// How long a finger has to hold still before it counts as a long press.
// 500ms is the usual touch long-press (Chrome, Android). 300ms was too easy
// to trip while starting a scroll (issue #6006). The gesture engine only
// reports a contact once it has decided it is a pan, and the finger drifts
// while settling, so the GID_PAN path measures from when it comes to rest
// rather than from touch-down.
constexpr DWORD kTouchLongPressMs = 500;

// How far from the word the press may land and still count as meaning it.
constexpr int kTouchLongPressMaxDistDip = 40;

// How long after a finger leaves the glass mouse moves are still assumed to be
// echoes of that touch rather than someone reaching for the mouse.
constexpr DWORD kTouchMouseTakeoverMs = 1000;

// Mouse messages Windows synthesizes from a finger or a pen carry this
// signature in the extra info; a real mouse doesn't.
static bool IsMouseMessageFromTouch() {
    constexpr ULONG_PTR kSignatureMask = 0xFFFFFF00;
    constexpr ULONG_PTR kPenOrTouchSignature = 0xFF515700;
    auto extra = (ULONG_PTR)GetMessageExtraInfo();
    return (extra & kSignatureMask) == kPenOrTouchSignature;
}

static Str TouchSelHandleName(TouchSelHandle h) {
    switch (h) {
        case TouchSelHandle::Start:
            return StrL("start");
        case TouchSelHandle::End:
            return StrL("end");
        default:
            return StrL("none");
    }
}

// This contact is scrolling, so a later pause must not become a long press
// (issue #6006).
static void MarkTouchPanDidScroll(MainWindow* win) {
    win->touchState.panDidScroll = true;
    KillTimer(win->hwndCanvas, kTouchLongPressTimerID);
}

static void ResetTouchLongPress(MainWindow* win) {
    win->touchState.longPressFired = false;
    win->touchState.panMovedOnce = false;
    win->touchState.panDidScroll = false;
    win->touchLongPressDone = false;
}

// Dragging a touch selection handle: the other end stays put and the selection
// is extended from it to wherever the finger is now.
static void DragTouchSelHandle(MainWindow* win, int x, int y) {
    DisplayModel* dm = win->AsFixed();
    if (!dm || !dm->textSelection) {
        return;
    }
    // aim at the text the handle belongs to, not at the fingertip below it
    int handleDy = DpiScale(8);
    Point pt(x, y - handleDy);
    int pageNo = dm->GetPageNoByPoint(pt);
    if (!win->ctrl->ValidPageNo(pageNo)) {
        logf("touch: DragTouchSelHandle at %d,%d: no page there\n", x, y);
        return;
    }
    int fromPage, fromGlyph, toPage, toGlyph;
    dm->textSelection->GetGlyphRange(&fromPage, &fromGlyph, &toPage, &toGlyph);
    // anchor on the end that isn't moving
    if (win->touchSelDragging == TouchSelHandle::Start) {
        dm->textSelection->StartAt(toPage, toGlyph);
    } else {
        dm->textSelection->StartAt(fromPage, fromGlyph);
    }
    PointF ptf = dm->CvtFromScreen(pt, pageNo);
    dm->textSelection->SelectUpTo(pageNo, ptf.x, ptf.y);

    DeleteOldSelectionInfo(win, false);
    WindowTab* tab = win->CurrentTab();
    tab->selectionOnPage = SelectionOnPage::FromTextSelect(&dm->textSelection->result);
    win->showSelection = tab->selectionOnPage != nullptr;
    ScheduleRepaint(win, 0);
}

// A long press over a word selects it and puts a drag handle under each end.
// Returns true when it handled the press, i.e. the context menu should not
// open. x, y are canvas coordinates.
static bool OnTouchLongPress(MainWindow* win, int x, int y) {
    DisplayModel* dm = win->AsFixed();
    logf("touch: long press at %d,%d, dm=%d\n", x, y, (int)(dm != nullptr));
    if (!dm || !dm->textSelection) {
        return false;
    }
    Point pt(x, y);
    int pageNo = dm->GetPageNoByPoint(pt);
    logf("touch: long press overText=%d pageNo=%d\n", (int)dm->IsOverText(pt), pageNo);
    if (!win->ctrl->ValidPageNo(pageNo)) {
        return false;
    }
    // a long press replaces whatever a stray drag may have started
    if (win->mouseAction != MouseAction::None) {
        logf("touch: long press cancelling mouseAction=%d\n", (int)win->mouseAction);
        win->mouseAction = MouseAction::None;
        win->dragStartPending = false;
        win->selectingByWord = false;
        if (GetCapture() == win->hwndCanvas) {
            ReleaseCapture();
        }
        KillTimer(win->hwndCanvas, kSelectSmoothScrollTimerID);
    }

    PointF ptf = dm->CvtFromScreen(pt, pageNo);
    dm->textSelection->SelectWordAt(pageNo, ptf.x, ptf.y);

    DeleteOldSelectionInfo(win, false);
    WindowTab* tab = win->CurrentTab();
    tab->selectionOnPage = SelectionOnPage::FromTextSelect(&dm->textSelection->result);
    int nRects = tab->selectionOnPage ? len(*tab->selectionOnPage) : 0;
    logf("touch: long press selected %d rect(s)\n", nRects);
    if (nRects == 0) {
        return false;
    }
    // SelectWordAt() snaps to the nearest glyph, which is what a fingertip
    // needs -- it is far bigger than a letter and IsOverText() rejects most
    // presses that plainly meant a word. The nearest word can be anywhere on
    // the page though, so it only counts if the press landed near it.
    Rect wordRc = (*tab->selectionOnPage)[0].GetRect(dm);
    for (SelectionOnPage& s : *tab->selectionOnPage) {
        wordRc = wordRc.Union(s.GetRect(dm));
    }
    int maxDist = DpiScale(kTouchLongPressMaxDistDip);
    Rect nearRc = wordRc;
    nearRc.Inflate(maxDist, maxDist);
    if (!nearRc.Contains(pt)) {
        logf("touch: nearest word at %d,%d %dx%d is too far from %d,%d, ignoring\n", wordRc.x, wordRc.y, wordRc.dx,
             wordRc.dy, x, y);
        DeleteOldSelectionInfo(win, true);
        return false;
    }
    win->showSelection = true;
    win->touchSelHandles = true;
    win->touchSelDragging = TouchSelHandle::None;
    ScheduleRepaint(win, 0);
    return true;
}

static void OnMouseMove(MainWindow* win, int x, int y, WPARAM key) {
    DisplayModel* dm = win->AsFixed();
    // ReportIf(!dm); // can happen if reload fails, we delete DisplayModel
    if (!dm) return;

    if (AnnotationPlacementOnMouseMove(win, Point{x, y}, key)) {
        return;
    }

    if (win->touchSelDragging != TouchSelHandle::None) {
        DragTouchSelHandle(win, x, y);
        return;
    }
    if (win->lastInputWasTouch) {
        // a finger that wanders isn't holding still, so it isn't a long press
        int slop = DpiScale(10);
        if (abs(x - win->touchDownPos.x) > slop || abs(y - win->touchDownPos.y) > slop) {
            // Only kill the timer. Synthesized mouse moves around a touch can
            // carry a stale position, so this must not mark the contact as a
            // scroll or a still hold would never select.
            KillTimer(win->hwndCanvas, kTouchLongPressTimerID);
        }
    }
    if (win->touchSelHandles && !IsMouseMessageFromTouch()) {
        // A real mouse takes the handles away -- they're finger furniture --
        // while leaving the selection alone (issue #538). Windows also
        // synthesizes moves around a touch, untagged and sometimes carrying a
        // stale position, so anything arriving while a finger is on the glass
        // or has only just left doesn't count as the mouse taking over.
        DWORD sinceTouch = (DWORD)GetTickCount64() - win->touchLastActivityTime;
        if (win->touchPointerId >= 0 || sinceTouch < kTouchMouseTakeoverMs) {
            return;
        }
        logf("touch: mouse moved to %d,%d (%dms after touch), hiding selection handles\n", x, y, (int)sinceTouch);
        HideTouchSelHandles(win);
    }

    if (win->InPresentation()) {
        if (PM_BLACK_SCREEN == win->presentation || PM_WHITE_SCREEN == win->presentation) {
            // logf("OnMouseMove: hiding cursor because black screen or white screen\n");
            SetCursor((HCURSOR) nullptr);
            return;
        }

        bool showingCursor = (GetCursor() != nullptr);
        bool sameAsLastPos = win->dragPrevPos.Eq(x, y);
        // logf("OnMouseMove(): win->InPresentation() (%d, %d) showingCursor: %d, same as last pos: %d\n", x,
        // y,
        //     (int)showingCursor, (int)sameAsLastPos);
        if (!sameAsLastPos) {
            // shortly display the cursor if the mouse has moved and the cursor is hidden
            if (!showingCursor) {
                // logf("OnMouseMove: temporary showing cursor\n");
                if (win->mouseAction == MouseAction::None) {
                    SetCanvasCursor(win, IDC_ARROW);
                } else {
                    SendMessageW(win->hwndCanvas, WM_SETCURSOR, 0, 0);
                }
            }
            if (win->dragPrevPos.Eq(-2, -3)) {
                // hack: hide cursor immediately. see EnterFullScreen
                SetTimer(win->hwndCanvas, kHideCursorTimerID, 1, nullptr);
            } else {
                // logf("OnMouseMove: starting kHideCursorTimerID\n");
                SetTimer(win->hwndCanvas, kHideCursorTimerID, kHideCursorDelayInMs, nullptr);
            }
        }
    }

    Point pos{x, y};
    int pageNo = dm->GetPageNoByPoint(pos);
    if (dm->ValidPageNo(pageNo)) {
        dm->GetEngine()->RequestTextExtraction(pageNo);
    }

    NotificationWnd* cursorPosNotif = GetNotificationForGroup(win->hwndCanvas, kNotifCursorPos);

    if (win->textDragPending) {
        if (!IsDragDistance(x, win->dragStart.x, y, win->dragStart.y)) {
            return;
        }
        // threshold met: initiate OLE drag-drop of selected text
        win->textDragPending = false;
        win->dragStartPending = false;
        if (GetCapture() == win->hwndCanvas) {
            ReleaseCapture();
        }
        StartTextDragDrop(win);
        return;
    }

    if (win->imageDragPending) {
        if (!IsDragDistance(x, win->dragStart.x, y, win->dragStart.y)) {
            return;
        }
        win->imageDragPending = false;
        win->dragStartPending = false;
        if (GetCapture() == win->hwndCanvas) {
            ReleaseCapture();
        }
        StartImageDragDrop(win);
        win->imageDragElement = nullptr;
        win->imageDragPageNo = -1;
        return;
    }

    if (win->dragStartPending) {
        if (!IsDragDistance(x, win->dragStart.x, y, win->dragStart.y)) {
            return;
        }
        win->dragStartPending = false;
        win->linkOnLastButtonDown = nullptr;
    }

    Point prevPos = win->dragPrevPos;
    switch (win->mouseAction) {
        case MouseAction::None: {
            Annotation* annot = dm->GetAnnotationAtPos(pos, nullptr);
            Annotation* prev = win->annotationUnderCursor;
            bool editPdf = win->pdfAnnotationsToolbarEnabled;
            int srcPageNo = -1;
            IPageElement* el = dm->GetElementAtPos(pos, &srcPageNo);
            if (el && el->Is(kindPageElementDest) && gSettings->disableLinks) {
                el = nullptr;
            }
            int hoverDelayMs = gSettings->citationHoverDelay;
            if (annot != prev) {
                if (editPdf) {
                    ScheduleRepaint(win, 0);
                }
            }
            RemoveNotificationsForGroup(win->hwndCanvas, kNotifAnnotation);
            win->annotationUnderCursor = annot;
            if (editPdf) {
                UpdateAnnotationHoverOverlay(win);
            } else {
                HideAnnotationHoverOverlay(win);
            }

            RefHoverOnCanvasMouseMove(win->refHover, win->hwndCanvas, win->ctrl, win->linkHandler, dm, x, y, el,
                                      srcPageNo, hoverDelayMs);
            break;
        }

        case MouseAction::Scrolling: {
            win->annotationUnderCursor = nullptr;
            HideAnnotationHoverOverlay(win);
            win->yScrollSpeed = (float)(y - win->dragStart.y) / kSelectSmoothScrollSlowDownFactor;
            win->xScrollSpeed = (float)(x - win->dragStart.x) / kSelectSmoothScrollSlowDownFactor;
            break;
        }
        case MouseAction::SelectingText:
            if (GetCursor()) {
                SetCursorCached(IDC_IBEAM);
            }
            [[fallthrough]];
        case MouseAction::Selecting: {
            win->annotationUnderCursor = nullptr;
            HideAnnotationHoverOverlay(win);
            if (win->selectionDragEdge != SelectionDragEdge::None) {
                // move / resize existing rectangular selection
                UpdateRectangularSelectionEdit(win, x, y);
                SetCursorCached(CursorIdForSelectionEdge(win->selectionDragEdge));
            } else {
                // creating a new selection from the start corner
                win->selectionRect.dx = x - win->selectionRect.x;
                win->selectionRect.dy = y - win->selectionRect.y;
                win->selectionMeasure = dm->CvtFromScreen(win->selectionRect).Size();
            }
            OnSelectionEdgeAutoscroll(win, x, y);
            ScheduleRepaint(win, 0);
            break;
        }
        case MouseAction::Dragging: {
            Annotation* annot = win->annotationBeingDragged;
            if (annot) {
                if (win->annotationBeingResized) {
                    // During resize, calculate and apply new rectangle in real-time
                    win->dragPrevPos = pos;
                    // Keep the resize cursor active during resize
                    auto handle = (ResizeHandle)win->resizeHandle;
                    SetCursorCached(GetCursorForResizeHandle(handle));

                    if (IsLineEndpointHandle(handle)) {
                        PointF pagePt = dm->CvtFromScreen(Point{x, y}, PageNo(annot));
                        if (handle == ResizeHandle::LineStart) {
                            win->annotationLinePreviewStart = pagePt;
                        } else {
                            win->annotationLinePreviewEnd = pagePt;
                        }
                        // Overlay only: leave the PDF page bitmap alone until
                        // the drag ends.
                        ScheduleRepaint(win, 0);
                    } else if (IsVertexHandle(handle)) {
                        PointF pagePt = dm->CvtFromScreen(Point{x, y}, PageNo(annot));
                        int idx = win->annotationResizeVertexIndex;
                        if (idx >= 0 && idx < len(win->annotationVertexPreview)) {
                            win->annotationVertexPreview[idx] = pagePt;
                        }
                        ScheduleRepaint(win, 0);
                    } else if (win->annotationResizeOutlineOnly) {
                        // Outline only: writing the annotation re-lays out its
                        // text and re-renders the page, far too slow to do on
                        // every mouse move.
                        win->annotationResizePreviewRect = CalculateResizedRect(win, x, y);
                        ScheduleRepaint(win, 0);
                    } else {
                        RectF newRect = CalculateResizedRect(win, x, y);
                        SetRect(annot, newRect);
                        // Keep the bounds indicator tracking the pointer using the
                        // existing page bitmap. Re-render the PDF only after the
                        // resize has been idle for a moment.
                        ScheduleRepaint(win, 0);
                        win->annotationResizeRerenderTimer = SetTimer(win->hwndCanvas, kAnnotationResizeRerenderTimerID,
                                                                      kAnnotationResizeRerenderDelayMs, nullptr);
                        ReportIf(!win->annotationResizeRerenderTimer);
                    }
                } else {
                    Size size = win->annotationBeingMovedSize;
                    DrawMovePattern(win, prevPos, size);
                    DrawMovePattern(win, pos, size);
                }
            } else {
                win->MoveDocBy(win->dragPrevPos.x - x, win->dragPrevPos.y - y);
            }
            break;
        }
    }
    win->dragPrevPos = pos;

    if (cursorPosNotif) {
        UpdateCursorPositionHelper(win, pos, cursorPosNotif);
    }
}

static void StartAnnotationDrag(MainWindow* win, Annotation* annot, Point& pt) {
    win->annotationBeingDragged = annot;
    DisplayModel* dm = win->AsFixed();
    CreateMovePatternLazy(win);
    RectF r = GetRect(annot);
    int pageNo = PageNo(annot);
    Rect rScreen = dm->CvtToScreen(pageNo, r);
    win->annotationBeingMovedSize = {rScreen.dx, rScreen.dy};
    int offsetX = rScreen.x - pt.x;
    int offsetY = rScreen.y - pt.y;
    win->annotationBeingMovedOffset = Point{offsetX, offsetY};
    DrawMovePattern(win, pt, win->annotationBeingMovedSize);
}

// Helper function to calculate new rectangle during resize
static RectF CalculateResizedRect(MainWindow* win, int x, int y) {
    DisplayModel* dm = win->AsFixed();
    Annotation* annot = win->annotationBeingDragged;
    int pageNo = PageNo(annot);

    // Convert screen coordinates to page coordinates
    Rect screenPt{x, y, 1, 1};
    RectF pagePt = dm->CvtFromScreen(screenPt, pageNo);

    RectF orig = win->annotationOriginalRect;
    RectF r = orig;

    Point startPt = win->dragStart;
    Rect startScreen{startPt.x, startPt.y, 1, 1};
    RectF startPage = dm->CvtFromScreen(startScreen, pageNo);

    float deltaX = pagePt.x - startPage.x;
    float deltaY = pagePt.y - startPage.y;

    const float minSize = 10.0F;
    auto handle = (ResizeHandle)win->resizeHandle;

    bool moveLeft =
        handle == ResizeHandle::TopLeft || handle == ResizeHandle::Left || handle == ResizeHandle::BottomLeft;
    bool moveRight =
        handle == ResizeHandle::TopRight || handle == ResizeHandle::Right || handle == ResizeHandle::BottomRight;
    bool moveTop = handle == ResizeHandle::TopLeft || handle == ResizeHandle::Top || handle == ResizeHandle::TopRight;
    bool moveBottom =
        handle == ResizeHandle::BottomLeft || handle == ResizeHandle::Bottom || handle == ResizeHandle::BottomRight;

    if (moveLeft) {
        r.x = orig.x + deltaX;
        r.dx = orig.dx - deltaX;
        if (r.dx < minSize) {
            r.x = orig.x + orig.dx - minSize;
            r.dx = minSize;
        }
    }
    if (moveRight) {
        r.dx = orig.dx + deltaX;
        r.dx = std::max(r.dx, minSize);
    }
    if (moveTop) {
        r.y = orig.y + deltaY;
        r.dy = orig.dy - deltaY;
        if (r.dy < minSize) {
            r.y = orig.y + orig.dy - minSize;
            r.dy = minSize;
        }
    }
    if (moveBottom) {
        r.dy = orig.dy + deltaY;
        r.dy = std::max(r.dy, minSize);
    }

    float aspect = win->annotationResizeAspectRatio;
    if (aspect > 0) {
        bool widthDriven = moveLeft || moveRight;
        if (widthDriven && (moveTop || moveBottom)) {
            float widthChange = orig.dx > 0 ? fabsf(r.dx - orig.dx) / orig.dx : 0;
            float heightChange = orig.dy > 0 ? fabsf(r.dy - orig.dy) / orig.dy : 0;
            widthDriven = widthChange >= heightChange;
        }
        if (widthDriven) {
            r.dx = std::max(r.dx, minSize * aspect);
            r.dy = r.dx / aspect;
        } else {
            r.dy = std::max(r.dy, minSize);
            r.dx = r.dy * aspect;
        }

        if (moveLeft) {
            r.x = orig.x + orig.dx - r.dx;
        } else if (moveRight) {
            r.x = orig.x;
        } else {
            r.x = orig.x + (orig.dx - r.dx) / 2;
        }
        if (moveTop) {
            r.y = orig.y + orig.dy - r.dy;
        } else if (moveBottom) {
            r.y = orig.y;
        } else {
            r.y = orig.y + (orig.dy - r.dy) / 2;
        }
    }

    return r;
}

static void StartAnnotationResize(MainWindow* win, Annotation* annot, Point& pt, ResizeHandle handle) {
    CancelAnnotationResizeRerender(win);
    // the drag rewrites the annotation on every mouse move; one undo step
    BeginPdfEditOperation(win, "Resize annotation");
    win->annotationBeingDragged = annot;
    win->annotationBeingResized = true;
    // A completed right-click leaves dragRightClick set. This is a new
    // left-button drag; otherwise its button-up is mistaken for a right drag
    // and the annotation keeps resizing as the unpressed mouse moves (#5933).
    win->dragRightClick = false;
    win->resizeHandle = (int)handle;
    win->dragStart = pt;
    RectF r = GetRect(annot);
    win->annotationOriginalRect = r;
    win->annotationResizePreviewRect = r;
    // free text lays its text out again on every write; keep the drag to the
    // outline and write the annotation once, when the drag ends
    win->annotationResizeOutlineOnly =
        annot->type == AnnotationType::FreeText && !IsLineEndpointHandle(handle) && !IsVertexHandle(handle);
    win->annotationOriginalLineStart = {};
    win->annotationOriginalLineEnd = {};
    win->annotationLinePreviewStart = {};
    win->annotationLinePreviewEnd = {};
    win->annotationResizeVertexIndex = -1;
    VecReset(win->annotationVertexPreview);
    if (annot->type == AnnotationType::Line) {
        GetLinePoints(annot, win->annotationOriginalLineStart, win->annotationOriginalLineEnd);
        win->annotationLinePreviewStart = win->annotationOriginalLineStart;
        win->annotationLinePreviewEnd = win->annotationOriginalLineEnd;
    } else if (IsPolyVertexType(annot->type)) {
        win->annotationVertexPreview = GetVertices(annot);
        win->annotationResizeVertexIndex = GetPolyVertexAt(win->AsFixed(), pt, annot);
    }
    win->annotationResizeAspectRatio = 0;
    if (annot->type == AnnotationType::Stamp && r.dx > 0 && r.dy > 0) {
        // Rubber stamps are regenerated at a fixed aspect ratio by MuPDF;
        // preserving image-stamp aspect also avoids distortion.
        win->annotationResizeAspectRatio = r.dx / r.dy;
    }
    SetCapture(win->hwndCanvas);
    win->mouseAction = MouseAction::Dragging;
    win->dragPrevPos = pt;
}

static bool StopAnnotationResize(MainWindow* win, bool aborted) {
    if (!win->annotationBeingResized) {
        return false;
    }

    Annotation* annot = win->annotationBeingDragged;
    auto handle = (ResizeHandle)win->resizeHandle;
    PointF lineStart = win->annotationLinePreviewStart;
    PointF lineEnd = win->annotationLinePreviewEnd;
    bool outlineOnly = win->annotationResizeOutlineOnly;
    RectF previewRect = win->annotationResizePreviewRect;
    win->annotationBeingResized = false;
    win->annotationResizeOutlineOnly = false;
    win->annotationBeingDragged = nullptr;
    CancelAnnotationResizeRerender(win);

    // Release mouse capture and reset cursor
    if (GetCapture() == win->hwndCanvas) {
        ReleaseCapture();
    }
    SetCanvasCursor(win, IDC_ARROW);

    if (aborted || !annot) {
        EndPdfEditOperation(win);
        ScheduleRepaint(win, 0);
        return true;
    }

    if (IsLineEndpointHandle(handle)) {
        SetLinePoints(annot, lineStart, lineEnd);
    } else if (IsVertexHandle(handle)) {
        SetVertices(annot, win->annotationVertexPreview);
    } else if (outlineOnly) {
        SetRect(annot, previewRect);
    }

    // Other rectangle resizes already wrote the annot during mouse move.
    EndPdfEditOperation(win);
    NotifyAnnotationsChanged(win->CurrentTab());
    MainWindowRerender(win);
    ToolbarUpdateStateForWindow(win, true);
    UpdateAnnotFilterToolbar(win);

    return true;
}

// Windows has no triple-click message, so we detect it ourselves: a left button
// down shortly after (and near) the double-click that selected a word (#694)
static DWORD gLastWordSelectTime = 0;
static Point gLastWordSelectPos;

static bool IsTripleClick(int x, int y) {
    if ((DWORD)GetMessageTime() - gLastWordSelectTime > GetDoubleClickTime()) {
        return false;
    }
    int dx = abs(x - gLastWordSelectPos.x);
    int dy = abs(y - gLastWordSelectPos.y);
    return dx <= GetSystemMetrics(SM_CXDOUBLECLK) && dy <= GetSystemMetrics(SM_CYDOUBLECLK);
}

// a full-page image (e.g. a scanned page) shouldn't start an image drag-out:
// there click-and-drag is expected to pan the page (issue #5754). detect it by
// comparing the image's area to the page's.
static bool IsFullPageImage(DisplayModel* dm, IPageElement* el, int pageNo) {
    // in image documents every page is a full-page image and dragging
    // it out to another app is the expected behavior
    Kind k = dm->GetEngine()->kind;
    if (k == kindEngineImage || k == kindEngineImageDir || k == kindEngineComicBooks) {
        return false;
    }
    if (!dm->ValidPageNo(pageNo)) {
        return false;
    }
    RectF pageRc = dm->GetEngine()->PageMediabox(pageNo);
    float pageArea = pageRc.dx * pageRc.dy;
    if (pageArea <= 0) {
        return false;
    }
    RectF imgRc = el->GetRect();
    float imgArea = imgRc.dx * imgRc.dy;
    return imgArea >= 0.8f * pageArea;
}

static bool MouseHasCtrl(WPARAM key) {
    return IsCtrlPressed() || bit::IsMaskSet(key, (WPARAM)MK_CONTROL);
}

static void OpenOrSelectEditAnnotation(WindowTab* tab, Annotation* annot) {
    if (!tab || !annot) {
        return;
    }
    SetSelectedAnnotation(tab, annot);
    HideAnnotationHoverOverlay(tab->win);
}

static void OnMouseLeftButtonDown(MainWindow* win, int x, int y, WPARAM key) {
    // lf("Left button clicked on %d %d", x, y);
    if (IsRightDragging(win)) {
        return;
    }

    if (AnnotationPlacementOnLeftDown(win, Point{x, y}, key)) {
        return;
    }

    RefHoverOnCanvasLeftButtonDown(win->refHover, win->hwndCanvas);

    if (MouseAction::Scrolling == win->mouseAction) {
        win->mouseAction = MouseAction::None;
        return;
    }

    if (win->mouseAction != MouseAction::None) {
        // this can be MouseAction::SelectingText (4)
        // can't reproduce it so far
        logf("OnMouseLeftButtonDown: win->mouseAction=%d\n", (int)win->mouseAction);
        // ReportIf(win->mouseAction != MouseAction::Idle);
        win->mouseAction = MouseAction::None;
        return;
    }

    HwndSetFocus(win->hwndFrame);
    DisplayModel* dm = win->AsFixed();
    ReportIf(!dm);
    Point pt{x, y};

    // placing a new signature: the next drag draws the box, a click puts a
    // default-size one at the pointer (issue #5967). Consume the press so it
    // doesn't toggle a form field or start a text selection.
    if (IsPlacingSignature(win)) {
        win->dragStartPending = true;
        win->dragStart = pt;
        OnSelectionStart(win, x, y, key, true);
        return;
    }

    // remember how this sequence started: WM_CONTEXTMENU, which a long press
    // turns into, doesn't say whether a finger or a mouse produced it
    win->lastInputWasTouch = IsMouseMessageFromTouch();
    win->touchDownPos = pt;
    win->touchDownTime = (DWORD)GetMessageTime();
    if (win->lastInputWasTouch) {
        logf("touch: down at %d,%d, handles=%d, mouseAction=%d\n", x, y, (int)win->touchSelHandles,
             (int)win->mouseAction);
        // when touch arrives as mouse messages rather than gestures, this is
        // what turns a held finger into a long press (issue #538). Skip if
        // WM_POINTER is already timing this contact -- a late synthesized
        // mouse-down must not restart the hold or un-mark a scroll.
        if (win->touchPointerId < 0) {
            ResetTouchLongPress(win);
            SetTimer(win->hwndCanvas, kTouchLongPressTimerID, kTouchLongPressMs, nullptr);
        }
    }

    // grabbing a touch selection handle drags that end of the selection rather
    // than starting a new one (issue #538)
    TouchSelHandle handle = HitTestTouchSelHandle(win, x, y);
    if (handle != TouchSelHandle::None) {
        logf("touch: grabbed %s handle at %d,%d\n", TouchSelHandleName(handle), x, y);
        win->touchSelDragging = handle;
        win->mouseAction = MouseAction::None;
        SetCapture(win->hwndCanvas);
        return;
    }

    WindowTab* tab = win->CurrentTab();
    // PDF form filling: clicking a checkbox / radio-button toggles it; clicking a
    // text or choice field starts in-place editing. Widgets are hit-tested on
    // their own list (GetWidgetAtPos), separate from markup annotations. Consume
    // the click in either case so it doesn't start a drag/selection.
    Annotation* widget = dm->GetWidgetAtPos(pt);
    if (ToggleFormButton(widget)) {
        MainWindowRerender(win);
        win->mouseAction = MouseAction::None;
        return;
    }
    if (StartFormFieldEdit(win, widget)) {
        win->mouseAction = MouseAction::None;
        return;
    }
    // an unsigned signature field is there to be signed: open Sign Document on
    // it rather than making the user find the command in a menu (issue #5964)
    if (StartSignatureFieldSigning(win, widget)) {
        win->mouseAction = MouseAction::None;
        return;
    }

    // Resize handles sit outside the selected annotation's rect. Check them
    // before hit-testing other annotations: otherwise an overlapping annot
    // steals the click, selection jumps, and we resize the wrong one (#5818).
    ResizeHandle resizeHandle = ResizeHandle::None;
    if (tab->selectedAnnotation && AnnotationCanBeResized(tab->selectedAnnotation->type)) {
        resizeHandle = GetResizeHandleAt(win, pt, tab->selectedAnnotation);
    }
    if (resizeHandle != ResizeHandle::None) {
        StartAnnotationResize(win, tab->selectedAnnotation, pt, resizeHandle);
        win->dragStartPending = true;
        win->dragStart = pt;
        win->textDragPending = false;
        return;
    }

    Annotation* annot = dm->GetAnnotationAtPos(pt, tab->selectedAnnotation);
    if (MouseHasCtrl(key) && annot && tab) {
        EnablePdfAnnotationsToolbar(win);
    }
    bool editPdf = win->pdfAnnotationsToolbarEnabled;
    if (editPdf && annot && !AnnotationCanBeMoved(annot->type)) {
        OpenOrSelectEditAnnotation(tab, annot);
        win->textDragPending = false;
        return;
    }
    bool isMoveableAnnot = annot && AnnotationCanBeMoved(annot->type) && annot->type != AnnotationType::Widget;
    // Selecting / dragging an annotation is Edit PDF (Ctrl+click turns that
    // on above). A click outside that mode must stay a page click. An
    // annotation already selected (just created) can still be dragged.
    if (isMoveableAnnot && !editPdf && annot != tab->selectedAnnotation) {
        isMoveableAnnot = false;
    }
    if (isMoveableAnnot && annot != tab->selectedAnnotation) {
        // clicking a shape annotation selects it, so it can be moved /
        // resized right away. Only these: the text markup annotations
        // (highlight and friends) lie on top of text, where a click has to
        // stay a click on the text
        SetSelectedAnnotation(tab, annot);
    }

    if (isMoveableAnnot) {
        StartAnnotationDrag(win, annot, pt);
    } else {
        // Clicking empty page (or non-moveable markup) while a shape is in
        // size-edit must leave that mode on mouse-down. Mouse-up used to skip
        // deselect when the press moved past SM_CXDRAG and became a page pan,
        // so the only ways out were Esc or a right click (issue #5933).
        if (tab && tab->selectedAnnotation) {
            // a click that ended a contents edit is spent on ending it; the
            // annotation the text was written to stays selected
            if (!AnnotContentsEditJustEnded()) {
                SetSelectedAnnotation(tab, nullptr);
            }
            return;
        }
        ReportIf(win->linkOnLastButtonDown);
        IPageElement* pageEl = dm->GetElementAtPos(pt, nullptr);
        if (pageEl && pageEl->Is(kindPageElementDest) && !gSettings->disableLinks) {
            win->linkOnLastButtonDown = pageEl;
        }
    }

    win->dragStartPending = true;
    win->dragStart = pt;
    win->textDragPending = false;

    // - without modifiers, clicking on text starts a text selection
    //   and clicking somewhere else starts a drag
    // - pressing Shift forces dragging
    // - pressing Ctrl forces a rectangular selection
    // - pressing Ctrl+Shift forces text selection
    // - not having CopySelection permission forces dragging
    bool isShift = IsShiftPressed();
    bool isCtrl = IsCtrlPressed();
    bool canCopy = HasPermission(Perm::CopySelection);
    bool isOverText = win->AsFixed()->IsOverText(pt);

    // triple-click selects the whole line (issue #694). Must come before the
    // "already selected text" check below, because the 3rd click lands inside
    // the word that the 2nd click (double-click) just selected.
    if (canCopy && !isShift && !isCtrl && isOverText && IsTripleClick(x, y)) {
        int pageNo = dm->GetPageNoByPoint(pt);
        if (win->ctrl->ValidPageNo(pageNo)) {
            PointF ptf = dm->CvtFromScreen(pt, pageNo);
            dm->textSelection->SelectLineAt(pageNo, ptf.x, ptf.y);
            UpdateTextSelection(win, false);
            win->selectingByWord = false; // a drag now extends by glyph, not word
            win->showSelection = true;
            win->selectionRect = Rect(x, y, 0, 0);
            win->mouseAction = MouseAction::SelectingText;
            win->dragStartPending = false;
            SetCapture(win->hwndCanvas);
            SetTimer(win->hwndCanvas, kSelectSmoothScrollTimerID, kSelectSmoothScrollDelayInMs, nullptr);
            ScheduleRepaint(win, 0);
            gLastWordSelectTime = 0; // so a 4th click doesn't re-trigger
        }
        return;
    }

    // Move / resize an existing rectangular (Ctrl+drag) selection, like the crop
    // rectangle in the save-crop-resize image dialog. Before the drag-out check
    // below: a rectangle is usually drawn over text, and that check would claim
    // every press inside it, so the rectangle could never be moved or resized.
    // Dragging out has nothing to offer here anyway -- a rectangular selection
    // holds no glyphs (that is what IsRectangularSelection tests).
    if (canCopy && !isShift && !isCtrl && IsRectangularSelection(win)) {
        SelectionDragEdge edge = HitTestRectangularSelection(win, x, y);
        if (edge != SelectionDragEdge::None) {
            if (StartRectangularSelectionEdit(win, x, y, edge)) {
                return;
            }
        }
    }

    // if clicking on already selected text, prepare for drag-out instead of new selection
    if (canCopy && !isShift && !isCtrl && isOverText && win->showSelection && IsPointInSelection(win, pt)) {
        win->textDragPending = true;
        win->linkOnLastButtonDown = nullptr;
        SetCapture(win->hwndCanvas);
        return;
    }

    // if clicking on an image, prepare for image drag-out. skip full-page
    // images (e.g. scanned pages), where click-and-drag should pan instead.
    if (canCopy && !isShift && !isCtrl && !isOverText) {
        int elPageNo = -1;
        IPageElement* pageEl = dm->GetElementAtPos(pt, &elPageNo);
        if (pageEl && pageEl->Is(kindPageElementImage) && !IsFullPageImage(dm, pageEl, elPageNo)) {
            win->imageDragPending = true;
            win->imageDragElement = pageEl;
            win->imageDragPageNo = elPageNo;
            win->linkOnLastButtonDown = nullptr;
            SetCapture(win->hwndCanvas);
            return;
        }
    }

    // A finger doesn't rubber-band: dragging pans the page and a long press
    // selects, so letting touch start a selection here only flashes a
    // rectangle before the gesture takes over (issue #538).
    bool startDrag = resizeHandle != ResizeHandle::None || isMoveableAnnot || !canCopy || win->lastInputWasTouch ||
                     (isShift || !isOverText) && !isCtrl;
    if (startDrag) {
        StartMouseDrag(win, x, y);
    } else {
        OnSelectionStart(win, x, y, key);
    }
}

static void OnMouseLeftButtonUp(MainWindow* win, int x, int y, WPARAM key) {
    DisplayModel* dm = win->AsFixed();
    ReportIf(!dm);

    if (AnnotationPlacementOnLeftUp(win, Point{x, y}, key)) {
        return;
    }

    if (win->lastInputWasTouch) {
        DWORD heldMs = (DWORD)GetMessageTime() - win->touchDownTime;
        logf("touch: up at %d,%d after %dms, dragging=%s, mouseAction=%d\n", x, y, (int)heldMs,
             TouchSelHandleName(win->touchSelDragging), (int)win->mouseAction);
    }
    KillTimer(win->hwndCanvas, kTouchLongPressTimerID);
    // let go of a touch selection handle; the handles stay up so the selection
    // can be adjusted again (issue #538)
    if (win->touchSelDragging != TouchSelHandle::None) {
        win->touchSelDragging = TouchSelHandle::None;
        if (GetCapture() == win->hwndCanvas) {
            ReleaseCapture();
        }
        return;
    }

    // click on selected text without dragging: clear selection
    if (win->textDragPending) {
        win->textDragPending = false;
        win->dragStartPending = false;
        if (GetCapture() == win->hwndCanvas) {
            ReleaseCapture();
        }
        DeleteOldSelectionInfo(win, true);
        return;
    }

    // click on image without dragging: just cancel
    if (win->imageDragPending) {
        win->imageDragPending = false;
        win->imageDragElement = nullptr;
        win->imageDragPageNo = -1;
        win->dragStartPending = false;
        if (GetCapture() == win->hwndCanvas) {
            ReleaseCapture();
        }
        return;
    }

    auto ma = win->mouseAction;
    if (MouseAction::None == ma || IsRightDragging(win)) {
        return;
    }

    // Left-up during middle-click / CmdStartAutoScroll mode stops auto-scroll.
    // Also covers the crash-report case of a left-up after the down was lost
    // (different hwnd, focus change, etc.) — just reset cleanly.
    if (MouseAction::Scrolling == ma) {
        win->mouseAction = MouseAction::None;
        win->xScrollSpeed = 0;
        win->yScrollSpeed = 0;
        win->xScrollAccum = 0;
        win->yScrollAccum = 0;
        KillTimer(win->hwndCanvas, kAutoScrollTimerID);
        SetCanvasCursor(win, IDC_ARROW);
        return;
    }

    // Click without move: dragStartPending is still true, so this is a click not a drag.
    bool didDragMouse = !win->dragStartPending || IsDragDistance(x, win->dragStart.x, y, win->dragStart.y);
    if (MouseAction::Dragging == ma) {
        if (win->annotationBeingResized) {
            StopAnnotationResize(win, !didDragMouse);
            // Trigger cursor update after resize
            SendMessageW(win->hwndCanvas, WM_SETCURSOR, 0, 0);
        } else {
            StopMouseDrag(win, x, y, !didDragMouse);
        }
    } else {
        OnSelectionStop(win, x, y, !didDragMouse);
        if (MouseAction::Selecting == ma && win->showSelection) {
            win->selectionMeasure = dm->CvtFromScreen(win->selectionRect).Size();
        }
        if (FinishSignaturePlacement(win, x, y, !didDragMouse)) {
            win->mouseAction = MouseAction::None;
            return;
        }
    }

    win->mouseAction = MouseAction::None;

    Point pt(x, y);
    int pageNo = dm->GetPageNoByPoint(pt);
    PointF ptPage = dm->CvtFromScreen(pt, pageNo);

    // TODO: win->linkHandler->GotoLink might spin the event loop
    IPageElement* link = win->linkOnLastButtonDown;
    win->linkOnLastButtonDown = nullptr;

    WindowTab* tab = win->CurrentTab();
    if (didDragMouse) {
        // no-op
        return;
    }

    if (PM_BLACK_SCREEN == win->presentation || PM_WHITE_SCREEN == win->presentation) {
        /* return from white/black screens in presentation mode */
        win->ChangePresentationMode(PM_ENABLED);
        return;
    }

    // Hit-test the click, not annotationUnderCursor: that is last-move hover
    // and is stale when WM_MOUSEMOVE did not run (or returned early because
    // dragStartPending was still set from the create gesture). Using it
    // re-selected the new stamp when clicking empty page (issue #5933).
    Annotation* clickedAnnot = dm->GetAnnotationAtPos(pt, tab ? tab->selectedAnnotation : nullptr);
    if (MouseHasCtrl(key) && clickedAnnot && tab) {
        EnablePdfAnnotationsToolbar(win);
    }
    bool editPdf = win->pdfAnnotationsToolbarEnabled;

    if (clickedAnnot && tab && editPdf) {
        OpenOrSelectEditAnnotation(tab, clickedAnnot);
        return;
    }

    // Outside Edit PDF mode a click on an annotation did nothing. Show its
    // text, so a long comment can be read without the editing UI (issue #4790)
    if (clickedAnnot && tab && !MouseHasCtrl(key) && AnnotationHasText(clickedAnnot)) {
        if (ShowAnnotationTextPopup(win, clickedAnnot)) {
            return;
        }
    }

    if (clickedAnnot && tab && clickedAnnot == tab->selectedAnnotation) {
        return;
    }

    // clicking next to a selected annotation deselects it. Without this the
    // resize handles stayed up and the only ways out of "editing its size"
    // were Esc or a right click (issue #5933)
    if (tab && tab->selectedAnnotation && !AnnotContentsEditJustEnded()) {
        SetSelectedAnnotation(tab, nullptr);
    }

    if (link && link->GetRect().Contains(ptPage)) {
        /* follow an active link */
        IPageDestination* dest = link->AsLink();
        // highlight the clicked link (as a reminder of the last action once the user returns)
        Kind kind = nullptr;
        if (dest) {
            kind = dest->GetKind();
        }
        if ((kindDestinationLaunchURL == kind || kindDestinationLaunchFile == kind)) {
            DeleteOldSelectionInfo(win, true);
            tab->selectionOnPage = SelectionOnPage::FromRectangle(dm, dm->CvtToScreen(pageNo, link->GetRect()));
            win->showSelection = tab->selectionOnPage != nullptr;
            ScheduleRepaint(win, 0);
        }
        SetCanvasCursor(win, IDC_ARROW);

        // Ctrl+click on internal link: open in new tab and navigate there
        bool isInternal =
            (kindDestinationLaunchURL != kind && kindDestinationLaunchFile != kind && kindDestinationJsMenu != kind);
        if (IsCtrlPressed() && dest && isInternal && tab->filePath) {
            LoadArgs args(tab->filePath, win);
            args.showWin = true;
            args.noPlaceWindow = true;
            args.forceReuse = false;
            MainWindow* newWin = LoadDocument(&args);
            if (newWin && newWin->IsDocLoaded()) {
                newWin->linkHandler->ScrollTo(dest);
            }
            return;
        }

        win->ctrl->HandleLink(dest, win->linkHandler);
        // win->linkHandler->GotoLink(dest);
        return;
    }

    if (win->showSelection) {
        // A click that wasn't a drag, on empty space (clicking text starts a new
        // selection instead): drop the selection, like every other text UI does.
        // This used to go through ClearSearchResult(), which cleared the
        // selection as a side effect until #5737 made find highlights and the
        // selection independent -- leaving Esc as the only way out (issue #5881)
        DeleteOldSelectionInfo(win, true);
        ScheduleRepaint(win, 0);
        return;
    }

    if (win->fwdSearchMark.show && gSettings->forwardSearch.highlightPermanent) {
        /* if there's a permanent forward search mark, hide it */
        win->fwdSearchMark.show = false;
        ScheduleRepaint(win, 0);
        return;
    }

    // Click the left/right fifth of the canvas to turn the page (issue #1203).
    // Presentation mode has its own click-to-turn below. Manga (R2L) reverses
    // the sides so left still advances.
    if (gSettings && gSettings->clickEdgeToTurnPage && tab && tab->ctrl && PM_ENABLED != win->presentation) {
        Rect rc = HwndClientRect(win->hwndCanvas);
        if (rc.dx > 0) {
            int edgeDx = rc.dx / 5;
            bool r2l = dm && dm->GetDisplayR2L();
            bool goPrev = x < edgeDx;
            bool goNext = x >= rc.dx - edgeDx;
            if (r2l) {
                goPrev = x >= rc.dx - edgeDx;
                goNext = x < edgeDx;
            }
            if (goPrev) {
                tab->ctrl->GoToPrevPage();
                ReadAloudOnUserViewChanged(win);
                return;
            }
            if (goNext) {
                tab->ctrl->GoToNextPage();
                ReadAloudOnUserViewChanged(win);
                return;
            }
        }
    }

    if (PM_ENABLED == win->presentation) {
        /* in presentation mode, change pages on left/right-clicks */
        if ((key & MK_SHIFT)) {
            tab->ctrl->GoToPrevPage();
        } else {
            tab->ctrl->GoToNextPage();
        }
        ReadAloudOnUserViewChanged(win);
        return;
    }
}

bool gDisableInteractiveInverseSearch = false;

static void OnMouseLeftButtonDblClk(MainWindow* win, int x, int y, WPARAM key) {
    // lf("Left button clicked on %d %d", x, y);
    if (AnnotationPlacementOnLeftDblClk(win, Point{x, y})) {
        return;
    }
    // a double-click on free text edits its text where it sits on the page
    if (StartFreeTextInPlaceEditAt(win, Point{x, y})) {
        return;
    }
    auto isLeft = bit::IsMaskSet(key, (WPARAM)MK_LBUTTON);
    if (gSettings->enableTeXEnhancements && !gDisableInteractiveInverseSearch && isLeft) {
        bool dontSelect = OnInverseSearch(win, x, y);
        if (dontSelect) {
            return;
        }
    }

    DisplayModel* dm = win->AsFixed();
    // note: before 3.5 double-click used to turn 2 pages
    // OnMouseLeftButtonDown(win, x, y, key);
    Point mousePos = Point(x, y);
    bool isOverText = dm->IsOverText(mousePos);

    if (isLeft && (win->presentation || win->isFullScreen)) {
        // in fullscreen we allow to exit by tapping in upper right corner
        constexpr int kCornerSize = 64;
        Rect r = HwndClientRect(win->hwndCanvas);
        if (!isOverText && (x >= (r.dx - kCornerSize)) && (y < kCornerSize)) {
            ExitFullScreen(win);
            return;
        }
    }

    int elementPageNo = -1;
    IPageElement* pageEl = dm->GetElementAtPos(mousePos, &elementPageNo);
    if (isOverText) {
        int pageNo = dm->GetPageNoByPoint(mousePos);
        if (win->ctrl->ValidPageNo(pageNo)) {
            PointF pt = dm->CvtFromScreen(mousePos, pageNo);
            dm->textSelection->SelectWordAt(pageNo, pt.x, pt.y);
            UpdateTextSelection(win, false);
            // remember this double-click so a quick 3rd click nearby is detected
            // as a triple-click (line selection, issue #694)
            gLastWordSelectTime = (DWORD)GetMessageTime();
            gLastWordSelectPos = Point(x, y);
            // keep the gesture active so dragging after the double-click extends
            // the selection a word at a time (issue #4761). dragStartPending is
            // cleared so that releasing without dragging keeps the whole word.
            win->selectingByWord = true;
            win->showSelection = true;
            win->selectionRect = Rect(x, y, 0, 0);
            win->mouseAction = MouseAction::SelectingText;
            win->dragStartPending = false;
            SetCapture(win->hwndCanvas);
            SetTimer(win->hwndCanvas, kSelectSmoothScrollTimerID, kSelectSmoothScrollDelayInMs, nullptr);
            ScheduleRepaint(win, 0);
        }
        return;
    }

    if (!pageEl) {
        return;
    }
    if (pageEl->Is(kindPageElementDest)) {
        if (gSettings->disableLinks) {
            return;
        }
        // speed up navigation in a file where navigation links are in a fixed position
        OnMouseLeftButtonDown(win, x, y, key);
    } else if (pageEl->Is(kindPageElementImage)) {
        // select an image that could be copied to the clipboard
        Rect rc = dm->CvtToScreen(elementPageNo, pageEl->GetRect());

        DeleteOldSelectionInfo(win, true);
        win->CurrentTab()->selectionOnPage = SelectionOnPage::FromRectangle(dm, rc);
        win->showSelection = win->CurrentTab()->selectionOnPage != nullptr;
        ScheduleRepaint(win, 0);
    }
}

static void OnMouseMiddleButtonDown(MainWindow* win, int x, int y, WPARAM /*key*/) {
    // Handle message by recording placement then moving document as mouse moves.

    if (win->mouseAction == MouseAction::None) {
        win->mouseAction = MouseAction::Scrolling;

        win->dragStartPending = true;
        // record current mouse position, the farther the mouse is moved
        // from this position, the faster we scroll the document
        win->dragStart = Point(x, y);
        SetCanvasCursor(win, IDC_SIZEALL);
    } else if (win->mouseAction == MouseAction::Scrolling) {
        win->mouseAction = MouseAction::None;
    }
}

// Begin (or, if already active, end) middle-click-style auto-scroll anchored at
// canvas client point (x, y). Shared by the middle mouse button and by
// CmdStartAutoScroll, so auto-scroll can be triggered without a middle button.
static void ToggleAutoScroll(MainWindow* win, int x, int y) {
    win->xScrollAccum = 0;
    win->yScrollAccum = 0;
    SetTimer(win->hwndCanvas, kAutoScrollTimerID, USER_TIMER_MINIMUM, nullptr);
    OnMouseMiddleButtonDown(win, x, y, 0);
}

// CmdStartAutoScroll entry point: start/stop auto-scroll anchored at the current
// cursor position, exactly as a middle-click there would. Move the cursor away
// from that point to scroll; invoke again (or middle-click, or change focus) to stop.
void StartAutoScrollAtCursor(MainWindow* win) {
    if (!win || !win->AsFixed()) {
        return;
    }
    Point pt = HwndGetCursorPos(win->hwndCanvas);
    ToggleAutoScroll(win, pt.x, pt.y);
}

static void OnMouseMiddleButtonUp(MainWindow* win, WPARAM /*key*/) {
    // a middle-click that started auto-scrolling and then moved is a drag, and
    // releasing it ends the scroll; releasing without moving leaves auto-scroll
    // latched on, which is what dragStartPending still being set means
    if (win->mouseAction == MouseAction::Scrolling && !win->dragStartPending) {
        win->mouseAction = MouseAction::None;
        SetCanvasCursor(win, IDC_ARROW);
    }
}

static void OnMouseRightButtonDown(MainWindow* win, int x, int y) {
    // lf("Right button clicked on %d %d", x, y);
    if (AnnotationPlacementOnRightDown(win)) {
        return;
    }
    if (MouseAction::Scrolling == win->mouseAction) {
        win->mouseAction = MouseAction::None;
    } else if (win->mouseAction != MouseAction::None) {
        return;
    }
    ReportIf(!win->AsFixed());

    HwndSetFocus(win->hwndFrame);

    win->dragStartPending = true;
    win->dragStart = Point(x, y);

    StartMouseDrag(win, x, y, true);
}

static void OnMouseRightButtonUp(MainWindow* win, int x, int y, WPARAM key) {
    ReportIf(!win->AsFixed());
    // A held finger is delivered as a right-click, which would open the context
    // menu on top of the word the hold just selected (issue #538)
    if (win->touchSuppressContextMenu) {
        win->touchSuppressContextMenu = false;
        logf("touch: swallowing the right-click that followed the long press\n");
        if (IsRightDragging(win)) {
            StopMouseDrag(win, x, y, true);
            win->mouseAction = MouseAction::None;
        }
        return;
    }
    if (!IsRightDragging(win)) {
        return;
    }

    int isDragXOrY = IsDragDistance(x, win->dragStart.x, y, win->dragStart.y);
    bool didDragMouse = !win->dragStartPending || isDragXOrY;
    StopMouseDrag(win, x, y, !didDragMouse);

    win->mouseAction = MouseAction::None;

    if (didDragMouse) {
        /* pass */;
    } else if (PM_ENABLED == win->presentation) {
        if ((key & MK_CONTROL)) {
            OnWindowContextMenu(win, x, y);
        } else if ((key & MK_SHIFT)) {
            win->ctrl->GoToNextPage();
        } else {
            win->ctrl->GoToPrevPage();
        }
        ReadAloudOnUserViewChanged(win);
    }
    /* return from white/black screens in presentation mode */
    else if (PM_BLACK_SCREEN == win->presentation || PM_WHITE_SCREEN == win->presentation) {
        win->ChangePresentationMode(PM_ENABLED);
    } else {
        OnWindowContextMenu(win, x, y);
    }
}

static void OnMouseRightButtonDblClick(MainWindow* win, int x, int y, WPARAM key) {
    if (win->presentation && !(key & ~MK_RBUTTON)) {
        // in presentation mode, right clicks turn the page,
        // make two quick right clicks (AKA one double-click) turn two pages
        OnMouseRightButtonDown(win, x, y);
        return;
    }
}

#ifdef DRAW_PAGE_SHADOWS
constexpr int kBorderSize = 1;
constexpr int kShadowOffset = 4;
constexpr COLORREF kColPageShadow = RGB(0x40, 0x40, 0x40);
constexpr COLORREF kColPageFrame = RGB(0x88, 0x88, 0x88);
static void PaintPageFrameAndShadow(HDC hdc, Rect& bounds, Rect& pageRect, bool presentation, Color /*bgCol*/) {
    // Frame info
    Rect frame = bounds;
    frame.Inflate(kBorderSize, kBorderSize);

    // Shadow info
    Rect shadow = frame;
    shadow.Offset(kShadowOffset, kShadowOffset);
    if (frame.x < 0) {
        // the left of the page isn't visible, so start the shadow at the left
        int diff = std::min(-pageRect.x, kShadowOffset);
        shadow.x -= diff;
        shadow.dx += diff;
    }
    if (frame.y < 0) {
        // the top of the page isn't visible, so start the shadow at the top
        int diff = std::min(-pageRect.y, kShadowOffset);
        shadow.y -= diff;
        shadow.dy += diff;
    }

    // Draw shadow
    if (!presentation) {
        AutoDeleteBrush brush = CreateSolidBrush(kColPageShadow);
        HdcFillRect(hdc, shadow, brush);
    }

    // Draw frame
    ScopedGdiObj<HPEN> pe(CreatePen(PS_SOLID, 1, presentation ? TRANSPARENT : kColPageFrame));
    AutoDeleteBrush brush = CreateSolidBrush(gCurrentTheme->window.backgroundColor);
    SelectObject(hdc, pe);
    SelectObject(hdc, brush);
    Rectangle(hdc, frame.x, frame.y, frame.x + frame.dx, frame.y + frame.dy);
}
#else
static void PaintPageFrameAndShadow(HDC hdc, Rect& bounds, Rect& /*pageRect*/, bool /*presentation*/, Color bgCol) {
    AutoDeletePen pen(CreatePen(PS_NULL, 0, 0));
    AutoDeleteBrush brush(CreateSolidBrush(bgCol));
    ScopedSelectPen restorePen(hdc, pen);
    ScopedSelectObject restoreBrush(hdc, brush);
    Rectangle(hdc, bounds.x, bounds.y, bounds.x + bounds.dx + 1, bounds.y + bounds.dy + 1);
}
#endif

// CmdToggleImages. Like showLinks this is a debug aid (both live in the debug
// menu, so both are debug / pre-release only), and like it the outlines are
// only drawn, never saved - see CmdToggleImages in FrameOnCommand
static bool gShowImages = false;

// CmdToggleImages: outline images the way showLinks outlines links (debug aid)
bool ShowImageOutlines() {
    return gShowImages;
}

void ToggleShowImageOutlines() {
    gShowImages = !gShowImages;
}

// CmdToggleTransparencyGrid: Acrobat-style checkerboard under the page so
// transparent PDFs (white art on a hole) are visible. Session-only, not saved.
static bool gShowTransparencyGrid = false;

bool ShowTransparencyGrid() {
    return gShowTransparencyGrid;
}

void ToggleTransparencyGrid() {
    gShowTransparencyGrid = !gShowTransparencyGrid;
}

// CmdTogglePageGrid: dotted graph paper on the page (issue #4398).
// Session-only, not saved. Overlay on top of the page bitmap (normal PDFs
// are opaque, so this cannot sit under glyphs). Fixed-page documents only.
static bool gShowPageGrid = false;

bool ShowPageGrid() {
    return gShowPageGrid;
}

void TogglePageGrid() {
    gShowPageGrid = !gShowPageGrid;
}

void SetShowPageGrid(bool on) {
    gShowPageGrid = on;
}

void RedrawPageGridWindows() {
    for (MainWindow* w : gWindows) {
        if (w) {
            w->RedrawAll(true);
        }
    }
}

/* debug code to visualize links and images (can block while rendering) */
static void DebugOutlinePageElements(DisplayModel* dm, HDC hdc, bool images) {
    Rect viewPortRect(Point(), dm->GetViewPort().Size());
    Kind elementKind = images ? kindPageElementImage : kindPageElementDest;

    // blue for links, green for images, so both can be on at once
    Color col = images ? MkRgb(0x00, 0xa0, 0x00) : kColBlue;
    ScopedSelectObject autoPen(hdc, CreatePen(PS_SOLID, 1, col), true);

    for (int pageNo = dm->PageCount(); pageNo >= 1; --pageNo) {
        PageInfo* pi = dm->GetPageInfo(pageNo);
        if (!pi || !pi->isShown || 0.0 == pi->visibleRatio) {
            continue;
        }

        // don't block the paint (and the whole UI) behind a busy render thread
        // just to outline links; they get drawn on the next repaint
        Vec<IPageElement*> els;
        dm->GetEngine()->TryGetElements(pageNo, &els);

        for (auto& el : els) {
            if (!el->Is(elementKind)) {
                continue;
            }
            Rect rect = dm->CvtToScreen(pageNo, el->GetRect());
            Rect isect = viewPortRect.Intersect(rect);
            if (!isect.IsEmpty()) {
                isect.Inflate(2, 2);
                HdcDrawRect(hdc, isect);
            }
        }
    }
}

static void DebugShowLinks(DisplayModel* dm, HDC hdc) {
    if (gShowImages) {
        DebugOutlinePageElements(dm, hdc, true);
    }
    if (!gSettings->showLinks) {
        return;
    }
    DebugOutlinePageElements(dm, hdc, false);
}

// CmdDebugShowFitContentArea. Like gShowImages, a debug-only visualization that
// is drawn but never saved to settings
static bool gShowFitContentArea = false;

void ToggleShowFitContentArea() {
    gShowFitContentArea = !gShowFitContentArea;
}

bool ShowFitContentArea() {
    return gShowFitContentArea;
}

/* debug code to visualize the area "Fit Content" zoom would fit to, without
   actually switching the zoom. When no content box is detected we outline the
   whole page, which is the same fallback PageSizeAfterRotation() uses */
static Color ColorForPdfPageBox(PdfPageBoxKind kind) {
    switch (kind) {
        case PdfPageBoxKind::Media:
            return MkRgb(0x20, 0x20, 0x20);
        case PdfPageBoxKind::Crop:
            return MkRgb(0xc0, 0x20, 0x20);
        case PdfPageBoxKind::Bleed:
            return MkRgb(0x20, 0x40, 0xc0);
        case PdfPageBoxKind::Trim:
            return MkRgb(0x10, 0x90, 0x20);
        case PdfPageBoxKind::Art:
            return MkRgb(0xc0, 0x80, 0x00);
    }
    return kColBlack;
}

// Place the label so coincident boxes (crop == media, etc.) stay readable.
static Point PdfPageBoxLabelPos(const Rect& r, PdfPageBoxKind kind) {
    constexpr int kPad = 3;
    switch (kind) {
        case PdfPageBoxKind::Media:
            return Point(r.x + kPad, r.y + kPad);
        case PdfPageBoxKind::Crop:
            return Point(r.x + r.dx - kPad, r.y + kPad);
        case PdfPageBoxKind::Bleed:
            return Point(r.x + kPad, r.y + r.dy - kPad);
        case PdfPageBoxKind::Trim:
            return Point(r.x + r.dx - kPad, r.y + r.dy - kPad);
        case PdfPageBoxKind::Art:
            return Point(r.x + (r.dx / 2), r.y + kPad);
    }
    return r.TL();
}

static uint PdfPageBoxLabelFormat(PdfPageBoxKind kind) {
    switch (kind) {
        case PdfPageBoxKind::Crop:
            return DT_RIGHT | DT_TOP | DT_SINGLELINE;
        case PdfPageBoxKind::Trim:
            return DT_RIGHT | DT_BOTTOM | DT_SINGLELINE;
        case PdfPageBoxKind::Bleed:
            return DT_LEFT | DT_BOTTOM | DT_SINGLELINE;
        case PdfPageBoxKind::Art:
            return DT_CENTER | DT_TOP | DT_SINGLELINE;
        case PdfPageBoxKind::Media:
        default:
            return DT_LEFT | DT_TOP | DT_SINGLELINE;
    }
}

static Rect PdfPageBoxLabelRect(const Rect& box, PdfPageBoxKind kind) {
    Point p = PdfPageBoxLabelPos(box, kind);
    constexpr int kW = 44;
    constexpr int kH = 14;
    switch (kind) {
        case PdfPageBoxKind::Crop:
            return Rect(p.x - kW, p.y, kW, kH);
        case PdfPageBoxKind::Trim:
            // Bottom-right, above p (like Bleed). Drawing below the box clips
            // "trim" off the last/only page (#6005).
            return Rect(p.x - kW, p.y - kH, kW, kH);
        case PdfPageBoxKind::Bleed:
            return Rect(p.x, p.y - kH, kW, kH);
        case PdfPageBoxKind::Art:
            return Rect(p.x - (kW / 2), p.y, kW, kH);
        case PdfPageBoxKind::Media:
        default:
            return Rect(p.x, p.y, kW, kH);
    }
}

// Keep the label fully inside bounds so a box flush with the viewport
// does not clip the last few letters.
static Rect ClampRectTo(const Rect& r, const Rect& bounds) {
    Rect o = r;
    if (o.dx > bounds.dx) {
        o.dx = bounds.dx;
    }
    if (o.dy > bounds.dy) {
        o.dy = bounds.dy;
    }
    if (o.x < bounds.x) {
        o.x = bounds.x;
    }
    if (o.y < bounds.y) {
        o.y = bounds.y;
    }
    if (o.Right() > bounds.Right()) {
        o.x = bounds.Right() - o.dx;
    }
    if (o.Bottom() > bounds.Bottom()) {
        o.y = bounds.Bottom() - o.dy;
    }
    return o;
}

// Drawing limits. Appearance (spacing, color, style) lives in
// FixedPageUI.PageGrid; showing the overlay is session-only.
constexpr int kPageGridMinMinorPx = 6;
constexpr int kPageGridMinMajorPx = 4;
constexpr int kPageGridMaxDots = 8000;
constexpr int kPageGridMaxLines = 800;
constexpr Color kPageGridDefaultColor = MkRgb(128, 128, 255);

enum class PageGridStyleKind {
    Dots,
    Dotted,
    Solid
};

struct PageGridDraw {
    float widthPt = 72.f;
    float heightPt = 72.f;
    int subdiv = 4;
    float offsetXPt = 0;
    float offsetYPt = 0;
    Color color = kPageGridDefaultColor;
    PageGridStyleKind style = PageGridStyleKind::Dots;
};

static PageGridDraw GetPageGridDraw() {
    PageGridDraw d;
    if (!gSettings) {
        return d;
    }
    PageGrid& pg = gSettings->fixedPageUI.pageGrid;
    if (pg.width > 0) {
        d.widthPt = pg.width;
    }
    if (pg.height > 0) {
        d.heightPt = pg.height;
    }
    if (pg.subdivisions > 0) {
        d.subdiv = pg.subdivisions;
    }
    d.offsetXPt = pg.offsetX;
    d.offsetYPt = pg.offsetY;
    ParseColor(pg.color);
    if (pg.color.parsedOk && !IsSpecialColor(pg.color.col)) {
        d.color = pg.color.col;
    }
    if (str::EqI(pg.style, StrL("dotted"))) {
        d.style = PageGridStyleKind::Dotted;
    } else if (str::EqI(pg.style, StrL("solid"))) {
        d.style = PageGridStyleKind::Solid;
    }
    d.widthPt = limitValue(d.widthPt, 1.f, 720.f);
    d.heightPt = limitValue(d.heightPt, 1.f, 720.f);
    d.subdiv = limitValue(d.subdiv, 1, 32);
    d.offsetXPt = limitValue(d.offsetXPt, -720.f, 720.f);
    d.offsetYPt = limitValue(d.offsetYPt, -720.f, 720.f);
    return d;
}

static float PageGridAlignDown(float v, float origin, float step) {
    if (step <= 0) {
        return origin;
    }
    return origin + floorf((v - origin) / step) * step;
}

static bool PageGridIsMajor(float v, float origin, float minorPt, int subdiv) {
    if (minorPt <= 0.f || subdiv < 1) {
        return true;
    }
    int i = (int)floorf(((v - origin) / minorPt) + 0.5f);
    if (i < 0) {
        i = -i;
    }
    return (i % subdiv) == 0;
}

static int PageGridScreenDist(Point a, Point b) {
    int dx = a.x - b.x;
    int dy = a.y - b.y;
    if (dx < 0) {
        dx = -dx;
    }
    if (dy < 0) {
        dy = -dy;
    }
    return dx > dy ? dx : dy;
}

static void PageGridStroke(HDC hdc, DisplayModel* dm, int pageNo, PointF a, PointF b) {
    Point sa = dm->CvtToScreen(pageNo, a);
    Point sb = dm->CvtToScreen(pageNo, b);
    MoveToEx(hdc, sa.x, sa.y, nullptr);
    LineTo(hdc, sb.x, sb.y);
}

// Graph-paper overlay in page space, clipped to the visible page.
// Style "dots": 1px minor / 3px major at intersections.
// Style "dotted" / "solid": H/V lines (major heavier). Skips comics.
static void PaintPageGrid(DisplayModel* dm, HDC hdc) {
    EngineBase* engine = dm->GetEngine();
    if (!engine || engine->IsImageCollection()) {
        return;
    }
    PageGridDraw g = GetPageGridDraw();
    float minorX = g.widthPt / (float)g.subdiv;
    float minorY = g.heightPt / (float)g.subdiv;
    if (minorX <= 0.f || minorY <= 0.f) {
        return;
    }
    Rect viewPort(Point(), dm->GetViewPort().Size());
    HBRUSH br = CreateSolidBrush(g.color);
    if (!br) {
        return;
    }
    HPEN penMinor = CreatePen(g.style == PageGridStyleKind::Solid ? PS_SOLID : PS_DOT, 1, g.color);
    int majorWidth = g.style == PageGridStyleKind::Solid ? 2 : 1;
    HPEN penMajor = CreatePen(PS_SOLID, majorWidth, g.color);

    for (int pageNo = 1; pageNo <= dm->PageCount(); pageNo++) {
        PageInfo* pi = dm->GetPageInfo(pageNo);
        if (!pi || !pi->isShown || 0.0 == pi->visibleRatio) {
            continue;
        }
        Rect bounds = pi->pageOnScreen.Intersect(viewPort);
        if (bounds.IsEmpty()) {
            continue;
        }
        RectF box = engine->PageMediabox(pageNo);
        if (box.IsEmpty()) {
            continue;
        }
        RectF vis = dm->CvtFromScreen(bounds, pageNo).Intersect(box);
        if (vis.IsEmpty()) {
            continue;
        }

        float ox = box.x + g.offsetXPt;
        float oy = box.y + g.offsetYPt;
        Point p0 = dm->CvtToScreen(pageNo, PointF(ox, oy));
        Point px = dm->CvtToScreen(pageNo, PointF(ox + minorX, oy));
        Point py = dm->CvtToScreen(pageNo, PointF(ox, oy + minorY));
        int cell = std::min(PageGridScreenDist(p0, px), PageGridScreenDist(p0, py));
        if (cell * g.subdiv < kPageGridMinMajorPx) {
            continue;
        }
        bool drawMinor = cell >= kPageGridMinMinorPx;
        float stepX = drawMinor ? minorX : g.widthPt;
        float stepY = drawMinor ? minorY : g.heightPt;

        float x0 = PageGridAlignDown(vis.x, ox, stepX);
        float y0 = PageGridAlignDown(vis.y, oy, stepY);
        float x1 = vis.x + vis.dx;
        float y1 = vis.y + vis.dy;
        int nx = (int)((x1 - x0) / stepX) + 2;
        int ny = (int)((y1 - y0) / stepY) + 2;
        if (drawMinor) {
            bool tooMany = g.style == PageGridStyleKind::Dots ? (nx > 0 && ny > 0 && nx * ny > kPageGridMaxDots)
                                                              : (nx + ny > kPageGridMaxLines);
            if (tooMany) {
                drawMinor = false;
                stepX = g.widthPt;
                stepY = g.heightPt;
                x0 = PageGridAlignDown(vis.x, ox, stepX);
                y0 = PageGridAlignDown(vis.y, oy, stepY);
                nx = (int)((x1 - x0) / stepX) + 2;
                ny = (int)((y1 - y0) / stepY) + 2;
            }
        }

        int saved = SaveDC(hdc);
        IntersectClipRect(hdc, bounds.x, bounds.y, bounds.x + bounds.dx, bounds.y + bounds.dy);
        SetBkMode(hdc, TRANSPARENT);
        if (g.style == PageGridStyleKind::Dots) {
            for (float y = y0; y <= y1 + 0.01f; y += stepY) {
                bool yMajor = !drawMinor || PageGridIsMajor(y, oy, minorY, g.subdiv);
                for (float x = x0; x <= x1 + 0.01f; x += stepX) {
                    Point pt = dm->CvtToScreen(pageNo, PointF(x, y));
                    bool major = !drawMinor || (yMajor && PageGridIsMajor(x, ox, minorX, g.subdiv));
                    int sz = major ? 3 : 1;
                    int o = sz / 2;
                    RECT rc = {pt.x - o, pt.y - o, pt.x - o + sz, pt.y - o + sz};
                    FillRect(hdc, &rc, br);
                }
            }
        } else {
            for (float x = x0; x <= x1 + 0.01f; x += stepX) {
                bool major = !drawMinor || PageGridIsMajor(x, ox, minorX, g.subdiv);
                SelectObject(hdc, major ? penMajor : penMinor);
                PageGridStroke(hdc, dm, pageNo, PointF(x, vis.y), PointF(x, y1));
            }
            for (float y = y0; y <= y1 + 0.01f; y += stepY) {
                bool major = !drawMinor || PageGridIsMajor(y, oy, minorY, g.subdiv);
                SelectObject(hdc, major ? penMajor : penMinor);
                PageGridStroke(hdc, dm, pageNo, PointF(vis.x, y), PointF(x1, y));
            }
        }
        RestoreDC(hdc, saved);
    }
    DeleteObject(penMinor);
    DeleteObject(penMajor);
    DeleteObject(br);
}

// CmdTogglePageBoxes: outline the PDF boxes this page actually declares
// (MediaBox / CropBox / BleedBox / TrimBox / ArtBox) and label them.
static void PaintPdfPageBoxes(DisplayModel* dm, HDC hdc) {
    EngineBase* engine = dm->GetEngine();
    if (!engine) {
        return;
    }
    Rect viewPortRect(Point(), dm->GetViewPort().Size());
    PlatformFont* font = GetDefaultGuiFont(true, false);
    HFONT hfont = font ? font->GetHFont() : nullptr;

    Vec<PdfPageBox> boxes;
    for (int pageNo = 1; pageNo <= dm->PageCount(); pageNo++) {
        PageInfo* pi = dm->GetPageInfo(pageNo);
        if (!pi || !pi->isShown || 0.0 == pi->visibleRatio) {
            continue;
        }
        engine->GetPdfPageBoxes(pageNo, boxes);
        int n = len(boxes);
        for (int i = 0; i < n; i++) {
            const PdfPageBox& box = boxes[i];
            Rect rect = dm->CvtToScreen(pageNo, box.rect);
            // coincident boxes (crop == media) would paint on top of each
            // other; inset later kinds so every outline stays visible
            rect.Inflate(-(int)box.kind, -(int)box.kind);
            if (rect.dx < 2 || rect.dy < 2) {
                continue;
            }
            Rect vis = viewPortRect.Intersect(rect);
            if (vis.IsEmpty()) {
                continue;
            }
            Color col = ColorForPdfPageBox(box.kind);
            ScopedSelectObject autoPen(hdc, CreatePen(PS_SOLID, 1, col), true);
            HdcDrawRect(hdc, rect);

            Str name = Str(PdfPageBoxName(box.kind));
            // MediaBox often extends past CropBox (the drawn page); pin the
            // label to the on-screen part so it isn't clipped off-canvas
            Rect labelRc = ClampRectTo(PdfPageBoxLabelRect(vis, box.kind), viewPortRect);
            SetBkColor(hdc, RGB(255, 255, 255));
            SetBkMode(hdc, OPAQUE);
            SetTextColor(hdc, col);
            HdcDrawText(hdc, name, labelRc, PdfPageBoxLabelFormat(box.kind), hfont);
        }
    }
}

static void DebugShowFitContentArea(DisplayModel* dm, HDC hdc) {
    if (!gShowFitContentArea) {
        return;
    }
    Rect viewPortRect(Point(), dm->GetViewPort().Size());
    ScopedSelectObject autoPen(hdc, CreatePen(PS_SOLID, 2, kColRed), true);

    for (int pageNo = dm->PageCount(); pageNo >= 1; --pageNo) {
        PageInfo* pi = dm->GetPageInfo(pageNo);
        if (!pi || !pi->isShown || 0.0 == pi->visibleRatio) {
            continue;
        }
        // same cache DisplayModel uses for kZoomFitContent, so we don't
        // re-analyze the page on every repaint
        if (pi->contentBox.IsEmpty()) {
            pi->contentBox = dm->GetEngine()->PageContentBox(pageNo);
        }
        RectF box = pi->contentBox;
        if (box.IsEmpty()) {
            box = dm->PageMediaBox(pageNo);
        }
        Rect rect = dm->CvtToScreen(pageNo, box);
        if (!viewPortRect.Intersect(rect).IsEmpty()) {
            HdcDrawRect(hdc, rect);
        }
    }
}

// cf. https://web.archive.org/web/20140201011540/http://forums.fofou.org/sumatrapdf/topic?id=3183580&comments=15
static void GetGradientColor(Color a, Color b, float perc, TRIVERTEX* tv) {
    u8 ar, ag, ab;
    u8 br, bg, bb;
    UnpackColor(a, ar, ag, ab);
    UnpackColor(b, br, bg, bb);

    tv->Red = (COLOR16)(((float)ar + (perc * (float)(br - ar))) * 256);
    tv->Green = (COLOR16)(((float)ag + (perc * (float)(bg - ag))) * 256);
    tv->Blue = (COLOR16)(((float)ab + (perc * (float)(bb - ab))) * 256);
}

// Draw a border around selected annotation
static bool gDrawOldStyleAnnotationRect = false;

static void PaintHoveredAnnotationMark(MainWindow* win, HDC hdc, DisplayModel* dm) {
    WindowTab* tab = win ? win->CurrentTab() : nullptr;
    Annotation* annot = win ? win->annotationUnderCursor : nullptr;
    if (!win || !win->pdfAnnotationsToolbarEnabled || !tab || !annot || annot == tab->selectedAnnotation) {
        return;
    }
    int pageNo = annot->pageNo;
    if (!dm->PageVisible(pageNo)) {
        return;
    }
    Rect rect = dm->CvtToScreen(pageNo, GetRect(annot));
    rect.Inflate(4, 4);
    Gdiplus::Graphics gs(hdc);
    Gdiplus::Color blue(200, 0, 80, 200);
    Gdiplus::Pen pen(blue, 2);
    pen.SetDashStyle(Gdiplus::DashStyleDot);
    gs.DrawRectangle(&pen, rect.x, rect.y, rect.dx, rect.dy);
}

NO_INLINE static void PaintCurrentEditAnnotationMark(WindowTab* tab, HDC hdc, DisplayModel* dm) {
    if (!tab) {
        return;
    }
    Annotation* annot = tab->selectedAnnotation;
    if (!annot) {
        return;
    }
    int pageNo = annot->pageNo;
    if (!dm->PageVisible(pageNo)) {
        // CvtToScreen() might not work if page is not visible because
        // it might not have zoom etc. calculated yet
        return;
    }
    bool canResize = AnnotationCanBeResized(annot->type);
    MainWindow* win = tab->win;
    bool draggingLine = win && win->annotationBeingResized && IsLineEndpointHandle((ResizeHandle)win->resizeHandle) &&
                        annot->type == AnnotationType::Line;
    bool draggingVertices = win && win->annotationBeingResized && IsVertexHandle((ResizeHandle)win->resizeHandle) &&
                            IsPolyVertexType(annot->type);
    // an outline-only resize hasn't touched the annotation yet, so the marker
    // and its handles come from the preview rect instead
    bool draggingOutline =
        win && win->annotationBeingResized && win->annotationResizeOutlineOnly && win->annotationBeingDragged == annot;

    Rect rect = dm->CvtToScreen(pageNo, draggingOutline ? win->annotationResizePreviewRect : GetRect(annot));
    if (!tab->didScrollToSelectedAnnotation) {
        dm->ScrollScreenToRect(pageNo, rect);
        tab->didScrollToSelectedAnnotation = true;
    }
    rect.Inflate(4, 4);

    Gdiplus::Graphics gs(hdc);

    Gdiplus::SolidBrush handleBrush(Gdiplus::Color(255, 255, 255, 255)); // White
    Gdiplus::Pen handlePen(Gdiplus::Color(255, 0, 0, 0), 1);             // Black
    int hs = 6;                                                          // handle size
    int hh = hs / 2;                                                     // half handle

    auto drawHandle = [&](int x, int y) {
        gs.FillRectangle(&handleBrush, x, y, hs, hs);
        gs.DrawRectangle(&handlePen, x, y, hs, hs);
    };

    auto drawVertexHandles = [&](const Vec<PointF>& pts) {
        for (int i = 0; i < len(pts); i++) {
            Point p = dm->CvtToScreen(pageNo, pts[i]);
            drawHandle(p.x - hh, p.y - hh);
        }
    };

    auto drawVertexPath = [&](const Vec<PointF>& pts, bool closed) {
        int n = len(pts);
        if (n == 0) {
            return;
        }
        gs.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        Gdiplus::Color blue(255, 0, 80, 200);
        Gdiplus::Pen pen(blue, (Gdiplus::REAL)std::max(DpiScale(2), 1));
        Point prev = dm->CvtToScreen(pageNo, pts[0]);
        for (int i = 1; i < n; i++) {
            Point cur = dm->CvtToScreen(pageNo, pts[i]);
            gs.DrawLine(&pen, prev.x, prev.y, cur.x, cur.y);
            prev = cur;
        }
        if (closed && n >= 2) {
            Point first = dm->CvtToScreen(pageNo, pts[0]);
            gs.DrawLine(&pen, prev.x, prev.y, first.x, first.y);
        }
    };

    if (draggingLine) {
        Point startPt = dm->CvtToScreen(pageNo, win->annotationLinePreviewStart);
        Point endPt = dm->CvtToScreen(pageNo, win->annotationLinePreviewEnd);
        gs.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        Gdiplus::Color blue(255, 0, 80, 200);
        Gdiplus::Pen pen(blue, (Gdiplus::REAL)std::max(DpiScale(2), 1));
        gs.DrawLine(&pen, startPt.x, startPt.y, endPt.x, endPt.y);
        drawHandle(startPt.x - hh, startPt.y - hh);
        drawHandle(endPt.x - hh, endPt.y - hh);
        return;
    }

    if (draggingVertices) {
        bool closed = annot->type == AnnotationType::Polygon;
        drawVertexPath(win->annotationVertexPreview, closed);
        drawVertexHandles(win->annotationVertexPreview);
        return;
    }

    if (gDrawOldStyleAnnotationRect) {
        Gdiplus::Color col = GdiRgbFromColor(0xff3333); // blue
        Gdiplus::Color colHatch2((Gdiplus::ARGB)Gdiplus::Color::Yellow);
        Gdiplus::HatchBrush br(Gdiplus::HatchStyleCross, colHatch2, col);
        Gdiplus::Pen pen(&br, 4);
        gs.DrawRectangle(&pen, rect.x, rect.y, rect.dx, rect.dy);
    } else {
        Gdiplus::Color blue(255, 0, 80, 200);
        Gdiplus::Pen pen(blue, 2);
        pen.SetDashStyle(Gdiplus::DashStyleDot);
        gs.DrawRectangle(&pen, rect.x, rect.y, rect.dx, rect.dy);
    }

    if (!canResize) {
        return;
    }

    PointF lineStart, lineEnd;
    if (annot->type == AnnotationType::Line && GetLinePoints(annot, lineStart, lineEnd)) {
        Point startPt = dm->CvtToScreen(pageNo, lineStart);
        Point endPt = dm->CvtToScreen(pageNo, lineEnd);
        drawHandle(startPt.x - hh, startPt.y - hh);
        drawHandle(endPt.x - hh, endPt.y - hh);
        return;
    }

    if (IsPolyVertexType(annot->type)) {
        drawVertexHandles(GetVertices(annot));
        return;
    }

    if (annot->type == AnnotationType::Redact && len(GetQuadPointsAsRect(annot)) > 0) {
        return;
    }

    int left = rect.x - hh;
    int midX = rect.x + (rect.dx / 2) - hh;
    int right = rect.x + rect.dx - hh;
    int top = rect.y - hh;
    int midY = rect.y + (rect.dy / 2) - hh;
    int bottom = rect.y + rect.dy - hh;

    // corners
    drawHandle(left, top);
    drawHandle(right, top);
    drawHandle(right, bottom);
    drawHandle(left, bottom);
    // edges
    drawHandle(midX, top);
    drawHandle(right, midY);
    drawHandle(midX, bottom);
    drawHandle(left, midY);
}

static bool DrawDocument(MainWindow* win, HDC hdc, Rect rcArea) {
    ReportIf(!win->AsFixed());
    if (!win->AsFixed()) {
        return false;
    }
    DisplayModel* dm = win->AsFixed();
    // logf("DrawDocument RenderCache:\n");

    auto* engine = dm->GetEngine();
    bool isImage = engine->IsImageCollection();
    // draw comic books and single images on a black background
    // (without frame and shadow)
    bool paintOnBlackWithoutShadow = win->presentation || isImage;
    bool isEbook = engine->kind == kindEngineMupdf && !str::EqI(engine->defaultExt, StrL(".pdf"));
    bool isPdf = engine->kind == kindEngineMupdf && str::EqI(engine->defaultExt, StrL(".pdf"));
    Color colDocBg;
    Color colDocTxt = ThemeDocumentColors(colDocBg);
    if (isImage) {
        colDocBg = 0x0;
        colDocTxt = 0xffffff;
        // allow ComicBookUI/ImageUI WindowBgCol to override the default black
        ParsedColor* bgOverride = nullptr;
        if (engine->kind == kindEngineComicBooks) {
            bgOverride = GetPrefsColor(gSettings->comicBookUI.windowBgCol);
        } else {
            bgOverride = GetPrefsColor(gSettings->imageUI.windowBgCol);
        }
        if (bgOverride->parsedOk) {
            colDocBg = bgOverride->col;
        }
    } else if (isEbook) {
        ParsedColor* bgOverride = GetPrefsColor(gSettings->eBookUI.windowBgCol);
        if (bgOverride->parsedOk) {
            colDocBg = bgOverride->col;
        }
    } else if (isPdf) {
        ParsedColor* bgOverride = GetPrefsColor(gSettings->fixedPageUI.windowBgCol);
        if (bgOverride->parsedOk) {
            colDocBg = bgOverride->col;
        }
    }

    // per-document background color from FileState overrides everything
    WindowTab* curTab = win->CurrentTab();
    if (curTab && curTab->bgColorCheckered) {
        colDocBg = kColorUnset;
    } else if (curTab && curTab->bgColor != kColorUnset) {
        colDocBg = curTab->bgColor;
    }

    // placeholder painted where a page's bitmap isn't rendered yet; normally
    // the page render background so an incoming page doesn't flash a
    // different color
    Color colPlaceholder;
    ThemeDocumentColors(colPlaceholder);
    // until the first page of this tab has been painted, use the theme's
    // window background instead: e.g. restoring a session into a maximized
    // window can take a while to render the first page and a white
    // placeholder (+ white doc background) flashes in dark themes
    bool firstDocPaint = curTab && !curTab->everPaintedPage && !isImage;
    if (firstDocPaint) {
        colDocBg = ThemeMainWindowBackgroundColor();
        colDocTxt = ThemeWindowTextColor();
        colPlaceholder = colDocBg;
    }

    bool shouldPaint = false;
    auto* gcols = gSettings->fixedPageUI.gradientColors;
    auto nGCols = len(*gcols);
    auto paintBgOrCheckerboard = [&](Color col, Rect rc) {
        if (col == kColorUnset) {
            HdcPaintCheckerboard(hdc, rc.x, rc.y, rc.dx, rc.dy);
        } else {
            AutoDeleteBrush brush = CreateSolidBrush(col);
            HdcFillRect(hdc, rc, brush);
        }
    };

    if (paintOnBlackWithoutShadow || colDocBg == kColorUnset) {
        paintBgOrCheckerboard(colDocBg, rcArea);
    } else if (0 == nGCols) {
        AutoDeleteBrush brush = CreateSolidBrush(colDocBg);
        HdcFillRect(hdc, rcArea, brush);
    } else {
        Color colors[3];
        colors[0] = ParseColor((*gcols)[0], kColWhite);
        if (nGCols == 1) {
            colors[1] = colors[2] = colors[0];
        } else if (nGCols == 2) {
            colors[2] = ParseColor((*gcols)[1], kColWhite);
            colors[1] =
                MkRgb((GetRed(colors[0]) + GetRed(colors[2])) / 2, (GetGreen(colors[0]) + GetGreen(colors[2])) / 2,
                      (GetBlue(colors[0]) + GetBlue(colors[2])) / 2);
        } else {
            colors[1] = ParseColor((*gcols)[1], kColWhite);
            colors[2] = ParseColor((*gcols)[2], kColWhite);
        }
        Size size = dm->GetCanvasSize();
        float percTop = 1.0F * (float)dm->GetViewPort().y / (float)size.dy;
        float percBot = 1.0F * (float)dm->GetViewPort().BR().y / (float)size.dy;
        if (!IsContinuous(dm->GetDisplayMode())) {
            percTop += (float)dm->CurrentPageNo() - 1;
            percTop /= (float)dm->PageCount();
            percBot += (float)dm->CurrentPageNo() - 1;
            percBot /= (float)dm->PageCount();
        }
        Size vp = dm->GetViewPort().Size();
        TRIVERTEX tv[4] = {{0, 0}, {vp.dx, vp.dy / 2}, {0, vp.dy / 2}, {vp.dx, vp.dy}};
        GRADIENT_RECT gr[2] = {{0, 1}, {2, 3}};

        Color col0 = colors[0];
        Color col1 = colors[1];
        Color col2 = colors[2];
        if (percTop < 0.5F) {
            GetGradientColor(col0, col1, 2 * percTop, &tv[0]);
        } else {
            GetGradientColor(col1, col2, 2 * (percTop - 0.5F), &tv[0]);
        }

        if (percBot < 0.5f) {
            GetGradientColor(col0, col1, 2 * percBot, &tv[3]);
        } else {
            GetGradientColor(col1, col2, 2 * (percBot - 0.5F), &tv[3]);
        }

        bool needCenter = percTop < 0.5F && percBot > 0.5F;
        if (needCenter) {
            GetGradientColor(col1, col1, 0, &tv[1]);
            GetGradientColor(col1, col1, 0, &tv[2]);
            tv[1].y = tv[2].y = (LONG)((0.5F - percTop) / (percBot - percTop) * (float)vp.dy);
        } else {
            gr[0].LowerRight = 3;
        }
        // TODO: disable for less than about two screen heights?
        ULONG nMesh = 1;
        if (needCenter) {
            nMesh = 2;
        }
        GradientFill(hdc, tv, dimof(tv), gr, nMesh, GRADIENT_FILL_RECT_V);
    }

    bool rendering = false;
    Rect screen(Point(), dm->GetViewPort().Size());

    bool isRtl = IsUIRtl();
    for (int pageNo = 1; pageNo <= dm->PageCount(); ++pageNo) {
        PageInfo* pi = dm->GetPageInfo(pageNo);
        if (!pi || 0.0F == pi->visibleRatio) {
            continue;
        }
        ReportIf(!pi->isShown);
        if (!pi->isShown) {
            continue;
        }

        Rect bounds = pi->pageOnScreen.Intersect(screen);
        // don't paint the frame background for images
        if (!dm->GetEngine()->IsImageCollection()) {
            if (ShowTransparencyGrid()) {
                HdcPaintCheckerboard(hdc, bounds.x, bounds.y, bounds.dx, bounds.dy);
            } else {
                Rect r = pi->pageOnScreen;
                auto presMode = win->presentation;
                PaintPageFrameAndShadow(hdc, bounds, r, presMode, colPlaceholder);
            }
        }

        // check if this page is known to have failed rendering
        if (pi->failedToRender) {
            shouldPaint = true;
            PlatformFont* fontRightTxt = HdcCreateSimpleFont(hdc, StrL("MS Shell Dlg"), 14);
            HGDIOBJ hPrevFont = SelectObject(hdc, fontRightTxt->GetHFont());
            auto prevCol = SetTextColor(hdc, colDocTxt);
            TempStr msg = fmt(_TRA("Couldn't render page %d").s, pageNo);
            HdcDrawCenteredText(hdc, bounds, msg, isRtl);
            SetTextColor(hdc, prevCol);
            SelectObject(hdc, hPrevFont);
            continue;
        }

        bool renderOutOfDateCue = false;
        int renderDelay = gRenderCache->Paint(hdc, bounds, dm, pageNo, pi, &renderOutOfDateCue);
        if (renderDelay == 0) {
            shouldPaint = true;
            if (curTab) {
                curTab->everPaintedPage = true;
            }
        }
        if (renderDelay != 0) {
            PlatformFont* fontRightTxt = HdcCreateSimpleFont(hdc, StrL("MS Shell Dlg"), 14);
            HGDIOBJ hPrevFont = SelectObject(hdc, fontRightTxt->GetHFont());
            if (renderDelay != kRenderDelayFailed) {
                if (renderDelay < kRenderDelayShowNotif) {
                    ScheduleRepaint(win, kRenderDelayShowNotif - renderDelay);
                } else {
                    // the page is taking a while to render: tell the user about it.
                    // Also force a buffer flush (shouldPaint) so the notification is
                    // actually shown. Without this, in non-continuous mode the stale
                    // previous page would keep showing because no rendered page would
                    // set shouldPaint and gNoFlickerRender skips flushing the buffer.
                    shouldPaint = true;
                    SetTextColor(hdc, colDocTxt);
                    TempStr msg = fmt(_TRA("Rendering page %d...").s, pageNo);
                    HdcDrawCenteredText(hdc, bounds, msg, isRtl);
                }
                rendering = true;
            } else {
                shouldPaint = true;
                auto prevCol = SetTextColor(hdc, colDocTxt);
                TempStr msg = fmt(_TRA("Couldn't render page %d").s, pageNo);
                HdcDrawCenteredText(hdc, bounds, msg, isRtl);
                SetTextColor(hdc, prevCol);
            }
            SelectObject(hdc, hPrevFont);
            continue;
        }

        if (!renderOutOfDateCue) {
            continue;
        }

        HDC bmpDC = CreateCompatibleDC(hdc);
        if (!bmpDC) {
            continue;
        }
        SelectObject(bmpDC, gBitmapReloadingCue);
        int size = DpiScale(16);
        int cx = std::min(bounds.dx, 2 * size);
        int cy = std::min(bounds.dy, 2 * size);
        int x = bounds.x + bounds.dx - std::min((cx + size) / 2, cx);
        int y = bounds.y + std::max((cy - size) / 2, 0);
        int dxDest = std::min(cx, size);
        int dyDest = std::min(cy, size);
        StretchBlt(hdc, x, y, dxDest, dyDest, bmpDC, 0, 0, 16, 16, SRCCOPY);
        DeleteDC(bmpDC);
    }

    WindowTab* tab = win->CurrentTab();
    PaintAnnotationPlacement(win, hdc, dm);
    PaintHoveredAnnotationMark(win, hdc, dm);
    RepositionAnnotationHoverOverlay(win);
    RepositionAnnotationTextPopup(win);
    RepositionAnnotEditToolbar(win);
    RepositionFreeTextInPlaceEdit(win);
    PaintCurrentEditAnnotationMark(tab, hdc, dm);
    if (ShowPageGrid() && win->presentation == PM_DISABLED) {
        PaintPageGrid(dm, hdc);
    }
    GfxHdc gfx(hdc);

    // empty form fields, under find/selection so those stay visible
    PaintFormFieldHighlights(win, &gfx);

    // draw highlight rectangle around element under cursor during context menu
    if (win->contextMenuHighlightPageNo > 0 && dm->PageVisible(win->contextMenuHighlightPageNo)) {
        Rect rc = dm->CvtToScreen(win->contextMenuHighlightPageNo, win->contextMenuHighlightRect);
        Gdiplus::Graphics gs(hdc);
        Gdiplus::Color col(128, 0, 100, 255);
        Gdiplus::Pen pen(col, 2);
        gs.DrawRectangle(&pen, rc.x, rc.y, rc.dx, rc.dy);
    }

    // find-match highlighting and text selection are independent: paint both.
    // (when a find match is the current selection it's cleared in GoToFindMatch
    // so it isn't drawn twice; PaintAllFindMatches no-ops unless actively
    // searching). Using "else if" here hid the normal selection highlight
    // when all-match painting was on (issue #5737).
    PaintAllFindMatches(win, &gfx);
    if (win->showSelection) {
        PaintSelection(win, &gfx);
    }
    // keep the floating selection toolbar aligned with the selection while
    // scrolling/zooming; hides itself when the selection is gone or off-screen
    UpdateSelectionToolbarPosition(win);

    PaintReadAloudHighlight(win, &gfx);

    if (win->fwdSearchMark.show) {
        PaintForwardSearchMark(win, &gfx);
    }

    PaintKeyboardLinkTargets(win, &gfx);
    PaintKeyboardTextCaret(win, &gfx);

    if (!rendering) {
        DebugShowLinks(dm, hdc);
        DebugShowFitContentArea(dm, hdc);
        if (win->showPageBoxes) {
            PaintPdfPageBoxes(dm, hdc);
        }
    }
    return shouldPaint;
}

// Document keyboard focus lives on hwndFrame: AdvanceFocus() includes the frame
// as the "document" tab target, and canvas clicks call HwndSetFocus(hwndFrame)
// so arrow keys reach the frame. Optional focus ring is gated by
// ShowDocumentFocusIndicator (default off; #4644).
static bool CanvasShouldShowKeyboardFocus(MainWindow* win) {
    if (!win || !win->hwndFrame || !win->hwndCanvas) {
        return false;
    }
    if (!gSettings || !gSettings->showDocumentFocusIndicator) {
        return false;
    }
    if (win->presentation || win->isFullScreen) {
        return false;
    }
    return GetFocus() == win->hwndFrame;
}

// Draw a keyboard-focus ring on the canvas when document focus is on the frame
// (AdvanceFocus tab target). Call after painting the canvas client area (#4644).
void DrawCanvasKeyboardFocusIfNeeded(MainWindow* win, HDC hdc) {
    if (!hdc || !CanvasShouldShowKeyboardFocus(win)) {
        return;
    }
    Rect rc = HwndClientRect(win->hwndCanvas);
    // inset so the dashed rect is fully inside the client area
    rc.Inflate(-1, -1);
    if (!rc.IsEmpty()) {
        RECT nativeRect = ToRECT(rc);
        DrawFocusRect(hdc, &nativeRect);
    }
}

// Invalidate the canvas so the focus ring is shown/hidden after focus changes.
void InvalidateCanvasKeyboardFocus(MainWindow* win) {
    if (!win || !win->hwndCanvas) {
        return;
    }
    // Still invalidate when the setting is on so the ring appears/disappears
    // with focus; when off, skip the repaint cost.
    if (!gSettings || !gSettings->showDocumentFocusIndicator) {
        return;
    }
    HwndInvalidate(win->hwndCanvas);
}

static void OnPaintDocument(MainWindow* win) {
    auto t = TimeGet();
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(win->hwndCanvas, &ps);
    // page is never mirrored, even when the frame is RTL (issue #5326)
    SetLayout(hdc, 0);

    if (win->presentation == PM_BLACK_SCREEN) {
        HdcFillRect(hdc, ToRect(ps.rcPaint), GetStockBrush(BLACK_BRUSH));
    } else if (win->presentation == PM_WHITE_SCREEN) {
        HdcFillRect(hdc, ToRect(ps.rcPaint), GetStockBrush(WHITE_BRUSH));
    } else {
        bool shouldPaint = DrawDocument(win, win->buffer->GetDC(), ToRect(ps.rcPaint));
        // Flush when the focus ring is needed so DrawFocusRect is not XOR'd
        // on top of a stale frame that already had a ring.
        bool showFocus = CanvasShouldShowKeyboardFocus(win);
        if (!gNoFlickerRender || shouldPaint || showFocus) {
            win->buffer->Flush(hdc);
        }
    }
    DrawCanvasKeyboardFocusIfNeeded(win, hdc);

    EndPaint(win->hwndCanvas, &ps);
    win->ShowFrameRateDur(TimeSinceInMs(t));
}

static void SetTextOrArrorCursor(DisplayModel* dm, Point pt) {
    if (dm->IsOverText(pt)) {
        SetCursorCached(IDC_IBEAM);
    } else {
        SetCursorCached(IDC_ARROW);
    }
}

// TODO: this gets called way too often
static LRESULT OnSetCursorMouseNone(MainWindow* win, HWND hwnd) {
    DisplayModel* dm = win->AsFixed();
    Point pt = HwndGetCursorPos(hwnd);
    if (!dm || !GetCursor() || pt.IsEmpty()) {
        win->DeleteToolTip();
        return FALSE;
    }
    if (GetNotificationForGroup(win->hwndCanvas, kNotifCursorPos)) {
        SetCursorCached(IDC_CROSS);
        return TRUE;
    }

    WindowTab* tab = win->CurrentTab();
    Annotation* selected = tab->selectedAnnotation;

    // Check if hovering over resize handle of selected annotation
    if (selected && AnnotationCanBeResized(selected->type)) {
        ResizeHandle handle = GetResizeHandleAt(win, pt, selected);
        if (handle != ResizeHandle::None) {
            SetCursorCached(GetCursorForResizeHandle(handle));
            return TRUE;
        }
    }

    // PDF form fields: I-beam over text/choice, hand over checkbox/radio
    {
        WidgetCursorKind kind = GetWidgetCursorKind(dm->GetWidgetAtPos(pt));
        if (kind == WidgetCursorKind::Text) {
            SetCursorCached(IDC_IBEAM);
            return TRUE;
        }
        if (kind == WidgetCursorKind::Button) {
            SetCursorCached(IDC_HAND);
            return TRUE;
        }
    }

    Annotation* annot = dm->GetAnnotationAtPos(pt, selected);
    bool annotEditHover = annot && (win->pdfAnnotationsToolbarEnabled || selected);

    int pageNo = 0;
    IPageElement* pageEl = dm->GetElementAtPos(pt, &pageNo);
    if (pageEl && pageEl->Is(kindPageElementDest) && gSettings->disableLinks) {
        pageEl = nullptr;
    }
    if (!pageEl) {
        if (annotEditHover) {
            SetCursorCached(IDC_HAND);
        } else {
            SetTextOrArrorCursor(dm, pt);
        }
        win->DeleteToolTip();
        return TRUE;
    }
    // The Edit PDF hover card has the annotation's contents and metadata.
    // Do not put the old one-line comment tooltip on top of it.
    if (win->pdfAnnotationsToolbarEnabled && annot && pageEl->Is(kindPageElementComment)) {
        win->DeleteToolTip();
        SetCursorCached(IDC_HAND);
        return TRUE;
    }
    Str text = pageEl->GetValue();
    if (!dm->ValidPageNo(pageNo)) {
        Kind kind = pageEl->GetKind();
        logf("OnSetCursorMouseIdle: page element '%s' of kind '%s' on invalid page %d\n", Str(text), Str(kind), pageNo);
        ReportIf(true);
        return TRUE;
    }
    auto r = pageEl->GetRect();
    Rect rc = dm->CvtToScreen(pageNo, r);
    win->ShowToolTip(text, rc, true);

    // keep the hand cursor while editing an annotation, but still show the
    // comment tooltip (issue #5329)
    if (annotEditHover || pageEl->Is(kindPageElementDest)) {
        SetCursorCached(IDC_HAND);
    } else {
        SetTextOrArrorCursor(dm, pt);
    }
    return TRUE;
}

static LRESULT OnSetCursor(MainWindow* win, HWND hwnd) {
    ReportIf(win->hwndCanvas != hwnd);
    if (win->mouseAction != MouseAction::None) {
        win->DeleteToolTip();
    }

    if (AnnotationPlacementOnSetCursor(win)) {
        win->DeleteToolTip();
        return TRUE;
    }

    // the laser dot replaces every other cursor, and while pointing at the page
    // during a talk a link tooltip popping up is just in the way
    if (SetLaserPointerCursor(win)) {
        win->DeleteToolTip();
        return TRUE;
    }

    if (IsPlacingSignature(win)) {
        SetCursorCached(IDC_CROSS);
        win->DeleteToolTip();
        return TRUE;
    }

    switch (win->mouseAction) {
        case MouseAction::Dragging:
            if (win->annotationBeingResized) {
                SetCursorCached(GetCursorForResizeHandle((ResizeHandle)win->resizeHandle));
            } else {
                SetCursor(gCursorDrag);
            }
            return TRUE;
        case MouseAction::Scrolling:
            SetCursorCached(IDC_SIZEALL);
            return TRUE;
        case MouseAction::SelectingText:
            SetCursorCached(IDC_IBEAM);
            return TRUE;
        case MouseAction::Selecting:
            if (win->selectionDragEdge != SelectionDragEdge::None) {
                SetCursorCached(CursorIdForSelectionEdge(win->selectionDragEdge));
                return TRUE;
            }
            break;
        case MouseAction::None: {
            // resize / move cursors over an existing rectangular selection
            if (IsRectangularSelection(win)) {
                Point pt = HwndGetCursorPos(hwnd);
                SelectionDragEdge edge = HitTestRectangularSelection(win, pt.x, pt.y);
                if (edge != SelectionDragEdge::None) {
                    SetCursorCached(CursorIdForSelectionEdge(edge));
                    win->DeleteToolTip();
                    return TRUE;
                }
            }
            return OnSetCursorMouseNone(win, hwnd);
        }
    }
    return win->presentation ? TRUE : FALSE;
}

static float ScaleZoomBy(MainWindow* win, float factor) {
    auto zoomVirt = win->ctrl->GetZoomVirtual(true);
    return factor * zoomVirt;
}

static bool gWheelZoomRelative = true;

// we guess this is part of continous zoom action if WM_MOUSEWHEEL
static bool IsFirstWheelMsg(LARGE_INTEGER& lastTime) {
    auto currTime = TimeGet();
    auto elapsedMs = TimeDiffMs(lastTime, currTime);
    // 150 ms is a heuristic based on looking at logs
    if (elapsedMs < 150.0) {
        // logf("IsFirstWheelMsg: no, elapsed: %.f\n", (float)elapsedMs);
        lastTime = currTime;
        return false;
    }
    // logf("IsFirstWheelMsg: yes, elapsed: %.f\n", (float)elapsedMs);
    lastTime = currTime;
    return true;
}

// this does zooming via mouse wheel (with ctrl or right mouse buttone)
static void ZoomByMouseWheel(MainWindow* win, WPARAM wp) {
    // don't show the context menu when zooming with the right mouse-button down
    win->dragStartPending = false;
    // Stop smooth scroll when zooming — y offsets are no longer meaningful.
    StopSmoothScroll(win);

    short delta = GET_WHEEL_DELTA_WPARAM(wp);
    Point pt = HwndGetCursorPos(win->hwndCanvas);
    float newZoom;
    float factor = 0;
    // when ZoomIncrement is zero/negative, zoom must step through ZoomLevels (issue #5662)
    bool discreteWheelZoom = !gWheelZoomRelative || gSettings->zoomIncrement <= 0;
    if (discreteWheelZoom) {
        newZoom = win->ctrl->GetNextZoomStep(delta < 0 ? kZoomMin : kZoomMax);
        bool smartZoom = false; // Note: if true will prioritze selection
        SmartZoom(win, newZoom, &pt, smartZoom);
        return;
    }

    static LARGE_INTEGER lastWheelMsgTime{};
    static int accumDelta = 0;
    static float initialZoomVritual = 0;

    if (IsFirstWheelMsg(lastWheelMsgTime)) {
        initialZoomVritual = win->ctrl->GetZoomVirtual(true);
        accumDelta = 0;
    }

    // special case the value coming from pinch gensture on thinkpad touchpad
    // WHEEL_DELTA is 120, which is too fast, so we slow down zooming
    // 10 is heuristic
    if (delta == WHEEL_DELTA) {
        delta = 10;
    } else if (delta == -WHEEL_DELTA) {
        delta = -10;
    }

    accumDelta += delta;
    // calc zooming factor as centered around 1.f (1 is no change, > 1 is zoom in, < 1 is zoom out)
    // from delta values that are centered around 0
    bool negative = accumDelta < 0;

    factor = (float)std::abs(accumDelta) / 100.F;
    factor = 1.F + factor;
    if (negative) {
        factor = 1.F / factor;
    }
    newZoom = initialZoomVritual * factor;
    bool smartZoom = false; // Note: if true will prioritze selection
    SmartZoom(win, newZoom, &pt, smartZoom);

    // logf("delta: %d, accumDelta: %d, factor: %f, newZoom: %f\n", delta, accumDelta, factor, newZoom);
}

// Where the view is headed vertically. A smooth wheel scroll moves the view on
// a timer and OnVScroll deliberately doesn't hand the pending position to the
// scrollbar (the thumb would run ahead of the view), so GetScrollPos() still
// reads the old value right after WM_VSCROLL returns. Callers that ask "did
// that scroll do anything" have to compare the target instead, or they see no
// movement on every wheel event.
static int WheelScrollPosOrTarget(MainWindow* win) {
    if (gSettings->smoothScroll && win->scrollAnimActive) {
        return win->scrollTargetY;
    }
    return GetScrollPos(win->hwndCanvas, SB_VERT);
}

// Fit Content used to turn the mouse wheel into page flipping in every view mode.
// That is right when a whole page is on screen - there is nothing to scroll to -
// but in the continuous modes it silently replaced continuous scrolling with
// discrete page jumps, which is not something the user asked for by picking a
// zoom level. Flip to true to get the old behavior back for comparison; it is a
// plain global (not a setting) so it can also be toggled in the debugger.
bool gFitContentWheelFlipsPageInContinuous = false;

// whether a wheel notch in Fit Content should flip a whole page instead of
// scrolling. Always in the non-continuous modes; in continuous only if asked for
static bool FitContentWheelFlipsPage(DisplayModel* dm) {
    if (!dm || dm->GetZoomVirtual() != kZoomFitContent) {
        return false;
    }
    if (IsContinuous(dm->GetDisplayMode())) {
        return gFitContentWheelFlipsPageInContinuous;
    }
    return true;
}

static LRESULT CanvasOnMouseWheel(MainWindow* win, UINT msg, WPARAM wp, LPARAM lp) {
    // Scroll the ToC sidebar, if it's visible and the cursor is in it
    if (win->uiState.tocVisible && HwndIsCursorOverWindow(win->tocTreeView->hwnd) && !gWheelMsgRedirect) {
        // Note: hwndTocTree's window procedure doesn't always handle
        //       WM_MOUSEWHEEL and when it's bubbling up, we'd return
        //       here recursively - prevent that
        gWheelMsgRedirect = true;
        LRESULT res = SendMessageW(win->tocTreeView->hwnd, msg, wp, lp);
        gWheelMsgRedirect = false;
        return res;
    }

    bool wasInMouseWheelScroll = gInMouseWheelScroll;
    gInMouseWheelScroll = true;
    defer {
        gInMouseWheelScroll = wasInMouseWheelScroll;
    };

    // Mouse-wheel on the citation-hover popup (cursor still on the citation
    // link that opened it). Avoids moving the cursor onto the popup itself,
    // which would dismiss the hover.
    //   shift+wheel → scroll popup content (rolls over to prev/next page)
    //   ctrl+wheel  → zoom popup content
    //   plain wheel → falls through to scroll the main document, as if the
    //                 popup weren't there (modifier-less wheel scrolling a
    //                 document shouldn't get hijacked by the hover popup)
    if (win->refHover && win->refHover->hwndPopup && HwndIsVisible(win->refHover->hwndPopup)) {
        bool isCtrl = (LOWORD(wp) & MK_CONTROL) || IsCtrlPressed();
        bool isShift = (LOWORD(wp) & MK_SHIFT) || IsShiftPressed();
        if (isCtrl || isShift) {
            DisplayModel* dmHover = win->AsFixed();
            if (dmHover) {
                Point pt = HwndGetCursorPos(win->hwndCanvas);
                IPageElement* elHover = dmHover->GetElementAtPos(pt, nullptr);
                if (RefHoverIsInternalLink(elHover, dmHover)) {
                    short delta = GET_WHEEL_DELTA_WPARAM(wp);
                    if (isCtrl) {
                        RefHoverWheelZoom(win->refHover, dmHover->GetEngine(), delta);
                    } else {
                        RefHoverWheelScroll(win->refHover, dmHover->GetEngine(), delta);
                    }
                    return 0;
                }
            }
        }
    }

    // ignore wheel events while middle-button drag-scrolling is active
    if (MouseAction::Scrolling == win->mouseAction) {
        return 0;
    }

    DisplayModel* dm = win->AsFixed();

    // Note: not all mouse drivers correctly report the Ctrl / right-button
    // state on WM_MOUSEWHEEL. isCtrl is also set if this is a pinch gesture
    // from a touchpad (on a ThinkPad X1 at least).
    bool isCtrl = (LOWORD(wp) & MK_CONTROL) || IsCtrlPressed();
    bool isAlt = (LOWORD(wp) & MK_ALT) || IsAltPressed();
    bool isRightButton = (LOWORD(wp) & MK_RBUTTON) || IsRightButtonPressed();
    bool isZooming = isCtrl || isRightButton;
    if (isZooming) {
        ZoomByMouseWheel(win, wp);
        return 0;
    }

    bool hScroll = (LOWORD(wp) & MK_SHIFT) || IsShiftPressed();
    bool vScroll = !hScroll;
    bool isCont = !IsContinuous(win->ctrl->GetDisplayMode());

    // logf("delta: %d, accumDelta: %d, hscroll: %d, continuous: %d, gDeltaPerLine: %d\n", (int)delta,
    // win->wheelAccumDelta,
    //      (int)hScroll, (int)isCont, gDeltaPerLine);

    // Alt speeds up scrolling but also triggers showing menu
    // this will suppress next menu trigger to avoid accidental triggering of menu
    if (isAlt) {
        gSupressNextAltMenuTrigger = true;
    }

    short delta = GET_WHEEL_DELTA_WPARAM(wp);

    // run next-file-in-folder tip after any vertical wheel handling on this path
    struct VerticalScrollIntentGuard {
        MainWindow* win = nullptr;
        bool down = false;
        bool armed = false;
        ~VerticalScrollIntentGuard() {
            if (armed && win) {
                OnDocumentVerticalScrollIntent(win, down);
            }
        }
    } scrollIntent;
    if (vScroll) {
        scrollIntent.win = win;
        scrollIntent.down = delta < 0;
        scrollIntent.armed = true;
    }

    // MouseWheelTurnsPage: a wheel notch is a page turn, not a scroll, even when
    // the page is zoomed past the window. Pairs with RememberViewOffsetOnPageTurn
    // for reading zoomed-in pages (sheet music, scans with wide margins) without
    // the keyboard. Alt + wheel still scrolls, so the rest of the page is
    // reachable; Shift + wheel and Ctrl + wheel are unchanged
    if (vScroll && !isAlt && gSettings->mouseWheelTurnsPage) {
        win->wheelAccumDelta += delta;
        if (win->wheelAccumDelta >= WHEEL_DELTA) {
            win->ctrl->GoToPrevPage();
            win->wheelAccumDelta -= WHEEL_DELTA;
            ReadAloudOnUserViewChanged(win);
        } else if (win->wheelAccumDelta <= -WHEEL_DELTA) {
            win->ctrl->GoToNextPage();
            win->wheelAccumDelta += WHEEL_DELTA;
            ReadAloudOnUserViewChanged(win);
        }
        return 0;
    }

    // fit content: flip page on wheel, regardless of scrollbar state
    if (vScroll && dm && FitContentWheelFlipsPage(dm) && IsSingle(dm->GetDisplayMode())) {
        win->wheelAccumDelta += delta;
        if (win->wheelAccumDelta >= WHEEL_DELTA) {
            win->ctrl->GoToPrevPage();
            win->wheelAccumDelta -= WHEEL_DELTA;
            ReadAloudOnUserViewChanged(win);
        } else if (win->wheelAccumDelta <= -WHEEL_DELTA) {
            win->ctrl->GoToNextPage();
            win->wheelAccumDelta += WHEEL_DELTA;
            ReadAloudOnUserViewChanged(win);
        }
        return 0;
    }

    // Handle page-by-page navigation for non-continuous modes and SinglePage mode
    bool isSinglePageMode =
        gSettings->scrollbarInSinglePage && (win->ctrl->GetDisplayMode() == DisplayMode::SinglePage);

    // For SinglePage mode with content requiring scrolling, use continuous scrolling behavior
    if (isSinglePageMode && vScroll) {
        if (dm && dm->NeedVScroll()) {
            // Content is larger than viewport, use continuous scrolling
            // Fall through to the default scrolling behavior below
        } else {
            // Content fits in viewport, use page-by-page navigation
            int pageFlipDelta = WHEEL_DELTA; // One wheel click = one page
            win->wheelAccumDelta += delta;
            if (win->wheelAccumDelta >= pageFlipDelta) {
                win->ctrl->GoToPrevPage();
                win->wheelAccumDelta -= pageFlipDelta;
                ReadAloudOnUserViewChanged(win);
                return 0;
            }
            if (win->wheelAccumDelta <= -pageFlipDelta) {
                win->ctrl->GoToNextPage();
                win->wheelAccumDelta += pageFlipDelta;
                ReadAloudOnUserViewChanged(win);
                return 0;
            }
            return 0;
        }
    }

    // Handle page-by-page navigation for other non-continuous modes (but not SinglePage mode)
    if (vScroll && !isCont && !isSinglePageMode) {
        // in fit content we might show vert scrollbar but we want to flip the whole page on mouse wheel
        bool flipPage = FitContentWheelFlipsPage(dm);
        if (dm && !dm->NeedVScroll()) {
            // if page/pages fully fit in window, flip the whole page
            // logf("  flipping page because !dm->NeedVScroll()\n");
            flipPage = true;
        }
        // fit content/page: one wheel click = one page; otherwise 3 clicks
        int pageFlipDelta = flipPage ? WHEEL_DELTA : WHEEL_DELTA * 3;

        // int scrolLPos = GetScrollPos(win->hwndCanvas, SB_VERT);
        //  Note: pre 3.6 didn't care about horizontallScroll and kZoomFitPage was handled below
        if (flipPage) {
            win->wheelAccumDelta += delta;
            if (win->wheelAccumDelta >= pageFlipDelta) {
                win->ctrl->GoToPrevPage();
                win->wheelAccumDelta -= pageFlipDelta;
                ReadAloudOnUserViewChanged(win);
                return 0;
            }
            if (win->wheelAccumDelta <= -pageFlipDelta) {
                win->ctrl->GoToNextPage();
                win->wheelAccumDelta += pageFlipDelta;
                ReadAloudOnUserViewChanged(win);
                return 0;
            }
            return 0;
        }
    }

    if (gDeltaPerLine == 0) {
        return 0;
    }

    // For SinglePage mode with zoomed content, use continuous scrolling with page transitions
    if (isSinglePageMode && vScroll && dm) {
        if (dm->NeedVScroll()) {
            // Use continuous scrolling that handles page transitions at boundaries
            SCROLLINFO si{};
            si.cbSize = sizeof(si);
            si.fMask = SIF_PAGE;
            GetScrollInfo(win->hwndCanvas, hScroll ? SB_HORZ : SB_VERT, &si);
            int scrollBy = -MulDiv((int)si.nPage, delta * 30, WHEEL_DELTA);
            // on sensitive touchpads delta can be very small
            if (scrollBy == 0) {
                return 0;
            }
            if (hScroll) {
                dm->ScrollXBy(scrollBy);
            } else {
                dm->ScrollYBy(scrollBy, true);
            }
            // ScrollYBy updates the thumb via UpdateScrollbars; also force the
            // thin smart bar to appear for wheel-only reading (#5859).
            if (ScrollbarsUseOverlay()) {
                OverlayScrollbarNotifyScroll(hScroll ? win->overlayScrollH : win->overlayScrollV);
            }
            ReadAloudOnUserViewChanged(win);
            return 0;
        }
    }

    if (gDeltaPerLine < 0 && dm) {
        // scroll by (fraction of a) page
        SCROLLINFO si{};
        si.cbSize = sizeof(si);
        si.fMask = SIF_PAGE;
        GetScrollInfo(win->hwndCanvas, hScroll ? SB_HORZ : SB_VERT, &si);
        int scrollBy = -MulDiv((int)si.nPage, delta, WHEEL_DELTA);
        // on sensitive touchpads delta can be very small
        if (scrollBy == 0) {
            return 0;
        }
        if (hScroll) {
            dm->ScrollXBy(scrollBy);
        } else {
            dm->ScrollYBy(scrollBy, true);
        }
        if (ScrollbarsUseOverlay()) {
            OverlayScrollbarNotifyScroll(hScroll ? win->overlayScrollH : win->overlayScrollV);
        }
        ReadAloudOnUserViewChanged(win);
        return 0;
    }

    // alt while scrolling will scroll by half a page per tick
    // usefull for browsing long files
    if (isAlt) {
        wp = (delta > 0) ? kSbHalfPageUp : kSbHalfPageDown;
        SendMessageW(win->hwndCanvas, WM_VSCROLL, wp, 0);
        return 0;
    }

    if (gSettings->fastScrollOverScrollbar) {
        // scroll faster if the cursor is over the scroll bar
        if (HwndIsCursorOverWindow(win->hwndCanvas)) {
            Point pt = HwndGetCursorPos(win->hwndCanvas);
            if (pt.x > win->canvasRc.dx) {
                wp = (delta > 0) ? kSbHalfPageUp : kSbHalfPageDown;
                SendMessageW(win->hwndCanvas, WM_VSCROLL, wp, 0);
                return 0;
            }
        }
    }

    win->wheelAccumDelta += delta;
    int prevScrollPos = WheelScrollPosOrTarget(win);

    UINT scrollMsg = hScroll ? WM_HSCROLL : WM_VSCROLL;
    bool didScrollByLine = false;
    if (win->wheelAccumDelta < 0) {
        // SB_LINERIGHT == SB_LINEDOWN, but spell out which axis we mean
        WPARAM scrollWp = hScroll ? SB_LINERIGHT : SB_LINEDOWN; // NOLINT(bugprone-branch-clone)
        while (win->wheelAccumDelta <= -gDeltaPerLine) {
            SendMessageW(win->hwndCanvas, scrollMsg, scrollWp, 0);
            win->wheelAccumDelta += gDeltaPerLine;
            // logf("  line down\n");
            didScrollByLine = true;
        }
    } else {
        // SB_LINELEFT == SB_LINEUP, but spell out which axis we mean
        WPARAM scrollWp = hScroll ? SB_LINELEFT : SB_LINEUP; // NOLINT(bugprone-branch-clone)
        while (win->wheelAccumDelta >= gDeltaPerLine) {
            SendMessageW(win->hwndCanvas, scrollMsg, scrollWp, 0);
            win->wheelAccumDelta -= gDeltaPerLine;
            // logf("  line up\n");
            didScrollByLine = true;
        }
    }
    // in non-continuous mode flip page if necessary
    if (!vScroll || !isCont) {
        return 0;
    }
    if (!didScrollByLine) {
        // we haven't reached accumulated delta to scroll by line
        return 0;
    }

    int currScrollPos = WheelScrollPosOrTarget(win);
    bool didScroll = (currScrollPos != prevScrollPos);
    if (didScroll) {
        // we don't flip a page if we did scroll by line
        return 0;
    }
    // logf("  flip page: delta: %d, accumDelta: %d\n", (int)delta, (int)win->wheelAccumDelta);
    if (delta > 0) {
        win->ctrl->GoToPrevPage(true);
    } else if (dm) {
        // this page turn continues a scroll, so start the new page at its top
        // even with RememberViewOffsetOnPageTurn on - we're at the bottom of the
        // old page only because we scrolled there (see GoToNextPage(bool))
        dm->GoToNextPage(false);
    } else {
        win->ctrl->GoToNextPage();
    }
    ReadAloudOnUserViewChanged(win);

    return 0;
}

static LRESULT CanvasOnMouseHWheel(MainWindow* win, UINT msg, WPARAM wp, LPARAM lp) {
    // Scroll the ToC sidebar, if it's visible and the cursor is in it
    if (win->uiState.tocVisible && HwndIsCursorOverWindow(win->tocTreeView->hwnd) && !gWheelMsgRedirect) {
        // Note: hwndTocTree's window procedure doesn't always handle
        //       WM_MOUSEHWHEEL and when it's bubbling up, we'd return
        //       here recursively - prevent that
        gWheelMsgRedirect = true;
        LRESULT res = SendMessageW(win->tocTreeView->hwnd, msg, wp, lp);
        gWheelMsgRedirect = false;
        return res;
    }

    short delta = GET_WHEEL_DELTA_WPARAM(wp);
    win->wheelAccumDelta += delta;

    while (win->wheelAccumDelta >= gDeltaPerLine) {
        SendMessageW(win->hwndCanvas, WM_HSCROLL, SB_LINERIGHT, 0);
        win->wheelAccumDelta -= gDeltaPerLine;
    }
    while (win->wheelAccumDelta <= -gDeltaPerLine) {
        SendMessageW(win->hwndCanvas, WM_HSCROLL, SB_LINELEFT, 0);
        win->wheelAccumDelta += gDeltaPerLine;
    }

    return TRUE;
}

static u32 LowerU64(ULONGLONG v) {
    u32 res = (u32)v;
    return res;
}

__unused static Str GiFlagsToStr(DWORD flags) {
    switch (flags) {
        case 0:
            return StrL("");
        case GF_BEGIN:
            return StrL("GF_BEGIN");
        case GF_INERTIA:
            return StrL("GF_INERTIA");
        case GF_END:
            return StrL("GF_END");
        case GF_INERTIA | GF_END:
            return StrL("GF_INERTIA  | GF_END");
    }
    return StrL("unknown");
}

static LRESULT OnGesture(MainWindow* win, UINT msg, WPARAM wp, LPARAM lp) {
    DisplayModel* dm = win->AsFixed();
    if (!dm) {
        return DefWindowProc(win->hwndFrame, msg, wp, lp);
    }

    HGESTUREINFO hgi = (HGESTUREINFO)lp;
    GESTUREINFO gi{};
    gi.cbSize = sizeof(GESTUREINFO);
    TouchState& touchState = win->touchState;

    BOOL ok = GetGestureInfo(hgi, &gi);
    if (!ok) {
        CloseGestureInfoHandle(hgi);
        return 0;
    }

    switch (gi.dwID) {
        case GID_ZOOM: {
            auto curr = (float)LowerU64(gi.ullArguments);
            bool isBegin = gi.dwFlags & GF_BEGIN;
            if (!isBegin) {
                auto prev = touchState.zoomIntermediate;
                float factor = curr / prev;
                Point pt = HwndScreenToClient(win->hwndCanvas, Point(gi.ptsLocation.x, gi.ptsLocation.y));
                float newZoom = ScaleZoomBy(win, factor);
                SmartZoom(win, newZoom, &pt, false);
            }
            touchState.zoomIntermediate = curr;
            break;
        }

        case GID_PAN: {
            // A single finger on the glass is a pan as far as Windows is
            // concerned, whether it moves or not, so a long press and a
            // selection-handle drag both have to be recognized from this
            // stream (issue #538).
            Point cpt = HwndScreenToClient(win->hwndCanvas, Point(gi.ptsLocation.x, gi.ptsLocation.y));
            if (gi.dwFlags & GF_BEGIN) {
                touchState.pressRestPos = gi.ptsLocation;
                touchState.pressRestTime = (DWORD)GetMessageTime();
                touchState.longPressFired = false;
                touchState.panMovedOnce = false;
                // POINTERDOWN already reset these for this contact. Don't
                // clear panDidScroll here: the finger may have started
                // scrolling before the gesture engine sent GF_BEGIN.
                if (win->touchPointerId < 0) {
                    touchState.panDidScroll = false;
                    win->touchLongPressDone = false;
                }
                TouchSelHandle h = HitTestTouchSelHandle(win, cpt.x, cpt.y);
                if (h != TouchSelHandle::None) {
                    // this finger is here to move the selection, not the page
                    logf("touch: gesture grabbed %s handle at %d,%d\n", TouchSelHandleName(h), cpt.x, cpt.y);
                    win->touchSelDragging = h;
                }
            }
            if (win->touchSelDragging != TouchSelHandle::None) {
                if (!(gi.dwFlags & GF_BEGIN)) {
                    DragTouchSelHandle(win, cpt.x, cpt.y);
                }
                if (gi.dwFlags & GF_END) {
                    logf("touch: released %s handle\n", TouchSelHandleName(win->touchSelDragging));
                    win->touchSelDragging = TouchSelHandle::None;
                }
                break;
            }
            if (!(gi.dwFlags & GF_BEGIN)) {
                int slop = DpiScale(10);
                int dx = abs((int)gi.ptsLocation.x - (int)touchState.pressRestPos.x);
                int dy = abs((int)gi.ptsLocation.y - (int)touchState.pressRestPos.y);
                DWORD now = (DWORD)GetMessageTime();
                DWORD restMs = now - touchState.pressRestTime;
                if (dx > slop || dy > slop) {
                    // The first jump is the gesture engine deciding this is a
                    // pan; further movement is the user scrolling, and a pause
                    // after that must not select (issue #6006).
                    if (touchState.panMovedOnce) {
                        MarkTouchPanDidScroll(win);
                    }
                    touchState.panMovedOnce = true;
                    touchState.pressRestPos = gi.ptsLocation;
                    touchState.pressRestTime = now;
                    restMs = 0;
                }
                if (!touchState.panDidScroll && !touchState.longPressFired && !win->touchLongPressDone &&
                    restMs >= kTouchLongPressMs) {
                    touchState.longPressFired = true;
                    logf("touch: finger at rest for %dms at %d,%d -> long press\n", (int)restMs, cpt.x, cpt.y);
                    if (OnTouchLongPress(win, cpt.x, cpt.y)) {
                        // the page must not scroll out from under the selection
                        touchState.panStarted = false;
                        win->touchLongPressDone = true;
                        win->touchSuppressContextMenu = true;
                        break;
                    }
                }
            }
            // Flicking left or right changes the page,
            // panning moves the document in the scroll window
            if (gi.dwFlags == GF_BEGIN) {
                touchState.panStarted = true;
                touchState.panPos = gi.ptsLocation;
                touchState.panScrollOrigX = GetScrollPos(win->hwndCanvas, SB_HORZ);
                // logf("OnGesture: GID_PAN, GF_BEGIN, scrollX: %d\n", touchState.panScrollOrigX);
            } else if (touchState.panStarted) {
                int deltaX = touchState.panPos.x - gi.ptsLocation.x;
                int deltaY = touchState.panPos.y - gi.ptsLocation.y;
                touchState.panPos = gi.ptsLocation;

                // on left / right flick, go to next / prev page
                // unless we can pan/scroll the document
                bool isFlickX = (gi.dwFlags & GF_INERTIA) && (abs(deltaX) > abs(deltaY)) && (abs(deltaX) > 26);
                // logf("OnGesture: GID_PAN, flags: %d (%s), dx: %d, dy: %d, isFlick: %d\n", gi.dwFlags,
                // GiFlagsToStr(gi.dwFlags), deltaX, deltaY, (int)isFlickX);
                bool flipPage = false;
                if (!dm->NeedHScroll()) {
                    // if the page is fully visible
                    flipPage = true;
                    // logf("flipPage because !dm->NeedHScroll()");
                }
                if (deltaX > 0 && !dm->CanScrollRight()) {
                    flipPage = true;
                    // logf("flipPage because deltaX > 0 && !dm->CanScrollRight()");
                }
                if (deltaX < 0 && !dm->CanScrollLeft()) {
                    flipPage = true;
                    // logf("flipPage because deltaX < 0 && !dm->CanScrollLeft()");
                }

                if (isFlickX && flipPage) {
                    // deltaX < 0: finger moved left (content follows) → leftward spatial nav
                    // In manga (R2L) mode, left advances (issue #3964)
                    if (deltaX < 0) {
                        bool goNext = dm->GetDisplayR2L();
                        dm->GoToPageHorizontal(false);
                        // TODO: scroll to show the right-hand part
                        int x = dm->canvasSize.dx - dm->viewPort.dx;
                        // logf("x: %d\n");
                        dm->ScrollXTo(x);
                        OnDocumentVerticalScrollIntent(win, goNext);
                    } else if (deltaX > 0) {
                        bool goNext = !dm->GetDisplayR2L();
                        dm->GoToPageHorizontal(true);
                        dm->ScrollXTo(0);
                        OnDocumentVerticalScrollIntent(win, goNext);
                    }
                    ReadAloudOnUserViewChanged(win);
                    // When we switch pages prevent further pan movement
                    // caused by the inertia
                    touchState.panStarted = false;
                } else {
                    // pan / scroll
                    bool canScrollRightBefore = dm->CanScrollRight();
                    bool canScrollLeftBefore = dm->CanScrollLeft();
                    win->MoveDocBy(deltaX, deltaY);

                    // if pan to the rigth edge, we want to "sticK" to it
                    // and only flip page on the next flick motion
                    bool stopPanning = false;
                    if (canScrollRightBefore != dm->CanScrollRight()) {
                        stopPanning = true;
                        // logf("stopPanning because canScrollRightBefore != dm->CanScrollRight()\n");
                    }
                    if (canScrollLeftBefore != dm->CanScrollLeft()) {
                        stopPanning = true;
                        // logf("stopPanning because canScrollLeftBefore != dm->CanScrollLeft()\n");
                    }
                    if (stopPanning) {
                        touchState.panStarted = false;
                    }
                }
            }
            break;
        }

        case GID_ROTATE:
            // Rotate the PDF 90 degrees in one direction
            if (gi.dwFlags == GF_END && dm) {
                // This is in radians
                double rads = GID_ROTATE_ANGLE_FROM_ARGUMENT(LowerU64(gi.ullArguments));
                // The angle from the rotate is the opposite of the Sumatra rotate, thus the negative
                double degrees = -rads * 180 / M_PI;

                // Playing with the app, I found that I often didn't go quit a full 90 or 180
                // degrees. Allowing rotate without a full finger rotate seemed more natural.
                if (degrees < -120 || degrees > 120) {
                    dm->RotateBy(180);
                } else if (degrees < -45) {
                    dm->RotateBy(-90);
                } else if (degrees > 45) {
                    dm->RotateBy(90);
                }
            }
            break;

        case GID_TWOFINGERTAP:
            // Two-finger tap toggles fullscreen mode
            ToggleFullScreen(win);
            break;

        case GID_PRESSANDTAP:
            // Toggle between Fit Page, Fit Width and Fit Content (same as 'z')
            if (gi.dwFlags == GF_BEGIN) {
                win->ToggleZoom();
            }
            break;

        default:
            // A gesture was not recognized
            break;
    }

    CloseGestureInfoHandle(hgi);
    return 0;
}

// WM_POINTER message support for pen/stylus input
#ifndef WM_POINTERDOWN
constexpr UINT WM_POINTERDOWN = 0x0246;
constexpr UINT WM_POINTERUP = 0x0247;
constexpr UINT WM_POINTERUPDATE = 0x0245;
#endif

// POINTER_INPUT_TYPE values
constexpr int kSumatraPtTouch = 2;
constexpr int kSumatraPtPen = 3;

// pointer message flags (in HIWORD of wParam)
constexpr int kSumatraPointerMessageFlagInContact = 0x0004;
constexpr int kSumatraPointerMessageFlagFirstButton = 0x0010;

// dynamically loaded pointer API (Windows 8+)
typedef BOOL(WINAPI* Sig_GetPointerType)(UINT32 pointerId, DWORD* pointerType);
static Sig_GetPointerType DynGetPointerType = nullptr;
static bool triedLoadPointerApi = false;

static void EnsurePointerApiLoaded() {
    if (triedLoadPointerApi) {
        return;
    }
    triedLoadPointerApi = true;
    HMODULE h = GetModuleHandleW(L"user32.dll");
    if (h) {
        DynGetPointerType = (Sig_GetPointerType)GetProcAddress(h, "GetPointerType");
    }
}

// A finger's contact, watched through WM_POINTER* purely to time it. Whether
// the contact later turns into a pan gesture or into a synthesized click, the
// hold is recognized here (issue #538).
static void OnTouchPointer(MainWindow* win, HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    Point pt = HwndScreenToClient(hwnd, Point(GET_X_LPARAM(lp), GET_Y_LPARAM(lp)));
    DWORD now = (DWORD)GetTickCount64();
    win->touchLastActivityTime = now;
    if (msg == WM_POINTERDOWN) {
        win->touchDownPos = pt;
        win->touchDownTime = now;
        win->touchPointerId = LOWORD(wp);
        ResetTouchLongPress(win);
        logf("touch: pointer down at %d,%d\n", pt.x, pt.y);
        SetTimer(hwnd, kTouchLongPressTimerID, kTouchLongPressMs, nullptr);
        return;
    }
    if ((int)LOWORD(wp) != win->touchPointerId) {
        // a second finger: that's a gesture, not a press
        KillTimer(hwnd, kTouchLongPressTimerID);
        return;
    }
    if (msg == WM_POINTERUPDATE) {
        if (win->touchLongPressDone) {
            if (win->touchSelDragging != TouchSelHandle::None) {
                DragTouchSelHandle(win, pt.x, pt.y);
            }
            return;
        }
        int slop = DpiScale(10);
        if (abs(pt.x - win->touchDownPos.x) > slop || abs(pt.y - win->touchDownPos.y) > slop) {
            // the finger itself moved: this contact is a scroll, not a press
            MarkTouchPanDidScroll(win);
        }
        return;
    }
    if (msg == WM_POINTERUP) {
        logf("touch: pointer up at %d,%d after %dms, longPressDone=%d\n", pt.x, pt.y, (int)(now - win->touchDownTime),
             (int)win->touchLongPressDone);
        KillTimer(hwnd, kTouchLongPressTimerID);
        if (win->touchSelDragging != TouchSelHandle::None) {
            logf("touch: released %s handle\n", TouchSelHandleName(win->touchSelDragging));
            win->touchSelDragging = TouchSelHandle::None;
        }
        win->touchPointerId = -1;
    }
}

// handle WM_POINTER* messages for pen input by translating to mouse handlers
// pen input on Windows 8+ generates WM_POINTER* instead of WM_LBUTTON*
// and gesture configuration can prevent automatic promotion to mouse messages
static bool OnPointerMessage(MainWindow* win, HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    EnsurePointerApiLoaded();
    if (!DynGetPointerType) {
        return false;
    }

    UINT32 pointerId = LOWORD(wp);
    DWORD pointerType = 0;
    if (!DynGetPointerType(pointerId, &pointerType)) {
        return false;
    }
    if (pointerType == kSumatraPtTouch) {
        // Watch the raw contact but don't consume it: gestures (panning,
        // zooming) still have to come out of DefWindowProc. This is the only
        // place a finger's true timing shows up -- the gesture engine reports a
        // hold as a pan, and if it doesn't claim the contact at all the mouse
        // messages arrive as a single down+up pair at lift, both stamped with
        // the same time (issue #538).
        OnTouchPointer(win, hwnd, msg, wp, lp);
        return false;
    }
    // only handle pen input; let mouse and touch go through normal paths
    if (pointerType != kSumatraPtPen) {
        return false;
    }

    // WM_POINTER* lp contains screen coordinates
    Point pt = HwndScreenToClient(hwnd, Point(GET_X_LPARAM(lp), GET_Y_LPARAM(lp)));
    int x = pt.x;
    int y = pt.y;

    // pointer message flags are in HIWORD(wParam)
    WORD flags = HIWORD(wp);
    WPARAM mouseWp = 0;

    if (msg == WM_POINTERDOWN) {
        mouseWp = MK_LBUTTON;
        OnMouseLeftButtonDown(win, x, y, mouseWp);
        return true;
    }
    if (msg == WM_POINTERUPDATE) {
        bool inContact = (flags & kSumatraPointerMessageFlagInContact) != 0;
        if (inContact) {
            mouseWp = MK_LBUTTON;
        }
        OnMouseMove(win, x, y, mouseWp);
        return true;
    }
    if (msg == WM_POINTERUP) {
        OnMouseLeftButtonUp(win, x, y, mouseWp);
        return true;
    }
    return false;
}

static LRESULT WndProcCanvasFixedPageUI(MainWindow* win, HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    // DbgLogMsg("canvas:", hwnd, msg, wp, lp);

    if (!IsMainWindowValid(win)) {
        bool hwndValid = IsWindow(hwnd);
        logf("WndProcCanvasFixedPageUI: MainWindow win: 0x%p is no longer valid, msg: %d, hwnd valid: %d\n", win,
             (int)msg, (int)hwndValid);
        ReportIfFast(true);
        return 0;
    }

    int x = GET_X_LPARAM(lp);
    int y = GET_Y_LPARAM(lp);
    switch (msg) {
        case WM_PAINT:
            if (gRedrawLog) {
                RECT urc;
                GetUpdateRect(hwnd, &urc, FALSE);
                logf("redraw: WM_PAINT hwnd=0x%p (canvas-fixed) rc=(%d,%d,%d,%d)\n", hwnd, urc.left, urc.top, urc.right,
                     urc.bottom);
            }
            OnPaintDocument(win);
            return 0;

        case WM_MOUSEMOVE:
            OnMouseMove(win, x, y, wp);
            return 0;

        case WM_MOUSELEAVE:
            win->annotationUnderCursor = nullptr;
            HideAnnotationHoverOverlay(win);
            ScheduleRepaint(win, 0);
            RefHoverOnCanvasMouseLeave(win->refHover, win->hwndCanvas, gSettings->citationHoverDelay);
            return 0;

        case WM_LBUTTONDOWN:
            OnMouseLeftButtonDown(win, x, y, wp);
            return 0;

        case WM_LBUTTONUP:
            OnMouseLeftButtonUp(win, x, y, wp);
            return 0;

        case WM_LBUTTONDBLCLK:
            OnMouseLeftButtonDblClk(win, x, y, wp);
            return 0;

        case WM_MBUTTONDOWN:
            // drive auto-scroll from a high-frequency timer (with fractional-pixel
            // accumulation in the handler) so it's smooth, not choppy (issue #2693)
            // TODO: Create window that shows location of initial click for reference
            ToggleAutoScroll(win, x, y);
            return 0;

        case WM_MBUTTONUP:
            OnMouseMiddleButtonUp(win, wp);
            return 0;

        case WM_RBUTTONDOWN:
            OnMouseRightButtonDown(win, x, y);
            return 0;

        case WM_RBUTTONUP:
            OnMouseRightButtonUp(win, x, y, wp);
            return 0;

        case WM_RBUTTONDBLCLK:
            OnMouseRightButtonDblClick(win, x, y, wp);
            return 0;

        case WM_VSCROLL:
            OnVScroll(win, wp);
            return 0;

        case WM_HSCROLL:
            OnHScroll(win, wp);
            return 0;

        case WM_MOUSEWHEEL:
            return CanvasOnMouseWheel(win, msg, wp, lp);

        case WM_MOUSEHWHEEL:
            return CanvasOnMouseHWheel(win, msg, wp, lp);

        case WM_SETCURSOR:
            if (OnSetCursor(win, hwnd)) {
                return TRUE;
            }
            return DefWindowProc(hwnd, msg, wp, lp);

        case WM_CONTEXTMENU: {
            bool fromKeyboard = (x == -1 || y == -1);
            if (fromKeyboard) {
                // if invoked with a keyboard (shift-F10) use current mouse position
                Point pt = HwndGetCursorPos(hwnd);
                x = pt.x;
                y = pt.y;
            } else {
                // lParam is in screen coordinates for a real context-menu click
                Point pt = HwndScreenToClient(hwnd, Point(x, y));
                x = pt.x;
                y = pt.y;
            }
            // super defensive
            x = std::max(x, 0);
            y = std::max(y, 0);
            // Windows turns a held finger into a context menu of its own, which
            // arrives after we've already selected the word. Swallow that one
            // menu -- and only that one, so a later right-click still opens it
            // (issue #538).
            if (!fromKeyboard && win->touchSuppressContextMenu) {
                win->touchSuppressContextMenu = false;
                logf("touch: swallowing the context menu that followed the long press\n");
                return 0;
            }
            // A long press with a finger arrives as WM_CONTEXTMENU. Over text
            // that means "select this word" the way a phone browser does, so
            // the menu is suppressed there; everywhere else it still opens
            // (issue #538).
            // On a device where a hold does arrive as WM_CONTEXTMENU (a pen,
            // or touch with panning off) treat it as a long press too; the
            // gesture path above has usually handled it already.
            if (!fromKeyboard && !win->touchState.panDidScroll &&
                (win->lastInputWasTouch || IsMouseMessageFromTouch()) && OnTouchLongPress(win, x, y)) {
                return 0;
            }
            OnWindowContextMenu(win, x, y);
            return 0;
        }

        case WM_GESTURE:
            return OnGesture(win, msg, wp, lp);

        case WM_POINTERDOWN:
        case WM_POINTERUPDATE:
        case WM_POINTERUP:
            if (OnPointerMessage(win, hwnd, msg, wp, lp)) {
                return 0;
            }
            return DefWindowProc(hwnd, msg, wp, lp);

        case WM_NCPAINT:
            // Do not call ShowScrollBar here. Visibility is owned by
            // UpdateScrollbars; ShowScrollBar mid-NCPAINT re-enters uxtheme /
            // comctl32 while the themed native scrollbar is already painting
            // and can AV (null + 8) under dark themes (crash 8c1831c15000001).
            // Overlay/hidden modes strip WS_*SCROLL in WM_NCCALCSIZE instead.
            goto def;
    }
def:
    return DefWindowProc(hwnd, msg, wp, lp);
}

///// methods needed for ChmUI canvases (should be subclassed by HtmlHwnd) /////

// Wipe leftover pixels from a previous tab (the canvas HWND is shared). Without
// this, resize / WM_SETREDRAW flashes the last PDF/CBR paint around WebView2.
void FillCanvasThemeBackground(HWND hwndCanvas) {
    if (!hwndCanvas) {
        return;
    }
    HDC hdc = GetDC(hwndCanvas);
    HdcFillRect(hdc, HwndClientRect(hwndCanvas), ThemeMainWindowBackgroundColor());
    ReleaseDC(hwndCanvas, hdc);
}

static LRESULT WndProcCanvasChmUI(MainWindow* win, HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            HdcFillRect(hdc, ToRect(ps.rcPaint), ThemeMainWindowBackgroundColor());
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_SETCURSOR:
            win->DeleteToolTip();
            return DefWindowProc(hwnd, msg, wp, lp);

        default:
            return DefWindowProc(hwnd, msg, wp, lp);
    }
}

///// methods needed for FixedPageUI canvases with loading error /////

// "Error loading <name>" with the file name in bold, centered in r. The
// translation decides where the name sits in the sentence, so split its format
// string around the %s instead of assuming the name comes last. Falls back to
// one plain run for RTL, where laying runs out left to right would be wrong.
static void DrawLoadErrorLine(Gfx* gfx, Rect r, Str name, PlatformFont* font, Color textColor) {
    Str tmpl = _TRA("Error loading %s");
    int at = str::IndexOf(tmpl, StrL("%s"));
    if (at < 0 || IsUIRtl()) {
        u32 flags = gfxTextCenter | gfxTextVCenter | (IsUIRtl() ? gfxTextRtl : 0);
        gfx->DrawText(fmt(tmpl.s, name), r, flags, font, textColor);
        return;
    }
    Str prefix = Str(tmpl.s, at);
    Str suffix = Str(tmpl.s + at + 2, tmpl.len - at - 2);

    PlatformFont* boldFont = GetBoldPlatformFont(font);
    Size szName = gfx->MeasureText(name, boldFont);
    int dxPrefix = len(prefix) > 0 ? gfx->MeasureText(prefix, font).dx : 0;
    int dxSuffix = len(suffix) > 0 ? gfx->MeasureText(suffix, font).dx : 0;
    int x = r.x + ((r.dx - (dxPrefix + szName.dx + dxSuffix)) / 2);
    int y = r.y + ((r.dy - szName.dy) / 2);

    u32 flags = gfxTextSingleLine | gfxTextNoClip;
    if (len(prefix) > 0) {
        gfx->DrawTextAt(prefix, {x, y}, flags, font, textColor);
        x += dxPrefix;
    }
    gfx->DrawTextAt(name, {x, y}, flags, boldFont, textColor);
    x += szName.dx;
    if (len(suffix) > 0) {
        gfx->DrawTextAt(suffix, {x, y}, flags, font, textColor);
    }
}

// a red that stays readable on both a light and a dark canvas background
static Color LoadErrorTextColor() {
    if (IsLightColor(ThemeMainWindowBackgroundColor())) {
        return MkRgb(0xc6, 0x28, 0x28);
    }
    return MkRgb(0xef, 0x9a, 0x9a);
}

static void OnPaintDocumentStatus(MainWindow* win) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(win->hwndCanvas, &ps);
    SetLayout(hdc, 0);

    Gfx* gfx = GfxCreate(hdc);
    PlatformFont* fontRightTxt = GetUserGuiFont(StrL("MS Shell Dlg"), DpiScale(14));
    auto bgCol = ThemeMainWindowBackgroundColor();
    gfx->FillRect(ToRect(ps.rcPaint), bgCol);
    auto* tab = win->CurrentTab();
    Str filePath = tab->filePath;
    if (filePath) {
        TempStr msg;
        if (tab->loadState == WindowTab::LoadState::Loading || tab->loadState == WindowTab::LoadState::LoadedPending) {
            TempStr basename = path::GetBaseNameTemp(filePath);
            // prefer network-drive copy progress when available (set by
            // OnFileCopyProgress); the 1s loading timer only invalidates and
            // re-reads these fields so it does not replace the copy message
            if (tab->loadCopyBytesCopied >= 0) {
                TempStr copied = str::FormatSizeShortTemp(tab->loadCopyBytesCopied, nullptr);
                if (tab->loadCopyBytesTotal > 0) {
                    TempStr total = str::FormatSizeShortTemp(tab->loadCopyBytesTotal, nullptr);
                    msg = fmt(_TRA("Copying %s: %s / %s").s, basename, copied, total);
                } else {
                    msg = fmt(_TRA("Copying %s: %s").s, basename, copied);
                }
            } else {
                msg = fmt(_TRA("Loading %s ...").s, basename);
            }
            if (tab->loadStartedAt != 0) {
                u64 elapsedSecs = (GetTickCount64() - tab->loadStartedAt) / 1000;
                if (elapsedSecs > 0) {
                    TempStr elapsed;
                    u64 hours = elapsedSecs / 3600;
                    u64 minutes = (elapsedSecs % 3600) / 60;
                    u64 seconds = elapsedSecs % 60;
                    if (hours > 0) {
                        elapsed = fmt("%dh %dm %ds", hours, minutes, seconds);
                    } else if (minutes > 0) {
                        elapsed = fmt("%dm %ds", minutes, seconds);
                    } else {
                        elapsed = fmt("%ds", seconds);
                    }
                    msg = fmt("%s %s", msg, elapsed);
                }
            }
            u32 flags = gfxTextCenter | gfxTextVCenter | (IsUIRtl() ? gfxTextRtl : 0);
            gfx->DrawText(msg, HwndClientRect(win->hwndCanvas), flags, fontRightTxt, ThemeWindowTextColor());
        } else {
            // red, with the file name in bold and the reason (file gone, no
            // permission, locked by another program) on a second line: a bare
            // "Error loading foo.pdf" left the user with nothing to act on
            SetTextColor(hdc, LoadErrorTextColor());
            TempStr name = path::GetBaseNameTemp(filePath);
            Rect rc = HwndClientRect(win->hwndCanvas);
            Str reason = tab->loadErrorReason;
            Rect top = rc;
            if (len(reason) > 0) {
                int lineDy = PlatformFontMeasureText(fontRightTxt, name).dy;
                top.dy -= lineDy;
                Rect bottom = rc;
                bottom.y += lineDy;
                bottom.dy -= lineDy;
                u32 flags = gfxTextCenter | gfxTextVCenter | (IsUIRtl() ? gfxTextRtl : 0);
                gfx->DrawText(reason, bottom, flags, fontRightTxt, LoadErrorTextColor());
            }
            DrawLoadErrorLine(gfx, top, name, fontRightTxt, LoadErrorTextColor());
        }
    }
    delete gfx;
    DrawCanvasKeyboardFocusIfNeeded(win, hdc);

    EndPaint(win->hwndCanvas, &ps);
}

static LRESULT WndProcCanvasLoadError(MainWindow* win, HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_PAINT:
            if (gRedrawLog) {
                logf("redraw: WM_PAINT hwnd=0x%p (canvas-error)\n", hwnd);
            }
            OnPaintDocumentStatus(win);
            return 0;

        case WM_SETCURSOR:
            win->DeleteToolTip();
            return DefWindowProc(hwnd, msg, wp, lp);

        default:
            return DefWindowProc(hwnd, msg, wp, lp);
    }
}

///// methods needed for all types of canvas /////

struct RepaintTaskData {
    MainWindow* win = nullptr;
    int delayInMs = 0;
};

static void RepaintTask(RepaintTaskData* d) {
    AutoDelete delData(d);

    auto* win = d->win;
    if (!IsMainWindowValidAndNotClosing(win)) {
        return;
    }
    if (!d->delayInMs) {
        WndProcCanvas(win->hwndCanvas, WM_TIMER, kRepaintTimerID, 0);
    } else if (!win->delayedRepaintTimer) {
        win->delayedRepaintTimer = SetTimer(win->hwndCanvas, kRepaintTimerID, (uint)d->delayInMs, nullptr);
    }
}

void ScheduleRepaint(MainWindow* win, int delayInMs) {
    if (gRedrawLog) {
        logf("redraw: ScheduleRepaint delayMs=%d canvas=0x%p\n", delayInMs, win->hwndCanvas);
    }
    auto* data = new RepaintTaskData;
    data->win = win;
    data->delayInMs = delayInMs;
    auto fn = MkFunc0<RepaintTaskData>(RepaintTask, data);
    // even though RepaintAsync is mostly called from the UI thread,
    // we depend on the repaint message to happen asynchronously
    uitask::Post(fn, nullptr);
}

static void OnTimer(MainWindow* win, HWND hwnd, WPARAM timerId) {
    Point pt;

    if (!IsMainWindowValidAndNotClosing(win)) {
        return;
    }

    switch (timerId) {
        case kRepaintTimerID:
            win->delayedRepaintTimer = 0;
            KillTimer(hwnd, kRepaintTimerID);
            // Only the canvas needs a document repaint (scroll, page render,
            // selection, etc.). RedrawAllIncludingNonClient() repaints the
            // entire frame and all children, so the toolbar "Page:" label and
            // page-number edit flash on every scroll even when the page is
            // unchanged (very visible with tall comic pages).
            HwndInvalidate(hwnd);
            break;

        case kAnnotationResizeRerenderTimerID:
            CancelAnnotationResizeRerender(win);
            MainWindowRerender(win);
            break;

        case kTouchLongPressTimerID: {
            KillTimer(hwnd, kTouchLongPressTimerID);
            if (win->touchState.panDidScroll || win->touchState.longPressFired) {
                logf("touch: long press timer ignored (already panned or fired)\n");
                break;
            }
            Point dp = win->touchDownPos;
            logf("touch: long press timer fired at %d,%d, mouseAction=%d\n", dp.x, dp.y, (int)win->mouseAction);
            win->touchLongPressDone = true;
            win->touchState.longPressFired = true;
            if (OnTouchLongPress(win, dp.x, dp.y)) {
                // The press selects the word and stops there. Carrying straight
                // on into a drag looks like a good idea but the finger is never
                // quite still, so the selection creeps into the lines below
                // before it is even lifted; extending is what the handles are
                // for (issue #538).
                // Windows will raise its own press-and-hold menu next
                win->touchSuppressContextMenu = true;
            }
            break;
        }

        case kSelectSmoothScrollTimerID:
            if (MouseAction::Selecting == win->mouseAction || MouseAction::SelectingText == win->mouseAction) {
                pt = HwndGetCursorPos(win->hwndCanvas);
                if (NeedsSelectionEdgeAutoscroll(win, pt.x, pt.y)) {
                    OnMouseMove(win, pt.x, pt.y, MK_CONTROL);
                }
            } else {
                KillTimer(hwnd, kSelectSmoothScrollTimerID);
            }
            break;

        case kAutoScrollTimerID:
            if (MouseAction::Scrolling == win->mouseAction) {
                // xScrollSpeed/yScrollSpeed are in pixels per 20ms; this timer
                // fires far more often, so move only the matching fraction each
                // tick and carry the leftover sub-pixel amount, which keeps
                // middle-click auto-scroll smooth instead of stepping (issue #2693)
                constexpr float kBaseIntervalMs = 20.0f;
                float scale = (float)USER_TIMER_MINIMUM / kBaseIntervalMs;
                win->xScrollAccum += win->xScrollSpeed * scale;
                win->yScrollAccum += win->yScrollSpeed * scale;
                int dx = (int)win->xScrollAccum;
                int dy = (int)win->yScrollAccum;
                win->xScrollAccum -= (float)dx;
                win->yScrollAccum -= (float)dy;
                if (dx != 0 || dy != 0) {
                    win->MoveDocBy(dx, dy);
                }
            } else {
                KillTimer(hwnd, kAutoScrollTimerID);
                win->xScrollSpeed = 0;
                win->yScrollSpeed = 0;
                win->xScrollAccum = 0;
                win->yScrollAccum = 0;
            }
            break;

        case kHideCursorTimerID:
            // logf("got kHideCursorTimerID\n");
            KillTimer(hwnd, kHideCursorTimerID);
            // a laser pointer that disappears when you stop moving it would be
            // useless, so it opts out of the presentation-mode cursor hiding
            if (win->InPresentation() && !IsLaserPointerActive()) {
                // logf("hiding cursor because win->presentations\n");
                SetCursor((HCURSOR) nullptr);
            }
            break;

        case kRefHoverTimerID:
        case kRefHoverHideTimerID:
            RefHoverOnCanvasTimer(win->refHover, hwnd, win->AsFixed(), timerId);
            break;

        case kLinkFollowTimerID:
            // scrolling settled: relabel the links that are on screen now
            KillTimer(hwnd, kLinkFollowTimerID);
            KeyboardLinkFollowingRecompute(win);
            ScheduleRepaint(win, 0);
            break;

        case kTextSelectCaretTimerID:
            SelectTextWithKeyboardBlinkCaret(win);
            break;

        case kSelectionToolbarShowTimerID:
            // the selection settled: pop up the floating selection toolbar
            SelectionToolbarOnShowTimer(win);
            break;

        case kHideFwdSearchMarkTimerID:
            win->fwdSearchMark.hideStep++;
            if (1 == win->fwdSearchMark.hideStep) {
                SetTimer(hwnd, kHideFwdSearchMarkTimerID, kHideFwdSearchMarkDecayIntervalInMs, nullptr);
            } else if (win->fwdSearchMark.hideStep >= kHideFwdSearchMarkSteps) {
                KillTimer(hwnd, kHideFwdSearchMarkTimerID);
                win->fwdSearchMark.show = false;
                ScheduleRepaint(win, 0);
            } else {
                ScheduleRepaint(win, 0);
            }
            break;

        case kReadAloudHighlightTimerID:
            if (GetReadAloudSourceTab()) {
                TtsProcessEvents();
                ReadAloudAfterTtsEvents();
                int pos = TtsGetSpokenPosUtf8();
                static int sLastTtsPos = -999;
                if (pos != sLastTtsPos) {
                    sLastTtsPos = pos;
                    DBG_TTS(dbgtts("tick pos=%d speaking=%d\n", pos, (int)TtsIsSpeaking()));
                    ReadAloudUpdateAutoScroll(win);
                    HwndInvalidate(hwnd);
                }
                ReadAloudPlaybackBarTick(win);
            } else {
                ReadAloudHighlightTimerStop(win);
            }
            break;

        case kAutoReloadTimerID: {
            KillTimer(hwnd, kAutoReloadTimerID);
            auto* tab = win->CurrentTab();
            if (tab && tab->reloadOnFocus) {
                if (tab->ignoreNextAutoReload) {
                    // consume the save-triggered watcher event; do not leave
                    // reloadOnFocus set or a later tab focus would reload
                    tab->ignoreNextAutoReload = false;
                    tab->reloadOnFocus = false;
                } else if (AutoReloadFileStillChanging(tab)) {
                    // a writer (LaTeX etc.) is still producing the file: reloading
                    // now shows a half-written document ("cannot find startxref",
                    // "document has no pages") and costs a second reload once the
                    // write finishes. Wait for it to go quiet instead.
                    SetTimer(hwnd, kAutoReloadTimerID, kAutoReloadDelayInMs, nullptr);
                } else {
                    // timer-driven: never ask for a password here (#3493)
                    ReloadDocument(win, true, false);
                }
            }
            break;
        }

        case kSmoothScrollTimerID: {
            DisplayModel* dm = win->AsFixed();
            // Window/tab may have changed while the timer was running.
            if (!dm || !win->scrollAnimActive) {
                StopSmoothScroll(win);
                break;
            }

            // Real dt so motion is smooth even when timer delivery jitters.
            double dtMs = TimeSinceInMs(win->scrollAnimLastTime);
            win->scrollAnimLastTime = TimeGet();
            // Clamp: first tick / resume after stall should not jump a full page.
            if (dtMs < 0.5) {
                dtMs = 0.5;
            } else if (dtMs > 32.0) {
                dtMs = 32.0;
            }
            double dt = dtMs / 1000.0;

            int target = win->scrollTargetY;
            // Keep anim state in sync if something else moved the view.
            int viewY = dm->yOffset();
            if (fabs(win->scrollAnimY - (double)viewY) > 1.5) {
                win->scrollAnimY = (double)viewY;
            }

            double remaining = (double)target - win->scrollAnimY;
            if (fabs(remaining) < kSmoothScrollSnapPx) {
                if (viewY != target) {
                    dm->ScrollYTo(target);
                }
                ReadAloudOnUserViewChanged(win);
                StopSmoothScroll(win);
                break;
            }

            // Exponential approach: pos += (target-pos) * (1 - e^(-k*dt))
            double a = 1.0 - exp(-kSmoothScrollRate * dt);
            a = std::min(a, 1.0);
            win->scrollAnimY += remaining * a;

            int y = (int)lround(win->scrollAnimY);
            if (y != viewY) {
                dm->ScrollYTo(y);
                // If ScrollYTo clamped (document edge), stop chasing an
                // unreachable target.
                int after = dm->yOffset();
                if (after != y) {
                    win->scrollAnimY = (double)after;
                    win->scrollTargetY = after;
                    ReadAloudOnUserViewChanged(win);
                    StopSmoothScroll(win);
                    break;
                }
            }
            // Defer ReadAloud until the animation settles — calling it every
            // tick is work that competes with paint and adds hitchiness.
            break;
        }
    }
}

static void GetDropFilesResolved(HDROP hDrop, bool dragFinish, StrVec& files) {
    int nFiles = DragQueryFile(hDrop, DRAGQUERY_NUMFILES, nullptr, 0);
    WCHAR pathW[MAX_PATH]{};
    for (int i = 0; i < nFiles; i++) {
        DragQueryFile(hDrop, i, pathW, dimof(pathW));
        Str path = ToUtf8Temp(pathW);
        if (str::EndsWithI(path, StrL(".lnk"))) {
            TempStr resolved = ResolveLnkTemp(path);
            if (resolved) {
                path = resolved;
            }
        }
        files.Append(path);
    }
    if (dragFinish) {
        DragFinish(hDrop);
    }
}

static void OnDropFiles(MainWindow* win, HDROP hDrop, bool dragFinish) {
    StrVec files;
    bool isShift = IsShiftPressed();

    GetDropFilesResolved(hDrop, dragFinish, files);
    if (isShift && !win) {
        win = CreateAndShowMainWindow(nullptr);
    }
    StartLoadDocuments(files, win);
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
    if (!lastSlash) {
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
    MainWindow* win = FindMainWindowByHwnd(GetForegroundWindow());
    if (!win && len(gWindows) > 0) {
        win = gWindows[0];
    }
    if (win) {
        LoadArgs args(*path, win);
        StartLoadDocument(&args);
    }
    str::Free(*path);
    delete path;
}

static void DownloadAndOpenUrl(DownloadAndOpenUrlData* data) {
    Str url = data->url;

    TempStr downloadsDir = GetDownloadsDirTemp();
    if (!downloadsDir) {
        logf("DownloadAndOpenUrl: failed to get Downloads folder\n");
        str::Free(data->url);
        delete data;
        return;
    }

    TempStr fileName = FileNameFromUrlTemp(url);
    if (!fileName || str::Eq(fileName, StrL(".")) || str::Eq(fileName, StrL("..")) ||
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

class CanvasDropTarget : public IDropTarget {
    AtomicInt refCount = 1;
    HWND hwnd = nullptr;

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

    STDMETHODIMP DragEnter(IDataObject* dataObj, __unused DWORD grfKeyState, __unused POINTL pt,
                           DWORD* pdwEffect) override {
        if (DataObjectHasFiles(dataObj) || DataObjectHasUrl(dataObj)) {
            *pdwEffect = DROPEFFECT_COPY;
        } else {
            *pdwEffect = DROPEFFECT_NONE;
        }
        return S_OK;
    }

    STDMETHODIMP DragOver(__unused DWORD grfKeyState, __unused POINTL pt, DWORD* pdwEffect) override {
        *pdwEffect = DROPEFFECT_COPY;
        return S_OK;
    }

    STDMETHODIMP DragLeave() override { return S_OK; }

    STDMETHODIMP Drop(IDataObject* dataObj, DWORD /*grfKeyState*/, __unused POINTL pt, DWORD* pdwEffect) override {
        *pdwEffect = DROPEFFECT_COPY;

        // first try file drops (CF_HDROP)
        FORMATETC fmtHDrop = {CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
        STGMEDIUM medium{};
        HRESULT hr = dataObj->GetData(&fmtHDrop, &medium);
        if (SUCCEEDED(hr) && medium.hGlobal) {
            HDROP hDrop = (HDROP)medium.hGlobal;
            MainWindow* win = FindMainWindowByHwnd(hwnd);
            if (win) {
                OnDropFiles(win, hDrop, false);
            }
            ReleaseStgMedium(&medium);
            return S_OK;
        }

        // try URL drop
        TempStr url = GetUrlFromDataObject(dataObj);
        if (!url) {
            // fall back to plain text
            TempStr text = GetTextFromDataObject(dataObj);
            if (text && (str::StartsWithI(text, StrL("http://")) || str::StartsWithI(text, StrL("https://")))) {
                url = text;
            }
        }

        if (url) {
            auto* data = new DownloadAndOpenUrlData();
            data->url = str::Dup(url);
            data->hwndCanvas = hwnd;
            auto fn = MkFunc0<DownloadAndOpenUrlData>(DownloadAndOpenUrl, data);
            RunAsync(fn, StrL("DownloadAndOpenUrl"));
        }

        return S_OK;
    }
};

void RegisterCanvasDropTarget(HWND hwndCanvas) {
    auto* dt = new CanvasDropTarget(hwndCanvas);
    RegisterDragDrop(hwndCanvas, dt);
    dt->Release(); // RegisterDragDrop AddRef'd it
}

void RevokeCanvasDropTarget(HWND hwndCanvas) {
    RevokeDragDrop(hwndCanvas);
}

LRESULT CALLBACK WndProcCanvas(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    DpiScope dpiScope(hwnd);
    // messages that don't require win

    if (msg == WM_NCCALCSIZE && wp == TRUE) {
        // When overlay/hidden scrollbars are active, SetScrollInfo still adds
        // WS_VSCROLL/WS_HSCROLL styles. Handle WM_NCCALCSIZE to prevent Windows
        // from reserving non-client space for native scrollbars.
        if (ScrollbarsAreHidden() || ScrollbarsUseOverlay()) {
            // strip scroll styles that SetScrollInfo may have added
            DWORD style = GetWindowLong(hwnd, GWL_STYLE);
            if (style & (WS_VSCROLL | WS_HSCROLL)) {
                SetWindowLong(hwnd, GWL_STYLE, (LONG)style & ~(WS_VSCROLL | WS_HSCROLL));
            }
            // let DefWindowProc calculate NC size without scroll styles
            return DefWindowProc(hwnd, msg, wp, lp);
        }
    }

    // the canvas hosts wingui controls (the home page's search box); this hands
    // them their own messages (WM_CTLCOLOR*, ...) so they color themselves
    LRESULT res = TryReflectMessages(hwnd, msg, wp, lp);
    if (res) {
        return res;
    }

    MainWindow* win = FindMainWindowByHwnd(hwnd);
    switch (msg) {
        case WM_DROPFILES:
            ReportIf(lp != 0 && lp != 1);
            OnDropFiles(win, (HDROP)wp, !lp);
            return 0;

            // https://docs.microsoft.com/en-us/windows/win32/winmsg/wm-erasebkgnd
        case WM_ERASEBKGND: {
            if (gRedrawLog) {
                Rect rc = HwndClientRect(hwnd);
                logf("redraw: WM_ERASEBKGND hwnd=0x%p (canvas) rc=(%d,%d,%d,%d)\n", hwnd, rc.x, rc.y, rc.dx, rc.dy);
            }
            // markdown/CHM: fill now so leftover pixels from a previous tab
            // cannot show through while WebView2 is resized
            if (win && IsBrowserDocController(win->ctrl)) {
                HdcFillRect((HDC)wp, HwndClientRect(hwnd), ThemeMainWindowBackgroundColor());
                return 1;
            }
            // don't paint here; old content stays until WM_PAINT covers it
            // (CS_HREDRAW|CS_VREDRAW removed so no transparent flash)
            return 1;
        }

        case WM_NCHITTEST: {
            // return HTTRANSPARENT near frame edges so the parent frame
            // can handle resize hit-testing beyond kFrameBorderSize
            if (win && win->tabsInTitlebar && !IsZoomed(GetParent(hwnd))) {
                int x = GET_X_LPARAM(lp);
                int y = GET_Y_LPARAM(lp);
                Rect wrc = HwndWindowRect(GetParent(hwnd));
                int b = kFrameResizeHitTest;
                if ((x - wrc.x) < b || (wrc.x + wrc.dx - x) <= b || (y - wrc.y) < b || (wrc.y + wrc.dy - y) <= b) {
                    return HTTRANSPARENT;
                }
            }
            break;
        }
    }

    if (!win) {
        return DefWindowProc(hwnd, msg, wp, lp);
    }

    // Window close deletes controllers while DestroyWindow (WebView2, etc.) can
    // still deliver canvas messages; don't touch win->ctrl after that starts.
    if (win->isBeingClosed) {
        return DefWindowProc(hwnd, msg, wp, lp);
    }

    // reveal/hide the floating overlay toolbar as the mouse approaches the top;
    // don't consume the message, just observe it
    if (msg == WM_MOUSEMOVE && win->isToolbarOverlay) {
        UpdateOverlayToolbarForMouse(win);
    }

    // messages that require win
    switch (msg) {
        case WM_TIMER:
            OnTimer(win, hwnd, wp);
            return 0;

        case WM_KILLFOCUS:
            // stop middle-button auto-scroll when the canvas loses focus, e.g.
            // the user clicked the bookmarks/menu or minimized the window (#3203)
            if (win->mouseAction == MouseAction::Scrolling) {
                win->mouseAction = MouseAction::None;
                win->xScrollSpeed = 0;
                win->yScrollSpeed = 0;
                win->xScrollAccum = 0;
                win->yScrollAccum = 0;
                KillTimer(hwnd, kAutoScrollTimerID);
            }
            return DefWindowProc(hwnd, msg, wp, lp);

        case WM_SIZE:
            if (!IsIconic(win->hwndFrame)) {
                if (gRedrawLog) {
                    Rect rc = HwndClientRect(hwnd);
                    logf("redraw: WM_SIZE hwnd=0x%p (canvas) size=(%d,%d)\n", hwnd, rc.dx, rc.dy);
                }
                win->UpdateCanvasSize();
                // fully invalidate since layout depends on size
                // (replaces CS_HREDRAW | CS_VREDRAW which caused transparent flash)
                HwndInvalidate(hwnd);
                // paint immediately: newly exposed strips otherwise keep the
                // previous tab's last blit until WebView2 catches up
                if (IsBrowserDocController(win->ctrl)) {
                    FillCanvasThemeBackground(hwnd);
                }
            }
            return 0;

        case WM_GETOBJECT:
            // UI Automation root for screen readers (Narrator, NVDA, …).
            // Not exposed in the browser plugin. Document text is available for
            // fixed-page engines that implement GetTextForPage (PDF/XPS/DjVu).
            if (gPluginMode) {
                return DefWindowProc(hwnd, msg, wp, lp);
            }
            // Only the root object id requests our fragment root; other
            // accessibility ids fall through to the default handler.
            if ((long)lp != (long)UiaRootObjectId) {
                return DefWindowProc(hwnd, msg, wp, lp);
            }
            if (!win->CreateUIAProvider()) {
                return DefWindowProc(hwnd, msg, wp, lp);
            }
            // UiaReturnRawElementProvider AddRefs for the client; MainWindow holds
            // one ref for the window lifetime. Disconnect on window destroy.
            return UiaReturnRawElementProvider(hwnd, wp, lp, win->uiaProvider);

        default:
            // TODO: achieve this split through subclassing or different window classes
            if (win->AsFixed()) {
                HomePageDestroySearch(win);
                return WndProcCanvasFixedPageUI(win, hwnd, msg, wp, lp);
            }

            if (IsBrowserDocController(win->ctrl)) {
                HomePageDestroySearch(win);
                return WndProcCanvasChmUI(win, hwnd, msg, wp, lp);
            }

            if (win->IsCurrentTabAbout()) {
                return WndProcCanvasAbout(win, hwnd, msg, wp, lp);
            }

            HomePageDestroySearch(win);
            return WndProcCanvasLoadError(win, hwnd, msg, wp, lp);
    }
}
