/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include <Thumbcache.h>

class PageRenderer;

// File type enum for preview pipe protocol
enum class PreviewFileType : u32 {
    PDF = 1,
    DjVu = 2,
    EPUB = 3,
    FB2 = 4,
    MOBI = 5,
    CBX = 6,
    TGA = 7
};

// Protocol constants for named pipe preview - must match SumatraStartup.cpp
constexpr u32 kPreviewRequestMagic = 0x53505657;  // "SPVW" - SumatraPDF Preview
constexpr u32 kPreviewResponseMagic = 0x53505652; // "SPVR" - SumatraPDF Preview Response
constexpr u32 kPreviewProtocolVersion = 1;        // One-shot thumbnail mode
constexpr u32 kPreviewProtocolVersion2 = 2;       // Session-based preview mode
constexpr DWORD kPipeTimeoutMs = 30000;           // 30 second timeout for pipe operations

// Commands for protocol version 2 (session-based)
enum class PreviewCmd : u32 {
    Init = 1,       // Initialize with file data, returns page count
    GetPageBox = 2, // Get page dimensions
    Render = 3,     // Render a page
    Shutdown = 255, // Close session
};

// Pipe session for version 2 protocol
class PreviewPipeSession {
  public:
    HANDLE hPipe = INVALID_HANDLE_VALUE;
    HANDLE hProcess = nullptr;
    int pageCount = 0;

    ~PreviewPipeSession() {
        Close();
    }

    bool IsConnected() const {
        return hPipe != INVALID_HANDLE_VALUE;
    }

    void Close() {
        if (hPipe != INVALID_HANDLE_VALUE) {
            // Send shutdown command
            DWORD bytesWritten = 0;
            u32 magic = kPreviewRequestMagic;
            u32 version = kPreviewProtocolVersion2;
            u32 cmd = (u32)PreviewCmd::Shutdown;
            WriteFile(hPipe, &magic, 4, &bytesWritten, nullptr);
            WriteFile(hPipe, &version, 4, &bytesWritten, nullptr);
            WriteFile(hPipe, &cmd, 4, &bytesWritten, nullptr);
            FlushFileBuffers(hPipe);
            DisconnectNamedPipe(hPipe);
            CloseHandle(hPipe);
            hPipe = INVALID_HANDLE_VALUE;
        }
        if (hProcess) {
            TerminateProcess(hProcess, 0);
            CloseHandle(hProcess);
            hProcess = nullptr;
        }
        pageCount = 0;
    }

    // Send GetPageBox command and return page dimensions (in points at zoom 1.0)
    RectF GetPageBox(int pageNo) {
        if (!IsConnected()) {
            return RectF();
        }

        DWORD bytesWritten = 0, bytesRead = 0;

        // Send command header + pageNo
        u32 magic = kPreviewRequestMagic;
        u32 version = kPreviewProtocolVersion2;
        u32 cmd = (u32)PreviewCmd::GetPageBox;
        u32 pn = (u32)pageNo;

        WriteFile(hPipe, &magic, 4, &bytesWritten, nullptr);
        WriteFile(hPipe, &version, 4, &bytesWritten, nullptr);
        WriteFile(hPipe, &cmd, 4, &bytesWritten, nullptr);
        WriteFile(hPipe, &pn, 4, &bytesWritten, nullptr);
        FlushFileBuffers(hPipe);

        // Read response: magic(4) + status(4) + widthBits(4) + heightBits(4)
        u32 respMagic = 0, status = 0, widthBits = 0, heightBits = 0;
        if (!ReadFile(hPipe, &respMagic, 4, &bytesRead, nullptr) || bytesRead != 4 ||
            respMagic != kPreviewResponseMagic) {
            return RectF();
        }
        if (!ReadFile(hPipe, &status, 4, &bytesRead, nullptr) || bytesRead != 4 || status != 0) {
            return RectF();
        }
        if (!ReadFile(hPipe, &widthBits, 4, &bytesRead, nullptr) || bytesRead != 4) {
            return RectF();
        }
        if (!ReadFile(hPipe, &heightBits, 4, &bytesRead, nullptr) || bytesRead != 4) {
            return RectF();
        }

        float width, height;
        memcpy(&width, &widthBits, 4);
        memcpy(&height, &heightBits, 4);

        return RectF(0, 0, width, height);
    }

    // Send Render command and return bitmap (caller owns the returned HBITMAP)
    HBITMAP RenderPage(int pageNo, float zoom, int targetWidth, int targetHeight) {
        if (!IsConnected()) {
            return nullptr;
        }

        DWORD bytesWritten = 0, bytesRead = 0;

        // Send command header + render params
        u32 magic = kPreviewRequestMagic;
        u32 version = kPreviewProtocolVersion2;
        u32 cmd = (u32)PreviewCmd::Render;
        u32 pn = (u32)pageNo;
        u32 zoomBits;
        memcpy(&zoomBits, &zoom, 4);
        u32 tw = (u32)targetWidth;
        u32 th = (u32)targetHeight;

        WriteFile(hPipe, &magic, 4, &bytesWritten, nullptr);
        WriteFile(hPipe, &version, 4, &bytesWritten, nullptr);
        WriteFile(hPipe, &cmd, 4, &bytesWritten, nullptr);
        WriteFile(hPipe, &pn, 4, &bytesWritten, nullptr);
        WriteFile(hPipe, &zoomBits, 4, &bytesWritten, nullptr);
        WriteFile(hPipe, &tw, 4, &bytesWritten, nullptr);
        WriteFile(hPipe, &th, 4, &bytesWritten, nullptr);
        FlushFileBuffers(hPipe);

        // Read response header: magic(4) + status(4) + width(4) + height(4) + dataLen(4)
        u32 respMagic = 0, status = 0, width = 0, height = 0, bmpDataLen = 0;
        if (!ReadFile(hPipe, &respMagic, 4, &bytesRead, nullptr) || bytesRead != 4 ||
            respMagic != kPreviewResponseMagic) {
            return nullptr;
        }
        if (!ReadFile(hPipe, &status, 4, &bytesRead, nullptr) || bytesRead != 4 || status != 0) {
            return nullptr;
        }
        if (!ReadFile(hPipe, &width, 4, &bytesRead, nullptr) || bytesRead != 4) {
            return nullptr;
        }
        if (!ReadFile(hPipe, &height, 4, &bytesRead, nullptr) || bytesRead != 4) {
            return nullptr;
        }
        if (!ReadFile(hPipe, &bmpDataLen, 4, &bytesRead, nullptr) || bytesRead != 4) {
            return nullptr;
        }

        if (bmpDataLen == 0 || bmpDataLen != width * height * 4) {
            return nullptr;
        }

        // Read bitmap data
        u8* bmpData = AllocArray<u8>(bmpDataLen);
        if (!bmpData) {
            return nullptr;
        }

        DWORD totalRead = 0;
        while (totalRead < bmpDataLen) {
            DWORD toRead = bmpDataLen - totalRead;
            if (!ReadFile(hPipe, bmpData + totalRead, toRead, &bytesRead, nullptr) || bytesRead == 0) {
                free(bmpData);
                return nullptr;
            }
            totalRead += bytesRead;
        }

        // Create DIB section
        BITMAPINFO bmi{};
        bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
        bmi.bmiHeader.biWidth = width;
        bmi.bmiHeader.biHeight = height; // positive = bottom-up DIB
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        u8* dibData = nullptr;
        HBITMAP hBitmap = CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS, (void**)&dibData, nullptr, 0);
        if (!hBitmap || !dibData) {
            free(bmpData);
            return nullptr;
        }

        memcpy(dibData, bmpData, bmpDataLen);
        free(bmpData);

        return hBitmap;
    }
};

class PreviewBase : public IThumbnailProvider,
                    public IInitializeWithStream,
                    public IObjectWithSite,
                    public IPreviewHandler,
                    public IOleWindow {
  public:
    PreviewBase(long* plRefCount, const char* clsid) {
        m_plModuleRef = plRefCount;
        InterlockedIncrement(m_plModuleRef);
    }

    virtual ~PreviewBase() {
        Unload();
        delete m_gdiScope;
        InterlockedDecrement(m_plModuleRef);
    }

    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) {
        static const QITAB qit[] = {QITABENT(PreviewBase, IInitializeWithStream),
                                    QITABENT(PreviewBase, IThumbnailProvider),
                                    QITABENT(PreviewBase, IObjectWithSite),
                                    QITABENT(PreviewBase, IPreviewHandler),
                                    QITABENT(PreviewBase, IOleWindow),
                                    {0}};
        return QISearch(this, qit, riid, ppv);
    }
    IFACEMETHODIMP_(ULONG) AddRef() {
        return InterlockedIncrement(&m_lRef);
    }
    IFACEMETHODIMP_(ULONG) Release() {
        long cRef = InterlockedDecrement(&m_lRef);
        if (cRef == 0) {
            delete this;
        }
        return cRef;
    }

    // IThumbnailProvider
    IFACEMETHODIMP GetThumbnail(uint cx, HBITMAP* phbmp, WTS_ALPHATYPE* pdwAlpha);

    // IInitializeWithStream
    IFACEMETHODIMP Initialize(IStream* pStm, __unused DWORD grfMode) {
        m_pStream = pStm;
        if (!m_pStream) {
            return E_INVALIDARG;
        }
        m_pStream->AddRef();
        return S_OK;
    };

    // IObjectWithSite
    IFACEMETHODIMP SetSite(IUnknown* punkSite) {
        m_site = nullptr;
        if (!punkSite) {
            return S_OK;
        }
        return punkSite->QueryInterface(&m_site);
    }
    IFACEMETHODIMP GetSite(REFIID riid, void** ppv) {
        if (m_site) {
            return m_site->QueryInterface(riid, ppv);
        }
        if (!ppv) {
            return E_INVALIDARG;
        }
        *ppv = nullptr;
        return E_FAIL;
    }

    // IPreviewHandler
    IFACEMETHODIMP SetWindow(HWND hwnd, const RECT* prc) {
        if (!hwnd || !prc) {
            return S_OK;
        }
        m_hwndParent = hwnd;
        return SetRect(prc);
    }
    IFACEMETHODIMP SetFocus() {
        if (!m_hwnd) {
            return S_FALSE;
        }
        ::SetFocus(m_hwnd);
        return S_OK;
    }
    IFACEMETHODIMP QueryFocus(HWND* phwnd) {
        if (!phwnd) {
            return E_INVALIDARG;
        }
        *phwnd = GetFocus();
        if (!*phwnd) {
            return HRESULT_FROM_WIN32(GetLastError());
        }
        return S_OK;
    }
    IFACEMETHODIMP TranslateAccelerator(MSG* pmsg) {
        if (!m_site) {
            return S_FALSE;
        }
        ScopedComQIPtr<IPreviewHandlerFrame> frame(m_site);
        if (!frame) {
            return S_FALSE;
        }
        return frame->TranslateAccelerator(pmsg);
    }
    IFACEMETHODIMP SetRect(const RECT* prc) {
        if (!prc) {
            return E_INVALIDARG;
        }
        m_rcParent = ToRect(*prc);
        if (m_hwnd) {
            UINT flags = SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE;
            int x = m_rcParent.x;
            int y = m_rcParent.y;
            int dx = m_rcParent.dx;
            int dy = m_rcParent.dy;
            SetWindowPos(m_hwnd, nullptr, x, y, dx, dy, flags);
            InvalidateRect(m_hwnd, nullptr, TRUE);
            UpdateWindow(m_hwnd);
        }
        return S_OK;
    }
    IFACEMETHODIMP DoPreview();
    IFACEMETHODIMP Unload() {
        if (m_hwnd) {
            DestroyWindow(m_hwnd);
            m_hwnd = nullptr;
        }
        m_pStream = nullptr;
        if (pipeSession) {
            delete pipeSession;
            pipeSession = nullptr;
        }
        return S_OK;
    }

    // IOleWindow
    IFACEMETHODIMP GetWindow(HWND* phwnd) {
        if (!m_hwndParent || !phwnd) {
            return E_INVALIDARG;
        }
        *phwnd = m_hwndParent;
        return S_OK;
    }
    IFACEMETHODIMP ContextSensitiveHelp(__unused BOOL fEnterMode) {
        return E_NOTIMPL;
    }

    PageRenderer* renderer = nullptr;
    PreviewPipeSession* pipeSession = nullptr;

  protected:
    long m_lRef = 1;
    long* m_plModuleRef = nullptr;
    ScopedComPtr<IStream> m_pStream;
    // engines based on EngineImages require GDI+ to be preloaded
    ScopedGdiPlus* m_gdiScope = nullptr;
    // state for IPreviewHandler
    ScopedComPtr<IUnknown> m_site;
    HWND m_hwnd = nullptr;
    HWND m_hwndParent = nullptr;
    Rect m_rcParent;

    virtual PreviewFileType GetFileType() = 0;

    HBITMAP GetThumbnailViaPipe(uint cx);
    bool InitPreviewSession();
};

class PdfPreview : public PreviewBase {
  public:
    PdfPreview(long* plRefCount) : PreviewBase(plRefCount, kPdfPreviewClsid) {
    }

  protected:
    PreviewFileType GetFileType() override {
        return PreviewFileType::PDF;
    }
};

#if 0
class XpsPreview : public PreviewBase {
  public:
    XpsPreview(long* plRefCount) : PreviewBase(plRefCount, kXpsPreviewClsid) {
    }

  protected:
    PreviewFileType GetFileType() override {
        return PreviewFileType::XPS;
    }
};
#endif

class DjVuPreview : public PreviewBase {
  public:
    DjVuPreview(long* plRefCount) : PreviewBase(plRefCount, kDjVuPreviewClsid) {
        m_gdiScope = new ScopedGdiPlus();
    }

  protected:
    PreviewFileType GetFileType() override {
        return PreviewFileType::DjVu;
    }
};

class EpubPreview : public PreviewBase {
  public:
    EpubPreview(long* plRefCount);
    ~EpubPreview();

  protected:
    PreviewFileType GetFileType() override {
        return PreviewFileType::EPUB;
    }
};

class Fb2Preview : public PreviewBase {
  public:
    Fb2Preview(long* plRefCount);
    ~Fb2Preview();

  protected:
    PreviewFileType GetFileType() override {
        return PreviewFileType::FB2;
    }
};

class MobiPreview : public PreviewBase {
  public:
    MobiPreview(long* plRefCount);
    ~MobiPreview();

  protected:
    PreviewFileType GetFileType() override {
        return PreviewFileType::MOBI;
    }
};

class CbxPreview : public PreviewBase {
  public:
    CbxPreview(long* plRefCount) : PreviewBase(plRefCount, kCbxPreviewClsid) {
        m_gdiScope = new ScopedGdiPlus();
    }

  protected:
    PreviewFileType GetFileType() override {
        return PreviewFileType::CBX;
    }
};

class TgaPreview : public PreviewBase {
  public:
    TgaPreview(long* plRefCount) : PreviewBase(plRefCount, kTgaPreviewClsid) {
        m_gdiScope = new ScopedGdiPlus();
    }

  protected:
    PreviewFileType GetFileType() override {
        return PreviewFileType::TGA;
    }
};
