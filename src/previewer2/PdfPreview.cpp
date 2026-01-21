/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/GdiPlusUtil.h"
#include "utils/WinUtil.h"
#include "utils/FileUtil.h"
#include "mui/Mui.h"

#include "wingui/UIModels.h"

#include "Settings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "EngineAll.h"
#include "Annotation.h"
#include "RegistryPreview.h"

// TODO: move code to PdfPreviewBase.cpp
#include "PdfPreviewBase.h"

#include "utils/Log.h"

constexpr COLORREF kColWindowBg = RGB(0x99, 0x99, 0x99);
constexpr int kPreviewMargin = 2;
constexpr UINT kUwmPaintAgain = (WM_USER + 101);

// Protocol constants for named pipe preview - must match SumatraStartup.cpp
constexpr u32 kPreviewRequestMagic = 0x53505657;  // "SPVW" - SumatraPDF Preview
constexpr u32 kPreviewResponseMagic = 0x53505652; // "SPVR" - SumatraPDF Preview Response
constexpr u32 kPreviewProtocolVersion = 1;
constexpr DWORD kPipeTimeoutMs = 30000; // 30 second timeout for pipe operations

EBookUI* GetEBookUI() {
    return nullptr;
}

// Generate a unique pipe name using a GUID
static char* GenerateUniquePipeName() {
    GUID guid;
    if (FAILED(CoCreateGuid(&guid))) {
        return nullptr;
    }
    return str::Format("\\\\.\\pipe\\LOCAL\\SumatraPDF-Preview-%08X%04X%04X%02X%02X%02X%02X%02X%02X%02X%02X",
                       guid.Data1, guid.Data2, guid.Data3, guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
                       guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
}

// Create the named pipe (DLL is the server)
static HANDLE CreatePreviewPipe(const char* pipeName) {
    HANDLE hPipe = CreateNamedPipeA(pipeName, PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE,
                                    PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
                                    1,         // maxInstances - only one client
                                    64 * 1024, // outBufferSize
                                    64 * 1024, // inBufferSize
                                    kPipeTimeoutMs,
                                    nullptr); // security attributes
    if (hPipe == INVALID_HANDLE_VALUE) {
        logf("CreatePreviewPipe: CreateNamedPipeA failed with error %d\n", (int)GetLastError());
    }
    return hPipe;
}

// Launch SumatraPDF.exe with -preview-pipe argument
static HANDLE LaunchSumatraForPreview(const char* pipeName) {
    // Find SumatraPDF.exe in the same directory as the DLL
    TempStr exePath = GetPathInExeDirTemp("SumatraPDF.exe");
    if (!file::Exists(exePath)) {
        logf("LaunchSumatraForPreview: SumatraPDF.exe not found at '%s'\n", exePath);
        return nullptr;
    }

    TempStr cmdLine = str::FormatTemp("\"%s\" -preview-pipe %s", exePath, pipeName);
    logf("LaunchSumatraForPreview: launching '%s'\n", cmdLine);

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi{};
    BOOL ok = CreateProcessA(exePath, cmdLine, nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    if (!ok) {
        logf("LaunchSumatraForPreview: CreateProcessA failed with error %d\n", (int)GetLastError());
        return nullptr;
    }

    CloseHandle(pi.hThread);
    return pi.hProcess;
}

// Send preview request through the pipe
static bool SendPreviewRequest(HANDLE hPipe, PreviewFileType fileType, uint cx, const ByteSlice& data) {
    DWORD bytesWritten = 0;

    // Write header: magic(4) + version(4) + fileType(4) + thumbSize(4) + dataSize(4) = 20 bytes
    u32 magic = kPreviewRequestMagic;
    u32 version = kPreviewProtocolVersion;
    u32 ft = (u32)fileType;
    u32 thumbSize = cx;
    u32 dataSize = (u32)data.size();

    if (!WriteFile(hPipe, &magic, 4, &bytesWritten, nullptr) || bytesWritten != 4) {
        return false;
    }
    if (!WriteFile(hPipe, &version, 4, &bytesWritten, nullptr) || bytesWritten != 4) {
        return false;
    }
    if (!WriteFile(hPipe, &ft, 4, &bytesWritten, nullptr) || bytesWritten != 4) {
        return false;
    }
    if (!WriteFile(hPipe, &thumbSize, 4, &bytesWritten, nullptr) || bytesWritten != 4) {
        return false;
    }
    if (!WriteFile(hPipe, &dataSize, 4, &bytesWritten, nullptr) || bytesWritten != 4) {
        return false;
    }

    // Write file data
    DWORD totalWritten = 0;
    while (totalWritten < dataSize) {
        DWORD toWrite = dataSize - totalWritten;
        if (!WriteFile(hPipe, data.data() + totalWritten, toWrite, &bytesWritten, nullptr) || bytesWritten == 0) {
            return false;
        }
        totalWritten += bytesWritten;
    }

    FlushFileBuffers(hPipe);
    return true;
}

// Receive preview response from the pipe and create HBITMAP
static HBITMAP ReceivePreviewResponse(HANDLE hPipe) {
    DWORD bytesRead = 0;

    // Read response header: magic(4) + status(4) + width(4) + height(4) + dataLen(4) = 20 bytes
    u32 magic = 0, status = 0, width = 0, height = 0, bmpDataLen = 0;

    if (!ReadFile(hPipe, &magic, 4, &bytesRead, nullptr) || bytesRead != 4 || magic != kPreviewResponseMagic) {
        logf("ReceivePreviewResponse: invalid magic 0x%08x\n", magic);
        return nullptr;
    }
    if (!ReadFile(hPipe, &status, 4, &bytesRead, nullptr) || bytesRead != 4) {
        logf("ReceivePreviewResponse: failed to read status\n");
        return nullptr;
    }
    if (status != 0) {
        logf("ReceivePreviewResponse: server returned error status %d\n", status);
        return nullptr;
    }
    if (!ReadFile(hPipe, &width, 4, &bytesRead, nullptr) || bytesRead != 4) {
        logf("ReceivePreviewResponse: failed to read width\n");
        return nullptr;
    }
    if (!ReadFile(hPipe, &height, 4, &bytesRead, nullptr) || bytesRead != 4) {
        logf("ReceivePreviewResponse: failed to read height\n");
        return nullptr;
    }
    if (!ReadFile(hPipe, &bmpDataLen, 4, &bytesRead, nullptr) || bytesRead != 4) {
        logf("ReceivePreviewResponse: failed to read bmpDataLen\n");
        return nullptr;
    }

    logf("ReceivePreviewResponse: width=%d, height=%d, bmpDataLen=%d\n", width, height, bmpDataLen);

    if (bmpDataLen == 0 || bmpDataLen != width * height * 4) {
        logf("ReceivePreviewResponse: invalid bitmap data length\n");
        return nullptr;
    }

    // Read bitmap data
    u8* bmpData = AllocArray<u8>(bmpDataLen);
    if (!bmpData) {
        logf("ReceivePreviewResponse: failed to allocate %d bytes\n", bmpDataLen);
        return nullptr;
    }

    DWORD totalRead = 0;
    while (totalRead < bmpDataLen) {
        DWORD toRead = bmpDataLen - totalRead;
        if (!ReadFile(hPipe, bmpData + totalRead, toRead, &bytesRead, nullptr) || bytesRead == 0) {
            logf("ReceivePreviewResponse: failed to read bitmap data at offset %d\n", totalRead);
            free(bmpData);
            return nullptr;
        }
        totalRead += bytesRead;
    }

    // Create DIB section from the bitmap data
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
        logf("ReceivePreviewResponse: CreateDIBSection failed\n");
        free(bmpData);
        return nullptr;
    }

    // Copy the bitmap data
    memcpy(dibData, bmpData, bmpDataLen);
    free(bmpData);

    return hBitmap;
}

HBITMAP PreviewBase::GetThumbnailViaPipe(uint cx) {
    logf("GetThumbnailViaPipe: cx=%d\n", cx);

    // Read stream data
    ByteSlice data = GetDataFromStream(m_pStream.Get(), nullptr);
    if (data.empty()) {
        logf("GetThumbnailViaPipe: failed to get data from stream\n");
        return nullptr;
    }

    logf("GetThumbnailViaPipe: read %d bytes from stream\n", (int)data.size());

    // Generate unique pipe name
    char* pipeName = GenerateUniquePipeName();
    if (!pipeName) {
        logf("GetThumbnailViaPipe: failed to generate pipe name\n");
        data.Free();
        return nullptr;
    }

    logf("GetThumbnailViaPipe: pipe name '%s'\n", pipeName);

    // Create named pipe (we are the server)
    HANDLE hPipe = CreatePreviewPipe(pipeName);
    if (hPipe == INVALID_HANDLE_VALUE) {
        logf("GetThumbnailViaPipe: failed to create pipe\n");
        str::Free(pipeName);
        data.Free();
        return nullptr;
    }

    // Launch SumatraPDF.exe
    HANDLE hProcess = LaunchSumatraForPreview(pipeName);
    if (!hProcess) {
        logf("GetThumbnailViaPipe: failed to launch SumatraPDF\n");
        CloseHandle(hPipe);
        str::Free(pipeName);
        data.Free();
        return nullptr;
    }

    str::Free(pipeName);

    HBITMAP result = nullptr;

    // Wait for client to connect (with timeout)
    // Use overlapped I/O for proper timeout handling
    OVERLAPPED ov{};
    ov.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    if (!ov.hEvent) {
        logf("GetThumbnailViaPipe: failed to create event\n");
        goto cleanup;
    }

    if (!ConnectNamedPipe(hPipe, &ov)) {
        DWORD err = GetLastError();
        if (err == ERROR_PIPE_CONNECTED) {
            // Client already connected, that's fine
        } else if (err == ERROR_IO_PENDING) {
            // Wait for connection with timeout
            DWORD waitResult = WaitForSingleObject(ov.hEvent, kPipeTimeoutMs);
            if (waitResult != WAIT_OBJECT_0) {
                logf("GetThumbnailViaPipe: pipe connection timed out or failed\n");
                CloseHandle(ov.hEvent);
                goto cleanup;
            }
        } else {
            logf("GetThumbnailViaPipe: ConnectNamedPipe failed with error %d\n", (int)err);
            CloseHandle(ov.hEvent);
            goto cleanup;
        }
    }

    CloseHandle(ov.hEvent);

    logf("GetThumbnailViaPipe: client connected\n");

    // Send request
    if (!SendPreviewRequest(hPipe, GetFileType(), cx, data)) {
        logf("GetThumbnailViaPipe: failed to send request\n");
        goto cleanup;
    }

    logf("GetThumbnailViaPipe: request sent\n");

    // Receive response
    result = ReceivePreviewResponse(hPipe);
    if (result) {
        logf("GetThumbnailViaPipe: received bitmap\n");
    } else {
        logf("GetThumbnailViaPipe: failed to receive response\n");
    }

cleanup:
    DisconnectNamedPipe(hPipe);
    CloseHandle(hPipe);

    // Terminate the process if still running
    TerminateProcess(hProcess, 0);
    CloseHandle(hProcess);

    data.Free();

    return result;
}

IFACEMETHODIMP PreviewBase::GetThumbnail(uint cx, HBITMAP* phbmp, WTS_ALPHATYPE* pdwAlpha) {
    logf("PreviewBase::GetThumbnail(cx=%d)\n", (int)cx);

    // Use pipe communication to SumatraPDF.exe for thumbnail generation
    HBITMAP hBitmap = GetThumbnailViaPipe(cx);
    if (!hBitmap) {
        logf("PreviewBase::GetThumbnail: GetThumbnailViaPipe failed\n");
        return E_FAIL;
    }

    *phbmp = hBitmap;
    if (pdwAlpha) {
        *pdwAlpha = WTSAT_RGB;
    }
    logf("PreviewBase::GetThumbnail: provided thumbnail via pipe\n");
    return S_OK;
}

class PageRenderer {
    EngineBase* engine = nullptr;
    HWND hwnd = nullptr;

    int currPage = 0;
    RenderedBitmap* currBmp = nullptr;
    // due to rounding differences, currBmp->Size() and currSize can differ slightly
    Size currSize;
    int reqPage = 0;
    float reqZoom = 0.f;
    Size reqSize = {};
    bool reqAbort = false;
    AbortCookie* abortCookie = nullptr;

    CRITICAL_SECTION currAccess;
    HANDLE thread = nullptr;

    // seeking inside an IStream spins an inner event loop
    // which can cause reentrance in OnPaint and leave an
    // engine semi-initialized when it's called recursively
    // (this only applies for the UI thread where the critical
    // sections can't prevent recursion without the risk of deadlock)
    bool preventRecursion = false;

  public:
    PageRenderer(EngineBase* engine, HWND hwnd) {
        this->engine = engine;
        this->hwnd = hwnd;
        InitializeCriticalSection(&currAccess);
    }
    ~PageRenderer() {
        if (thread) {
            WaitForSingleObject(thread, INFINITE);
        }
        delete currBmp;
        DeleteCriticalSection(&currAccess);
    }

    RectF GetPageRect(int pageNo) {
        if (preventRecursion) {
            return RectF();
        }

        preventRecursion = true;
        // assume that any engine methods could lead to a seek
        RectF bbox = engine->PageMediabox(pageNo);
        bbox = engine->Transform(bbox, pageNo, 1.0, 0);
        preventRecursion = false;
        return bbox;
    }

    void Render(HDC hdc, Rect target, int pageNo, float zoom) {
        log("PageRenderer::Render()\n");

        ScopedCritSec scope(&currAccess);
        if (currBmp && currPage == pageNo && currSize == target.Size()) {
            currBmp->Blit(hdc, target);
        } else if (!thread) {
            reqPage = pageNo;
            reqZoom = zoom;
            reqSize = target.Size();
            reqAbort = false;
            thread = CreateThread(nullptr, 0, RenderThread, this, 0, nullptr);
        } else if (reqPage != pageNo || reqSize != target.Size()) {
            if (abortCookie) {
                abortCookie->Abort();
            }
            reqAbort = true;
        }
    }

  protected:
    static DWORD WINAPI RenderThread(LPVOID data) {
        log("PageRenderer::RenderThread started\n");
        ScopedCom comScope; // because the engine reads data from a COM IStream

        PageRenderer* pr = (PageRenderer*)data;
        RenderPageArgs args(pr->reqPage, pr->reqZoom, 0, nullptr, RenderTarget::View, &pr->abortCookie);
        RenderedBitmap* bmp = pr->engine->RenderPage(args);
        if (!bmp) {
            return 0;
        }

        ScopedCritSec scope(&pr->currAccess);

        if (!pr->reqAbort) {
            delete pr->currBmp;
            pr->currBmp = bmp;
            pr->currPage = pr->reqPage;
            pr->currSize = pr->reqSize;
        } else {
            delete bmp;
        }
        delete pr->abortCookie;
        pr->abortCookie = nullptr;

        HANDLE th = pr->thread;
        pr->thread = nullptr;
        PostMessageW(pr->hwnd, kUwmPaintAgain, 0, 0);

        CloseHandle(th);
        DestroyTempAllocator();
        return 0;
    }
};

static LRESULT OnPaint(HWND hwnd) {
    Rect rect = ClientRect(hwnd);
    DoubleBuffer buffer(hwnd, rect);
    HDC hdc = buffer.GetDC();
    HBRUSH brushBg = CreateSolidBrush(kColWindowBg);
    HBRUSH brushWhite = GetStockBrush(WHITE_BRUSH);
    RECT rcClient = ToRECT(rect);
    FillRect(hdc, &rcClient, brushBg);

    PreviewBase* preview = (PreviewBase*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    if (preview && preview->renderer) {
        int pageNo = GetScrollPos(hwnd, SB_VERT);
        RectF page = preview->renderer->GetPageRect(pageNo);
        if (!page.IsEmpty()) {
            rect.Inflate(-kPreviewMargin, -kPreviewMargin);
            float zoom = (float)std::min(rect.dx / page.dx, rect.dy / page.dy) - 0.001f;
            Rect onScreen = RectF((float)rect.x, (float)rect.y, (float)page.dx * zoom, (float)page.dy * zoom).Round();
            onScreen.Offset((rect.dx - onScreen.dx) / 2, (rect.dy - onScreen.dy) / 2);

            RECT rcPage = ToRECT(onScreen);
            FillRect(hdc, &rcPage, brushWhite);
            preview->renderer->Render(hdc, onScreen, pageNo, zoom);
        }
    }

    DeleteObject(brushBg);
    DeleteObject(brushWhite);

    PAINTSTRUCT ps;
    buffer.Flush(BeginPaint(hwnd, &ps));
    EndPaint(hwnd, &ps);
    return 0;
}

static LRESULT OnVScroll(HWND hwnd, WPARAM wp) {
    SCROLLINFO si{};
    si.cbSize = sizeof(si);
    si.fMask = SIF_ALL;
    GetScrollInfo(hwnd, SB_VERT, &si);

    switch (LOWORD(wp)) {
        case SB_TOP:
            si.nPos = si.nMin;
            break;
        case SB_BOTTOM:
            si.nPos = si.nMax;
            break;
        case SB_LINEUP:
            si.nPos--;
            break;
        case SB_LINEDOWN:
            si.nPos++;
            break;
        case SB_PAGEUP:
            si.nPos--;
            break;
        case SB_PAGEDOWN:
            si.nPos++;
            break;
        case SB_THUMBTRACK:
            si.nPos = si.nTrackPos;
            break;
    }
    si.fMask = SIF_POS;
    SetScrollInfo(hwnd, SB_VERT, &si, TRUE);

    InvalidateRect(hwnd, nullptr, TRUE);
    UpdateWindow(hwnd);
    return 0;
}

static LRESULT OnKeydown(HWND hwnd, WPARAM key) {
    switch (key) {
        case VK_DOWN:
        case VK_RIGHT:
        case VK_NEXT:
            return OnVScroll(hwnd, SB_PAGEDOWN);
        case VK_UP:
        case VK_LEFT:
        case VK_PRIOR:
            return OnVScroll(hwnd, SB_PAGEUP);
        case VK_HOME:
            return OnVScroll(hwnd, SB_TOP);
        case VK_END:
            return OnVScroll(hwnd, SB_BOTTOM);
        default:
            return 0;
    }
}

static LRESULT OnDestroy(HWND hwnd) {
    PreviewBase* preview = (PreviewBase*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    if (preview) {
        delete preview->renderer;
        preview->renderer = nullptr;
    }
    return 0;
}

static LRESULT CALLBACK PreviewWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_PAINT:
            return OnPaint(hwnd);
        case WM_VSCROLL:
            return OnVScroll(hwnd, wp);
        case WM_KEYDOWN:
            return OnKeydown(hwnd, wp);
        case WM_LBUTTONDOWN:
            HwndSetFocus(hwnd);
            return 0;
        case WM_MOUSEWHEEL: {
            auto delta = GET_WHEEL_DELTA_WPARAM(wp);
            wp = delta > 0 ? SB_LINEUP : SB_LINEDOWN;
            return OnVScroll(hwnd, wp);
        }
        case WM_DESTROY:
            return OnDestroy(hwnd);
        case kUwmPaintAgain:
            InvalidateRect(hwnd, nullptr, TRUE);
            UpdateWindow(hwnd);
            return 0;
        default:
            return DefWindowProc(hwnd, msg, wp, lp);
    }
}

IFACEMETHODIMP PreviewBase::DoPreview() {
    log("PreviewBase::DoPreview()\n");

    WNDCLASSEX wcex{};
    wcex.cbSize = sizeof(wcex);
    wcex.lpfnWndProc = PreviewWndProc;
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.lpszClassName = L"SumatraPDF_PreviewPane";
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    RegisterClassEx(&wcex);

    m_hwnd = CreateWindow(wcex.lpszClassName, nullptr, WS_CHILD | WS_VSCROLL | WS_VISIBLE, m_rcParent.x, m_rcParent.x,
                          m_rcParent.dx, m_rcParent.dy, m_hwndParent, nullptr, nullptr, nullptr);
    if (!m_hwnd) {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    this->renderer = nullptr;
    SetWindowLongPtr(m_hwnd, GWLP_USERDATA, (LONG_PTR)this);

    EngineBase* engine = GetEngine();
    int pageCount = 1;
    if (engine) {
        pageCount = engine->PageCount();
        this->renderer = new PageRenderer(engine, m_hwnd);
        // don't use the engine afterwards directly (cf. PageRenderer::preventRecursion)
        engine = nullptr;
    }

    SCROLLINFO si{};
    si.cbSize = sizeof(si);
    si.fMask = SIF_ALL;
    si.nPos = 1;
    si.nMin = 1;
    si.nMax = pageCount;
    si.nPage = si.nMax > 1 ? 1 : 2;
    SetScrollInfo(m_hwnd, SB_VERT, &si, TRUE);

    ShowWindow(m_hwnd, SW_SHOW);
    return S_OK;
}

EngineBase* PdfPreview::LoadEngine(IStream* stream) {
    log("PdfPreview::LoadEngine()\n");
    return CreateEngineMupdfFromStream(stream, "foo.pdf");
}

#if 0
EngineBase* XpsPreview::LoadEngine(IStream* stream) {
    return CreateEngineXpFromStream(stream);
}
#endif

EngineBase* DjVuPreview::LoadEngine(IStream* stream) {
    log("DjVuPreview::LoadEngine()\n");
    return CreateEngineDjVuFromStream(stream);
}

EpubPreview::EpubPreview(long* plRefCount) : PreviewBase(plRefCount, kEpubPreviewClsid) {
    log("EpubPreview::EpubPreview()\n");
    m_gdiScope = new ScopedGdiPlus();
    mui::Initialize();
}

EpubPreview::~EpubPreview() {
    mui::Destroy();
}

EngineBase* EpubPreview::LoadEngine(IStream* stream) {
    log("EpubPreview::LoadEngine()\n");
    return CreateEngineEpubFromStream(stream);
}

Fb2Preview::Fb2Preview(long* plRefCount) : PreviewBase(plRefCount, kFb2PreviewClsid) {
    m_gdiScope = new ScopedGdiPlus();
    mui::Initialize();
}

Fb2Preview::~Fb2Preview() {
    mui::Destroy();
}

EngineBase* Fb2Preview::LoadEngine(IStream* stream) {
    log("Fb2Preview::LoadEngine()\n");
    return CreateEngineFb2FromStream(stream);
}

MobiPreview::MobiPreview(long* plRefCount) : PreviewBase(plRefCount, kMobiPreviewClsid) {
    m_gdiScope = new ScopedGdiPlus();
    mui::Initialize();
}

MobiPreview::~MobiPreview() {
    mui::Destroy();
}

EngineBase* MobiPreview::LoadEngine(IStream* stream) {
    log("MobiPreview::LoadEngine()\n");
    return CreateEngineMobiFromStream(stream);
}

EngineBase* CbxPreview::LoadEngine(IStream* stream) {
    log("CbxPreview::LoadEngine()\n");
    return CreateEngineCbxFromStream(stream);
}

EngineBase* TgaPreview::LoadEngine(IStream* stream) {
    log("TgaPreview::LoadEngine()\n");
    return CreateEngineImageFromStream(stream);
}
