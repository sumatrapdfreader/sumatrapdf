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
#include "OleDragDropCommon.h"

#include "SumatraLog.h"

static HRESULT gLastDragResult = E_FAIL;
static int gLastDragTextLen = 0;

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

// Longest edge of the proportional drag-out thumbnail (logical px; DPI-scaled).
constexpr int kDragImageThumbnailSize = 220;

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

void OpenDownloadedPath(Str* path) {
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
