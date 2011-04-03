/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include <windows.h>
#include "ComicBook.h"
#include "SumatraPDF.h"
#include "StrUtil.h"
#include "Vec.h"
#include "WinUtil.h"
#include "FileUtil.h"
#include "WindowInfo.h"
#include "AppPrefs.h"
#include "FileHistory.h"
#include "AppTools.h"

// mini(un)zip
#include <ioapi.h>
#include <iowin32.h>
#include <unzip.h>

using namespace Gdiplus;

bool IsComicBook(const TCHAR *fileName)
{
    if (Str::EndsWithI(fileName, _T(".cbz")))
        return true;
#if 0 // not yet
    if (Str::EndsWithI(fileName, _T(".cbr")))
        return true;
#endif
    return false;
}

static bool IsImageFile(char *fileName)
{
    if (Str::EndsWithI(fileName, ".png"))
        return true;
    if (Str::EndsWithI(fileName, ".jpg") || Str::EndsWithI(fileName, ".jpeg"))
        return true;
    return false;
}

// HGLOBAL must be allocated with GlobalAlloc(GMEM_MOVEABLE, ...)
Bitmap *BitmapFromHGlobal(HGLOBAL mem)
{
    IStream *stream = NULL;
    Bitmap *bmp = NULL;

    GlobalLock(mem); // not sure if needed

    if (CreateStreamOnHGlobal(mem, FALSE, &stream) != S_OK)
        goto Exit;

    bmp = Bitmap::FromStream(stream);
    stream->Release();
    if (!bmp)
        goto Exit;

    if (bmp->GetLastStatus() != Ok)
    {
        delete bmp;
        bmp = NULL;
    }
Exit:
    GlobalUnlock(mem);
    return bmp;
}

ComicBookPage *LoadCurrentComicBookPage(unzFile& uf)
{
    char fileName[MAX_PATH];
    unz_file_info64 finfo;
    ComicBookPage *page = NULL;
    HGLOBAL data = NULL;
    int readBytes;

    int err = unzGetCurrentFileInfo64(uf, &finfo, fileName, dimof(fileName), NULL, 0, NULL, 0);
    if (err != UNZ_OK)
        return NULL;

    if (!IsImageFile(fileName))
        return NULL;

    err = unzOpenCurrentFilePassword(uf, NULL);
    if (err != UNZ_OK)
        return NULL;

    unsigned len = (unsigned)finfo.uncompressed_size;
    ZPOS64_T len2 = len;
    if (len2 != finfo.uncompressed_size) // overflow check
        goto Exit;

    data = GlobalAlloc(GMEM_MOVEABLE, len);
    if (!data)
        goto Exit;

    void *buf = GlobalLock(data);
    readBytes = unzReadCurrentFile(uf, buf, len);
    GlobalUnlock(data);

    if ((unsigned)readBytes != len)
        goto Exit;

    Bitmap *bmp = BitmapFromHGlobal(data);
    if (!bmp)
        goto Exit;

    page = new ComicBookPage(bmp);

Exit:
    err = unzCloseCurrentFile(uf);
    // ignoring the error

    GlobalFree(data);
    return page;
}

// TODO: similar to WindowInfo_CreateEmpty(), extract common parts
WindowInfo* CreateEmptyComicBookWindow()
{
    RectI windowPos;
    if (gGlobalPrefs.m_windowPos.IsEmpty()) {
        // center the window on the primary monitor
        RECT workArea;
        SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0);
        windowPos.y = workArea.top;
        windowPos.dy = RectDy(&workArea);
        windowPos.dx = (int)min(windowPos.dy * DEF_PAGE_RATIO, RectDx(&workArea));
        windowPos.x = (RectDx(&workArea) - windowPos.dx) / 2;
    }
    else {
        windowPos = gGlobalPrefs.m_windowPos;
        EnsureWindowVisibility(&windowPos);
    }

    HWND hwndFrame = CreateWindow(
            FRAME_CLASS_NAME, SUMATRA_WINDOW_TITLE,
            WS_OVERLAPPEDWINDOW,
            windowPos.x, windowPos.y, windowPos.dx, windowPos.dy,
            NULL, NULL,
            ghinst, NULL);
    if (!hwndFrame)
        return NULL;

    assert(NULL == FindWindowInfoByHwnd(hwndFrame));
    WindowInfo *win = new WindowInfo(hwndFrame);

    HWND hwndCanvas = CreateWindowEx(
            WS_EX_STATICEDGE, 
            CANVAS_CLASS_NAME, NULL,
            WS_CHILD | WS_HSCROLL | WS_VSCROLL,
            0, 0, 0, 0, /* position and size determined in OnSize */
            hwndFrame, NULL,
            ghinst, NULL);
    if (!hwndCanvas)
        return NULL;
    // hide scrollbars to avoid showing/hiding on empty window
    ShowScrollBar(hwndCanvas, SB_BOTH, FALSE);
    win->menu = BuildMenu(win->hwndFrame); // TODO: probably BuildComicBookMenu()

    win->hwndCanvas = hwndCanvas;
    ShowWindow(win->hwndCanvas, SW_SHOW);
    UpdateWindow(win->hwndCanvas);

    win->hwndInfotip = CreateWindowEx(WS_EX_TOPMOST,
        TOOLTIPS_CLASS, NULL, WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        win->hwndCanvas, NULL, ghinst, NULL);

    CreateToolbar(win, ghinst); // TODO: a different toolbar for a comic book
    DragAcceptFiles(win->hwndCanvas, TRUE);

    gWindows.Append(win);
    return win;
}

static Vec<ComicBookPage*> *LoadComicBookPages(const TCHAR *filePath)
{
    zlib_filefunc64_def ffunc;
    unzFile uf;
    fill_win32_filefunc64(&ffunc);

    ScopedGdiPlus gdiPlus;

    uf = unzOpen2_64(filePath, &ffunc);
    if (!uf)
        goto Exit;

    unz_global_info64 ginfo;
    int err = unzGetGlobalInfo64(uf, &ginfo);
    if (err != UNZ_OK)
        goto Exit;

    // extract all contained files one by one
    Vec<ComicBookPage*> *pages = new Vec<ComicBookPage *>(256);

    unzGoToFirstFile(uf);

    // Note: maybe lazy loading would be beneficial (but at least I would
    // need to parse image headers to extract width/height information)
    for (int n = 0; n < ginfo.number_entry; n++) {
        ComicBookPage *page = LoadCurrentComicBookPage(uf);
        if (page)
            pages->Append(page);
        err = unzGoToNextFile(uf);
        if (err != UNZ_OK)
            break;
    }

Exit:
    if (uf)
        unzClose(uf);
    if (pages && pages->Count() == 0)
    {
        delete pages;
        return NULL;
    }
    return pages;

}

static bool LoadComicBookIntoWindow(
    const TCHAR *filePath,
    WindowInfo *win,
    const DisplayState *state,
    bool isNewWindow,    // if true then 'win' refers to a newly created window that needs to be resized and placed
    bool showWin,          // window visible or not
    bool placeWindow)      // if true then the Window will be moved/sized according to the 'state' information even if the window was already placed before (isNewWindow=false)
{
    Vec<ComicBookPage *> *pages = NULL;

    // Never load settings from a preexisting state if the user doesn't wish to
    // (unless we're just refreshing the document, i.e. only if placeWindow == true)
    if (placeWindow && (gGlobalPrefs.m_globalPrefsOnly || state && state->useGlobalValues))
        state = NULL;
    DisplayMode displayMode = gGlobalPrefs.m_defaultDisplayMode;
    size_t startPage = 1;
    ScrollState ss(1, -1, -1);
    bool showAsFullScreen = WIN_STATE_FULLSCREEN == gGlobalPrefs.m_windowState;
    int showType = gGlobalPrefs.m_windowState == WIN_STATE_MAXIMIZED || showAsFullScreen ? SW_MAXIMIZE : SW_NORMAL;

    if (state) {
        startPage = state->pageNo;
        displayMode = state->displayMode;
        showAsFullScreen = WIN_STATE_FULLSCREEN == state->windowState;
        if (state->windowState == WIN_STATE_NORMAL)
            showType = SW_NORMAL;
        else if (state->windowState == WIN_STATE_MAXIMIZED || showAsFullScreen)
            showType = SW_MAXIMIZE;
        else if (state->windowState == WIN_STATE_MINIMIZED)
            showType = SW_MINIMIZE;
    }
    /* TODO: need to get rid of that, but not sure if that won't break something
       i.e. UpdateCanvasSize() caches size of canvas and some code might depend
       on this being a cached value, not the real value at the time of calling */
    win->UpdateCanvasSize();

    free(win->loadedFilePath);
    win->loadedFilePath = Str::Dup(filePath);
    
    pages = LoadComicBookPages(filePath);
    if (!pages)
        goto Error;

    win->comicPages = pages;
    float zoomVirtual = gGlobalPrefs.m_defaultZoom;
    int rotation = DEFAULT_ROTATION;

    if (state) {
        if (startPage >= 1 && startPage <= pages->Count()) {
            ss.page = startPage;
            if (ZOOM_FIT_CONTENT != state->zoomVirtual) {
                ss.x = state->scrollPos.x;
                ss.y = state->scrollPos.y;
            }
            // else relayout scroll to fit the page (again)
        } else if (startPage > pages->Count())
            ss.page = pages->Count();
        zoomVirtual = state->zoomVirtual;
        rotation = state->rotation;
    }
    // TODO: layout

    if (!isNewWindow)
        win->RedrawAll();


    const TCHAR *title = Path::GetBaseName(win->loadedFilePath);
    Win::SetText(win->hwndFrame, title);

Error:
    if (isNewWindow || placeWindow && state) {
        assert(win);
        if (isNewWindow && state && !state->windowPos.IsEmpty()) {
            RectI rect = state->windowPos;
            // Make sure it doesn't have a position like outside of the screen etc.
            rect_shift_to_work_area(&rect.ToRECT(), FALSE);
            // This shouldn't happen until !win->IsAboutWindow(), so that we don't
            // accidentally update gGlobalState with this window's dimensions
            MoveWindow(win->hwndFrame, rect.x, rect.y, rect.dx, rect.dy, TRUE);
        }

        if (showWin) {
            ShowWindow(win->hwndFrame, showType);
        }
        UpdateWindow(win->hwndFrame);
    }

//    if (win->ComicBookLoaded())
//        win->dm->setScrollState(&ss);

    UpdateToolbarAndScrollbarsForAllWindows();
    if (!win->ComicBookLoaded()) {
        win->RedrawAll();
        return false;
    }
    // This should only happen after everything else is ready
    if ((isNewWindow || placeWindow) && showWin && showAsFullScreen)
        WindowInfo_EnterFullscreen(win);

//    if (!isNewWindow && win->presentation && win->dm)
//        win->dm->setPresentationMode(true);

    return true;
}

// Load *.cbz / *.cbr file
// TODO: far from being done
WindowInfo *LoadComicBook(const TCHAR *fileName, WindowInfo *win, bool showWin)
{
    ScopedMem<TCHAR> filePath(Path::Normalize(fileName));
    if (!filePath)
        return win;

    bool isNewWindow = false;
    if (!win && 1 == gWindows.Count() && gWindows[0]->IsAboutWindow()) {
        win = gWindows[0];
    } if (!win || win->PdfLoaded()) { // TODO: PdfLoaded probably a bad check
        win = CreateEmptyComicBookWindow();
        if (!win)
            return NULL;
        isNewWindow = true;
    }

    DisplayState *ds = gFileHistory.Find(filePath);
    if (ds) {
        AdjustRemovableDriveLetter(filePath);
        CheckPositionAndSize(ds);
    }

    if (!LoadComicBookIntoWindow(filePath, win, ds, isNewWindow, showWin, true)) {
        /* failed to open */
        gFileHistory.MarkFileInexistent(filePath);
        return win;
    }

    if (gGlobalPrefs.m_rememberOpenedFiles)
        gFileHistory.MarkFileLoaded(filePath);

    // Add the file also to Windows' recently used documents (this doesn't
    // happen automatically on drag&drop, reopening from history, etc.)
    SHAddToRecentDocs(SHARD_PATH, filePath);
    return win;
}

void DrawComicBook(HWND hwnd, HDC hdc, RectI rect)
{
    HFONT fontRightTxt = Win32_Font_GetSimple(hdc, _T("MS Shell Dlg"), 14);
    HGDIOBJ origFont = SelectObject(hdc, fontRightTxt); /* Just to remember the orig font */
    SetBkMode(hdc, TRANSPARENT);
    RECT r = rect.ToRECT();
    FillRect(hdc, &r, gBrushBg);
    DrawCenteredText(hdc, rect, _T("Will show comic books"));
    SelectObject(hdc, origFont);
    Win32_Font_Delete(fontRightTxt);
}

static void OnPaint(WindowInfo *win)
{
    ClientRect rc(win->hwndCanvas);
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(win->hwndCanvas, &ps);
    win->ResizeIfNeeded(false);
    DrawComicBook(win->hwndCanvas, win->hdcToDraw, rc);
    win->DoubleBuffer_Show(hdc);
    EndPaint(win->hwndCanvas, &ps);
}

LRESULT HandleWindowComicBookMsg(WindowInfo *win, HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, bool& handled)
{
    assert(win->ComicBookLoaded());
    handled = false;

    switch (message)
    {
        case WM_PAINT:
            OnPaint(win);
            handled = true;
            return 0;
    }

    return 0;
}


