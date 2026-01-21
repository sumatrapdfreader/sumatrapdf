/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/GdiPlusUtil.h"
#include "utils/WinUtil.h"
#include "utils/FileUtil.h"
#include "mui/Mui.h"

#include "RegistryPreview.h"

// TODO: move code to PdfPreviewBase.cpp
#include "PdfPreviewBase.h"

#include "utils/Log.h"

constexpr COLORREF kColWindowBg = RGB(0x99, 0x99, 0x99);
constexpr int kPreviewMargin = 2;
constexpr UINT kUwmPaintAgain = (WM_USER + 101);

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

    // Wait for client to connect
    if (!WaitForPipeConnection(hPipe)) {
        logf("GetThumbnailViaPipe: pipe connection failed\n");
        goto cleanup;
    }

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

// Initialize a pipe session for version 2 protocol (session-based preview)
bool PreviewBase::InitPreviewSession() {
    logf("InitPreviewSession\n");

    // Read stream data
    ByteSlice data = GetDataFromStream(m_pStream.Get(), nullptr);
    if (data.empty()) {
        logf("InitPreviewSession: failed to get data from stream\n");
        return false;
    }

    logf("InitPreviewSession: read %d bytes from stream\n", (int)data.size());

    // Generate unique pipe name
    char* pipeName = GenerateUniquePipeName();
    if (!pipeName) {
        logf("InitPreviewSession: failed to generate pipe name\n");
        data.Free();
        return false;
    }

    logf("InitPreviewSession: pipe name '%s'\n", pipeName);

    // Create named pipe (we are the server)
    HANDLE hPipe = CreatePreviewPipe(pipeName);
    if (hPipe == INVALID_HANDLE_VALUE) {
        logf("InitPreviewSession: failed to create pipe\n");
        str::Free(pipeName);
        data.Free();
        return false;
    }

    // Launch SumatraPDF.exe
    HANDLE hProcess = LaunchSumatraForPreview(pipeName);
    if (!hProcess) {
        logf("InitPreviewSession: failed to launch SumatraPDF\n");
        CloseHandle(hPipe);
        str::Free(pipeName);
        data.Free();
        return false;
    }

    str::Free(pipeName);

    // Wait for client to connect
    if (!WaitForPipeConnection(hPipe)) {
        logf("InitPreviewSession: pipe connection failed\n");
        CloseHandle(hPipe);
        TerminateProcess(hProcess, 0);
        CloseHandle(hProcess);
        data.Free();
        return false;
    }

    logf("InitPreviewSession: client connected\n");

    // Send Init command (version 2 protocol)
    DWORD bytesWritten = 0, bytesRead = 0;

    u32 magic = kPreviewRequestMagic;
    u32 version = kPreviewProtocolVersion2;
    u32 cmd = (u32)PreviewCmd::Init;
    u32 fileType = (u32)GetFileType();
    u32 dataSize = (u32)data.size();

    WriteFile(hPipe, &magic, 4, &bytesWritten, nullptr);
    WriteFile(hPipe, &version, 4, &bytesWritten, nullptr);
    WriteFile(hPipe, &cmd, 4, &bytesWritten, nullptr);
    WriteFile(hPipe, &fileType, 4, &bytesWritten, nullptr);
    WriteFile(hPipe, &dataSize, 4, &bytesWritten, nullptr);

    // Write file data
    DWORD totalWritten = 0;
    while (totalWritten < dataSize) {
        DWORD toWrite = dataSize - totalWritten;
        if (!WriteFile(hPipe, data.data() + totalWritten, toWrite, &bytesWritten, nullptr) || bytesWritten == 0) {
            logf("InitPreviewSession: failed to write file data\n");
            CloseHandle(hPipe);
            TerminateProcess(hProcess, 0);
            CloseHandle(hProcess);
            data.Free();
            return false;
        }
        totalWritten += bytesWritten;
    }

    FlushFileBuffers(hPipe);
    data.Free();

    // Read Init response: magic(4) + status(4) + pageCount(4)
    u32 respMagic = 0, status = 0, pageCount = 0;
    if (!ReadFile(hPipe, &respMagic, 4, &bytesRead, nullptr) || bytesRead != 4 || respMagic != kPreviewResponseMagic) {
        logf("InitPreviewSession: invalid response magic\n");
        CloseHandle(hPipe);
        TerminateProcess(hProcess, 0);
        CloseHandle(hProcess);
        return false;
    }
    if (!ReadFile(hPipe, &status, 4, &bytesRead, nullptr) || bytesRead != 4 || status != 0) {
        logf("InitPreviewSession: init failed with status %d\n", status);
        CloseHandle(hPipe);
        TerminateProcess(hProcess, 0);
        CloseHandle(hProcess);
        return false;
    }
    if (!ReadFile(hPipe, &pageCount, 4, &bytesRead, nullptr) || bytesRead != 4) {
        logf("InitPreviewSession: failed to read page count\n");
        CloseHandle(hPipe);
        TerminateProcess(hProcess, 0);
        CloseHandle(hProcess);
        return false;
    }

    logf("InitPreviewSession: success, pageCount=%d\n", pageCount);

    // Create pipe session
    pipeSession = new PreviewPipeSession();
    pipeSession->hPipe = hPipe;
    pipeSession->hProcess = hProcess;
    pipeSession->pageCount = (int)pageCount;

    return true;
}

// PageRenderer class using pipe session
class PageRenderer {
    PreviewPipeSession* session = nullptr;
    HWND hwnd = nullptr;

    int currPage = 0;
    HBITMAP currBmp = nullptr;
    Size currSize;

    int reqPage = 0;
    float reqZoom = 0.f;
    Size reqSize = {};
    bool reqAbort = false;

    CRITICAL_SECTION currAccess;
    HANDLE thread = nullptr;

  public:
    PageRenderer(PreviewPipeSession* session, HWND hwnd) {
        this->session = session;
        this->hwnd = hwnd;
        InitializeCriticalSection(&currAccess);
    }

    ~PageRenderer() {
        if (thread) {
            reqAbort = true;
            WaitForSingleObject(thread, INFINITE);
        }
        if (currBmp) {
            DeleteObject(currBmp);
        }
        DeleteCriticalSection(&currAccess);
    }

    RectF GetPageRect(int pageNo) {
        if (!session || !session->IsConnected()) {
            return RectF();
        }
        return session->GetPageBox(pageNo);
    }

    void Render(HDC hdc, Rect target, int pageNo, float zoom) {
        log("PageRenderer::Render()\n");

        ScopedCritSec scope(&currAccess);
        if (currBmp && currPage == pageNo && currSize == target.Size()) {
            // Blit cached bitmap
            HDC hdcMem = CreateCompatibleDC(hdc);
            HGDIOBJ oldBmp = SelectObject(hdcMem, currBmp);
            BitBlt(hdc, target.x, target.y, target.dx, target.dy, hdcMem, 0, 0, SRCCOPY);
            SelectObject(hdcMem, oldBmp);
            DeleteDC(hdcMem);
        } else if (!thread) {
            reqPage = pageNo;
            reqZoom = zoom;
            reqSize = target.Size();
            reqAbort = false;
            thread = CreateThread(nullptr, 0, RenderThread, this, 0, nullptr);
        } else if (reqPage != pageNo || reqSize != target.Size()) {
            reqAbort = true;
        }
    }

  protected:
    static DWORD WINAPI RenderThread(LPVOID data) {
        log("PageRenderer::RenderThread started\n");
        ScopedCom comScope;

        PageRenderer* pr = (PageRenderer*)data;

        if (!pr->session || !pr->session->IsConnected()) {
            return 0;
        }

        HBITMAP bmp = pr->session->RenderPage(pr->reqPage, pr->reqZoom, pr->reqSize.dx, pr->reqSize.dy);
        if (!bmp) {
            log("PageRenderer::RenderThread: RenderPage failed\n");
            ScopedCritSec scope(&pr->currAccess);
            HANDLE th = pr->thread;
            pr->thread = nullptr;
            CloseHandle(th);
            DestroyTempAllocator();
            return 0;
        }

        ScopedCritSec scope(&pr->currAccess);

        if (!pr->reqAbort) {
            if (pr->currBmp) {
                DeleteObject(pr->currBmp);
            }
            pr->currBmp = bmp;
            pr->currPage = pr->reqPage;
            pr->currSize = pr->reqSize;
        } else {
            DeleteObject(bmp);
        }

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

    int pageCount = 1;
    if (InitPreviewSession() && pipeSession) {
        pageCount = pipeSession->pageCount;
        this->renderer = new PageRenderer(pipeSession, m_hwnd);
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

EpubPreview::EpubPreview(long* plRefCount) : PreviewBase(plRefCount, kEpubPreviewClsid) {
    log("EpubPreview::EpubPreview()\n");
    m_gdiScope = new ScopedGdiPlus();
    mui::Initialize();
}

EpubPreview::~EpubPreview() {
    mui::Destroy();
}

Fb2Preview::Fb2Preview(long* plRefCount) : PreviewBase(plRefCount, kFb2PreviewClsid) {
    m_gdiScope = new ScopedGdiPlus();
    mui::Initialize();
}

Fb2Preview::~Fb2Preview() {
    mui::Destroy();
}

MobiPreview::MobiPreview(long* plRefCount) : PreviewBase(plRefCount, kMobiPreviewClsid) {
    m_gdiScope = new ScopedGdiPlus();
    mui::Initialize();
}

MobiPreview::~MobiPreview() {
    mui::Destroy();
}
