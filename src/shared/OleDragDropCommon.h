/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// --- shared by OleDragDropCommon_win.cpp, orig's Canvas.cpp and ng's gui/OleDragDrop_win.cpp ---

// for the automation channel, which must never drop anything on whatever
// window the real cursor is over: the drag is cancelled by its drop source as
// soon as OLE asks, as if Esc was pressed
extern bool gTestCancelDrag;

void FinishDragDrop(IDataObject* dataObj);
HGLOBAL EncodeBitmapToPngGlobal(HBITMAP hbmp);
HBITMAP CreateProportionalDragThumbnail(HBITMAP src, int maxEdge);
HIMAGELIST CreateDragImageList(HBITMAP hbmp);
TempStr GetDownloadsDirTemp();
TempStr FileNameFromUrlTemp(Str url);
TempStr GetTextFromDataObject(IDataObject* dataObj);
TempStr GetUrlFromDataObject(IDataObject* dataObj);
bool DataObjectHasFiles(IDataObject* dataObj);
bool DataObjectHasUrl(IDataObject* dataObj);

struct DownloadAndOpenUrlData {
    Str url;
    HWND hwndCanvas;
};
void DownloadAndOpenUrl(DownloadAndOpenUrlData* data);

// implemented by each app
void OpenDownloadedPath(Str* path);

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
