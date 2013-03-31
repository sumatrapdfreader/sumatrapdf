/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "EbookWindow.h"

#include "AppPrefs.h"
#include "AppTools.h"
#include "BaseEngine.h"
#include "EbookController.h"
#include "EbookControls.h"
#include "EbookDoc.h"
#include "EbookFormatter.h"
#include "ExternalPdfViewer.h"
#include "FileHistory.h"
using namespace Gdiplus;
#include "GdiPlusUtil.h"
#include "Menu.h"
#include "MobiDoc.h"
#include "Resource.h"
#include "SumatraAbout.h"
#include "SumatraDialogs.h"
#include "SumatraPDF.h"
#include "SumatraProperties.h"
#include "Touch.h"
#include "Translations.h"
#include "WindowInfo.h"
#include "WinUtil.h"

#define MOBI_FRAME_CLASS_NAME    L"SUMATRA_MOBI_FRAME"

#define WIN_DX    720
#define WIN_DY    640

static bool gShowTextBoundingBoxes = false;

const WCHAR *EbookWindow::LoadedFilePath() const
{
    if (ebookController)
        return ebookController->GetDoc().GetFilePath();
    return NULL;
}

static EbookWindow* FindEbookWindowByHwnd(HWND hwnd)
{
    for (EbookWindow **w = gEbookWindows.IterStart(); w; w = gEbookWindows.IterNext()) {
        if ((*w)->hwndFrame == hwnd)
            return *w;
    }
    return NULL;
}

EbookWindow* FindEbookWindowByController(EbookController *controller)
{
    for (EbookWindow **w = gEbookWindows.IterStart(); w; w = gEbookWindows.IterNext()) {
        if ((*w)->ebookController == controller)
            return *w;
    }
    return NULL;
}

#define LAYOUT_TIMER_ID 1

void RestartLayoutTimer(EbookController *controller)
{
    EbookWindow *win = FindEbookWindowByController(controller);
    KillTimer(win->hwndFrame, LAYOUT_TIMER_ID);
    SetTimer(win->hwndFrame,  LAYOUT_TIMER_ID, 600, NULL);
}

static void OnTimer(EbookWindow *win, WPARAM timerId)
{
    (void)timerId;
    CrashIf(timerId != LAYOUT_TIMER_ID);
    KillTimer(win->hwndFrame, LAYOUT_TIMER_ID);
    win->ebookController->OnLayoutTimer();
}

#if defined(SHOW_DEBUG_MENU_ITEMS) || defined(DEBUG)
static void OnToggleBbox(EbookWindow *win)
{
    gShowTextBoundingBoxes = !gShowTextBoundingBoxes;
    SetDebugPaint(gShowTextBoundingBoxes);
    win->ebookControls->mainWnd->RequestRepaint();
    InvalidateRect(win->hwndFrame, NULL, TRUE);
    win::menu::SetChecked(GetMenu(win->hwndFrame), IDM_DEBUG_SHOW_LINKS, gShowTextBoundingBoxes);
}
#endif

// closes a physical window, deletes the EbookWindow object and removes it
// from the global list of windows
void DeleteEbookWindow(EbookWindow *win, bool forceDelete)
{
    if (gPluginMode && !forceDelete)
        return;

    UpdateCurrentFileDisplayStateForWin(SumatraWindow::Make(win));

    DeletePropertiesWindow(win->hwndFrame);
    delete win->ebookController;
    DestroyEbookControls(win->ebookControls);
    gEbookWindows.Remove(win);
    HWND toDestroy = win->hwndFrame;
    delete win;
    // must be called after removing win from gEbookWindows so that window
    // message processing doesn't pick up a window being destroyed
    DestroyWindow(toDestroy);
}

// if forceClose is true, we force window deletion in plugin mode
// if quitIfLast is true, we quit if we closed the last window, otherwise
// we create an about window
static void CloseEbookWindow(EbookWindow *win, bool quitIfLast, bool forceClose)
{
    DeleteEbookWindow(win, forceClose);
    if (TotalWindowsCount() > 0)
        return;
    if (quitIfLast) {
        // TODO: the way we call SavePrefs() is all over the place. More principled approach would be better
        SavePrefs();
        PostQuitMessage(0);
        return;
    }
    WindowInfo *w = CreateAndShowWindowInfo();
    if (!w) {
        PostQuitMessage(0);
        return;
    }
}

static LRESULT OnMouseWheel(EbookWindow *win, UINT, WPARAM wParam, LPARAM)
{
    short delta = GET_WHEEL_DELTA_WPARAM(wParam);
    if (delta > 0)
        win->ebookController->AdvancePage(-1);
    else
        win->ebookController->AdvancePage(1);
    return 0;
}

static void OnMenuGoToPage(EbookWindow *win);

static LRESULT OnKeyDown(EbookWindow *win, UINT msg, WPARAM key, LPARAM lParam)
{
    switch (key) {
    case VK_LEFT: case VK_UP: case VK_PRIOR: case 'P':
        win->ebookController->AdvancePage(-1);
        break;
    case VK_RIGHT: case VK_DOWN: case VK_NEXT: case 'N':
        win->ebookController->AdvancePage(1);
        break;
    case VK_SPACE:
        win->ebookController->AdvancePage(IsShiftPressed() ? -1 : 1);
        break;
#ifdef DEBUG
    case VK_F1:
        OnToggleBbox(win);
        break;
#endif
    case VK_HOME:
        win->ebookController->GoToPage(1);
        break;
    case VK_END:
        win->ebookController->GoToLastPage();
        break;
    case 'Q':
        CloseEbookWindow(win, true, true);
        break;
    case 'G':
        OnMenuGoToPage(win);
        break;
    case VK_ESCAPE:
        if (gGlobalPrefs.escToExit)
            CloseEbookWindow(win, true, true);
        break;
    default:
        return DefWindowProc(win->hwndFrame, msg, key, lParam);
    }
    return 0;
}

static void RebuildMenuBarForEbookWindow(EbookWindow *win)
{
    HMENU oldMenu = GetMenu(win->hwndFrame);
    HMENU newMenu = BuildMenu(win);
#if 0 // TODO: support fullscreen mode when we have it
    if (!win->presentation && !win->fullScreen)
        SetMenu(win->hwndFrame, win->menu);
#endif
    SetMenu(win->hwndFrame, newMenu);
    DestroyMenu(oldMenu);
}

void RebuildMenuBarForEbookWindows()
{
    for (size_t i = 0; i < gEbookWindows.Count(); i++) {
        RebuildMenuBarForEbookWindow(gEbookWindows.At(i));
    }
}

static LRESULT OnGesture(EbookWindow *win, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (!Touch::SupportsGestures())
        return DefWindowProc(win->hwndFrame, message, wParam, lParam);

    HGESTUREINFO hgi = (HGESTUREINFO)lParam;
    GESTUREINFO gi = { 0 };
    gi.cbSize = sizeof(GESTUREINFO);

    BOOL ok = Touch::GetGestureInfo(hgi, &gi);
    if (!ok) {
        Touch::CloseGestureInfoHandle(hgi);
        return 0;
    }

    switch (gi.dwID) {
        case GID_ZOOM:
            win->touchState.startArg = LODWORD(gi.ullArguments);
            break;

        case GID_PAN:
            // Flicking left or right changes the page,
            // panning moves the document in the scroll window
            if (gi.dwFlags == GF_BEGIN) {
                win->touchState.panStarted = true;
                win->touchState.panPos = gi.ptsLocation;
            } else if (win->touchState.panStarted) {
                int deltaX = win->touchState.panPos.x - gi.ptsLocation.x;
                int deltaY = win->touchState.panPos.y - gi.ptsLocation.y;
                win->touchState.panPos = gi.ptsLocation;

                if ((gi.dwFlags & GF_INERTIA) && abs(deltaX) > abs(deltaY)) {
                    // Switch pages once we hit inertia in a horizontal direction
                    if (deltaX < 0)
                        win->ebookController->AdvancePage(-1);
                    else if (deltaX > 0)
                        win->ebookController->AdvancePage(1);
                    win->touchState.panStarted = false;
                }
            }
            break;

        case GID_TWOFINGERTAP:
            // Two-finger tap toggles fullscreen mode
            // TODO: write me when we have fullscreen mode
            break;

        case GID_PRESSANDTAP:
            // in engine window toggles between Fit Page, Fit Width and Fit Content (same as 'z')
            // TODO: should we do something here?
            break;

        default:
            // A gesture was not recognized
            break;
    }

    Touch::CloseGestureInfoHandle(hgi);
    return 0;
}

#ifdef SHOW_DEBUG_MENU_ITEMS
static void OnLoadMobiSample(EbookWindow *win)
{
    HRSRC resSrc = FindResource(ghinst, MAKEINTRESOURCE(IDD_SAMPLE_MOBI), RT_RCDATA);
    CrashIf(!resSrc);
    HGLOBAL res = LoadResource(NULL, resSrc);
    CrashIf(!res);
    MobiTestDoc *doc = new MobiTestDoc((const char *)LockResource(res), SizeofResource(NULL, resSrc));
    CrashIf(!doc || 0 == doc->GetBookHtmlSize());
    UnlockResource(res);
    win->ebookController->SetDoc(Doc(doc));
}
#endif

static void OnMenuGoToPage(EbookWindow *win)
{
    int pageNo = win->ebookController->GetCurrentPageNo();
    int maxPage = (int)win->ebookController->GetMaxPageCount();
    ScopedMem<WCHAR> label(str::Format(L"%d", pageNo));
    ScopedMem<WCHAR> newPageStr(Dialog_GoToPage(win->hwndFrame, label, maxPage));
    if (!newPageStr)
        return;

    int newPageNo = 0;
    str::Parse(newPageStr, L"%d", &newPageNo);
    if (newPageNo > 0)
        win->ebookController->GoToPage(newPageNo);
}

static LRESULT OnCommand(EbookWindow *win, UINT msg, WPARAM wParam, LPARAM lParam)
{
    CrashIf(!win);
    int wmId = LOWORD(wParam);

    // check if the menuId belongs to an entry in the list of
    // recently opened files and load the referenced file if it does
    if ((wmId >= IDM_FILE_HISTORY_FIRST) && (wmId <= IDM_FILE_HISTORY_LAST))
    {
        DisplayState *state = gFileHistory.Get(wmId - IDM_FILE_HISTORY_FIRST);
        if (state && HasPermission(Perm_DiskAccess))
            LoadDocument2(state->filePath, SumatraWindow::Make(win));
        return 0;
    }

    if (IDM_OPEN_WITH_EXTERNAL_FIRST <= wmId && wmId <= IDM_OPEN_WITH_EXTERNAL_LAST)
    {
        ViewWithExternalViewer(wmId - IDM_OPEN_WITH_EXTERNAL_FIRST, win->LoadedFilePath());
        return 0;
    }

    switch (wmId)
    {
        case IDM_OPEN:
        case IDT_FILE_OPEN:
            OnMenuOpen(SumatraWindow::Make(win));
            break;

        case IDT_FILE_EXIT:
        case IDM_CLOSE:
            CloseEbookWindow(win, false, false);
            break;

        case IDM_EXIT:
            OnMenuExit();
            break;

        case IDM_GOTO_NEXT_PAGE:
            win->ebookController->AdvancePage(1);
            break;

        case IDM_GOTO_PREV_PAGE:
            win->ebookController->AdvancePage(-1);
            break;

        case IDM_GOTO_FIRST_PAGE:
            win->ebookController->GoToPage(1);
            break;

        case IDM_GOTO_LAST_PAGE:
            win->ebookController->GoToLastPage();
            break;

        case IDM_CHANGE_LANGUAGE:
            OnMenuChangeLanguage(win->hwndFrame);
            break;

#if 0
        case IDM_VIEW_BOOKMARKS:
            ToggleTocBox(win);
            break;
#endif

        case IDM_GOTO_PAGE:
            OnMenuGoToPage(win);
            break;

        case IDM_VISIT_WEBSITE:
            LaunchBrowser(WEBSITE_MAIN_URL);
            break;

        case IDM_MANUAL:
            LaunchBrowser(WEBSITE_MANUAL_URL);
            break;

#ifdef SHOW_DEBUG_MENU_ITEMS
        case IDM_DEBUG_SHOW_LINKS:
            OnToggleBbox(win);
            break;

        case IDM_LOAD_MOBI_SAMPLE:
            OnLoadMobiSample(win);
            break;

        case IDM_DEBUG_EBOOK_UI:
            gUserPrefs.traditionalEbookUI = !gUserPrefs.traditionalEbookUI;
            win::menu::SetChecked(GetMenu(win->hwndFrame), IDM_DEBUG_EBOOK_UI, gUserPrefs.traditionalEbookUI);
            DebugAlternateChmEngine(gUserPrefs.traditionalEbookUI);
            break;

        case IDM_DEBUG_MUI:
            SetDebugPaint(!IsDebugPaint());
            win::menu::SetChecked(GetMenu(win->hwndFrame), IDM_DEBUG_MUI, !IsDebugPaint());
            break;
#endif

        case IDM_ABOUT:
            OnMenuAbout();
            break;

        case IDM_CHECK_UPDATE:
            AutoUpdateCheckAsync(win->hwndFrame, false);
            break;

        case IDM_SETTINGS:
            OnMenuSettings(win->hwndFrame);
            break;

        case IDM_PROPERTIES:
            OnMenuProperties(SumatraWindow::Make(win));
            break;

        default:
            return DefWindowProc(win->hwndFrame, msg, wParam, lParam);
    }

    return 0;
}

static LRESULT CALLBACK MobiWndProcFrame(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    // messages that don't require EbookWindow
    switch (msg)
    {
        case WM_DROPFILES:
            OnDropFiles((HDROP)wParam);
            break;

        // if we return 0, during WM_PAINT we can check
        // PAINTSTRUCT.fErase to see if WM_ERASEBKGND
        // was sent before WM_PAINT
        case WM_ERASEBKGND:
            return 0;
    }

    // messages that do require EbookWindow
    EbookWindow *win = FindEbookWindowByHwnd(hwnd);
    if (!win)
        return DefWindowProc(hwnd, msg, wParam, lParam);

    bool wasHandled;
    LRESULT res = win->hwndWrapper->evtMgr->OnMessage(msg, wParam, lParam, wasHandled);
    if (wasHandled)
        return res;

    switch (msg)
    {
        case WM_DESTROY:
            // called by windows if user clicks window's close button or if
            // we call DestroyWindow()
            CloseEbookWindow(win, true, true);
            break;

        case WM_PAINT:
            win->hwndWrapper->OnPaint(hwnd);
            break;

        case WM_KEYDOWN:
            return OnKeyDown(win, msg, wParam, lParam);

        case WM_MOUSEWHEEL:
            return OnMouseWheel(win, msg, wParam, lParam);

        case WM_COMMAND:
            return OnCommand(win, msg, wParam, lParam);
            break;

        case WM_INITMENUPOPUP:
            UpdateMenu(win, (HMENU)wParam);
            break;

        case WM_GESTURE:
            return OnGesture(win, msg, wParam, lParam);

        case WM_TIMER:
            OnTimer(win, wParam);
            break;

        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    return 0;
}

RenderedBitmap *RenderFirstDocPageToBitmap(Doc doc, SizeI pageSize, SizeI bmpSize, int border)
{
    PoolAllocator textAllocator;
    HtmlFormatterArgs *args = CreateFormatterArgsDoc(doc, pageSize.dx - 2 * border, pageSize.dy - 2 * border, &textAllocator);
    HtmlFormatter *formatter = CreateFormatter(doc, args);
    HtmlPage *pd = formatter->Next();
    delete formatter;
    delete args;
    if (!pd)
        return NULL;

    Bitmap pageBmp(pageSize.dx, pageSize.dy, PixelFormat24bppRGB);
    Graphics g(&pageBmp);
    Rect r(0, 0, pageSize.dx, pageSize.dy);
    r.Inflate(1, 1);
    SolidBrush br(Color(255, 255, 255));
    g.FillRectangle(&br, r);

    DrawHtmlPage(&g, &pd->instructions, (REAL)border, (REAL)border, false, &Color(Color::Black));
    delete pd;

    Bitmap res(bmpSize.dx, bmpSize.dy, PixelFormat24bppRGB);
    Graphics g2(&res);
    g2.SetInterpolationMode(InterpolationModeHighQualityBicubic);
    g2.DrawImage(&pageBmp, Rect(0, 0, bmpSize.dx, bmpSize.dy),
                 0, 0, pageSize.dx, pageSize.dy, UnitPixel);

    HBITMAP hbmp;
    Status ok = res.GetHBITMAP(Color::White, &hbmp);
    if (ok != Ok)
        return NULL;
    return new RenderedBitmap(hbmp, bmpSize);
}

static RenderedBitmap *ThumbFromCoverPage(Doc doc)
{
    ImageData *coverImage = doc.GetCoverImage();
    if (!coverImage)
        return NULL;
    Bitmap *coverBmp = BitmapFromData(coverImage->data, coverImage->len);
    if (!coverBmp)
        return NULL;

    Bitmap res(THUMBNAIL_DX, THUMBNAIL_DY, PixelFormat24bppRGB);
    float scale = (float)THUMBNAIL_DX / (float)coverBmp->GetWidth();
    int fromDy = THUMBNAIL_DY;
    if (scale < 1.f)
        fromDy = (int)((float)coverBmp->GetHeight() * scale);
    Graphics g(&res);
    g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
    g.DrawImage(coverBmp, Rect(0, 0, THUMBNAIL_DX, THUMBNAIL_DY),
        0, 0, coverBmp->GetWidth(), fromDy, UnitPixel);
    HBITMAP hbmp;
    Status ok = res.GetHBITMAP(Color::White, &hbmp);
    delete coverBmp;
    if (ok == Ok)
        return new RenderedBitmap(hbmp, SizeI(THUMBNAIL_DX, THUMBNAIL_DY));
    return NULL;
}

static void CreateThumbnailForDoc(Doc doc, DisplayState& ds)
{
    CrashIf(!doc.AsEpub() && !doc.AsFb2() && !doc.AsMobi());

    if (!ShouldSaveThumbnail(ds))
        return;

    // if there is cover image, we use it to generate thumbnail by scaling
    // image width to thumbnail dx, scaling height proportionally and using
    // as much of it as fits in thumbnail dy
    RenderedBitmap *bmp = ThumbFromCoverPage(doc);
    if (!bmp) {
        // no cover image so generate thumbnail from first page
        SizeI pageSize(THUMBNAIL_DX * 3, THUMBNAIL_DY * 3);
        SizeI dstSize(THUMBNAIL_DX, THUMBNAIL_DY);
        bmp = RenderFirstDocPageToBitmap(doc, pageSize, dstSize, 10);
    }
    SaveThumbnailForFile(doc.GetFilePath(), bmp);
}

void OpenMobiInWindow(Doc doc, SumatraWindow& winToReplace)
{
    const WCHAR *fullPath = doc.GetFilePath();
    DisplayState *ds = gFileHistory.Find(fullPath);

    if (doc.IsNone()) {
        // TODO: a hack. In LoadDocumentOld(), if current window IsAboutWindow(),
        // we set loadedFilePath to prevent a crash if multiple ebook files are
        // dropped on about window. We need to undo that in case of failure
        // or else the window will be stuck in an invalid state (not about window
        // but not a document window either)
        WindowInfo *w = winToReplace.AsWindowInfo();
        if (str::Eq(w->loadedFilePath, doc.GetFilePath())) {
            free(w->loadedFilePath);
            w->loadedFilePath = NULL;
            // this is now about window. We don't want to show it if we're already
            // showing other windows (the scenario: dragging ebook file on a window
            // showing a document, we create invisible window for this document and
            // we don't want to show empty window if loading fails
            if (gEbookWindows.Count() > 0 || gWindows.Count() > 1) {
                CloseWindow(w, false);
                if (gFileHistory.MarkFileInexistent(fullPath))
                    SavePrefs();
                // TODO: notify the use that loading failed (e.g. show a notification)
                return;
            }
        }
        // TODO: notify the user that loading failed (e.g. show a notification)

        // TODO: this is not a great solution. In case of opening
        // file from cmd-line, we create a window but don't show it
        // if loading afils, we have to show the window. If window
        // is already visible, this is a no-op
        ShowWindow(winToReplace.HwndFrame(), SW_SHOW);
        if (gFileHistory.MarkFileInexistent(fullPath))
            SavePrefs();
        return;
    }

    if (gGlobalPrefs.rememberOpenedFiles) {
        ds = gFileHistory.MarkFileLoaded(fullPath);
        if (gGlobalPrefs.showStartPage && ds) {
            // TODO: do it on a background thread?
            CreateThumbnailForDoc(doc, *ds);
        }
        SavePrefs();
    }

    int startReparseIdx = -1;
    if (ds)
        startReparseIdx = ds->reparseIdx;

    // Add the file also to Windows' recently used documents (this doesn't
    // happen automatically on drag&drop, reopening from history, etc.)
    if (HasPermission(Perm_DiskAccess) && !gPluginMode)
        SHAddToRecentDocs(SHARD_PATH, fullPath);

    ScopedMem<WCHAR> winTitle(str::Format(L"%s - %s", path::GetBaseName(fullPath), SUMATRA_WINDOW_TITLE));

    if (winToReplace.AsEbookWindow()) {
        EbookWindow *mw = winToReplace.AsEbookWindow();
        CrashIf(!mw);
        mw->ebookController->SetDoc(doc);
        win::SetText(mw->hwndFrame, winTitle);
        // TODO: if we have window position/last position for this file, restore it
        return;
    }

    RectI windowPos = gGlobalPrefs.windowPos;
    if (!windowPos.IsEmpty())
        EnsureAreaVisibility(windowPos);
    else
        windowPos = GetDefaultWindowPos();

    if (ds && !ds->windowPos.IsEmpty()) {
        // Make sure it doesn't have a position like outside of the screen etc.
        windowPos = ShiftRectToWorkArea(ds->windowPos);
    }

    WindowInfo *winInfo = winToReplace.AsWindowInfo();
    bool wasMaximized = false;
    if (winInfo && winInfo->hwndFrame) {
        wasMaximized = IsZoomed(winInfo->hwndFrame);
        // hide the window instead of destroying it so that
        // windows doesn't set a window from a different
        // process as foreground window instead of the once
        // created below
        ShowWindow(winInfo->hwndFrame, SW_HIDE);
    }

    HWND hwnd = CreateWindow(
            MOBI_FRAME_CLASS_NAME, SUMATRA_WINDOW_TITLE,
            WS_OVERLAPPEDWINDOW,
            windowPos.x, windowPos.y, windowPos.dx, windowPos.dy,
            NULL, NULL,
            ghinst, NULL);
    if (!hwnd) {
        CloseDocumentAndDeleteWindowInfo(winInfo);
        return;
    }
    if (HasPermission(Perm_DiskAccess) && !gPluginMode)
        DragAcceptFiles(hwnd, TRUE);
    if (Touch::SupportsGestures()) {
        GESTURECONFIG gc = { 0, GC_ALLGESTURES, 0 };
        Touch::SetGestureConfig(hwnd, 0, 1, &gc, sizeof(GESTURECONFIG));
    }

    EbookWindow *win = new EbookWindow();
    win->ebookControls = CreateEbookControls(hwnd);
    win->hwndWrapper = win->ebookControls->mainWnd;
    win->ebookController = new EbookController(win->ebookControls);
    win->hwndFrame = hwnd;

    gEbookWindows.Append(win);
    win::SetText(win->hwndFrame, winTitle);
    SetMenu(hwnd, BuildMenu(win));

    ShowWindow(hwnd, wasMaximized ? SW_SHOWMAXIMIZED : SW_SHOW);
    win->ebookController->SetDoc(doc, startReparseIdx);

    CloseDocumentAndDeleteWindowInfo(winInfo);
}

void RegisterMobiWinClass(HINSTANCE hinst)
{
    WNDCLASSEX  wcex;
    FillWndClassEx(wcex, hinst, MOBI_FRAME_CLASS_NAME, MobiWndProcFrame);
    // clear out CS_HREDRAW | CS_VREDRAW style so that resizing doesn't
    // cause the whole window redraw
    wcex.style          = 0;
    wcex.hIcon          = LoadIcon(hinst, MAKEINTRESOURCE(IDI_SUMATRAPDF));
    wcex.hbrBackground  = CreateSolidBrush(GetSysColor(COLOR_BTNFACE));
    RegisterClassEx(&wcex);
}

bool IsEbookFile(const WCHAR *fileName)
{
    return EpubDoc::IsSupportedFile(fileName) ||
           Fb2Doc::IsSupportedFile(fileName) ||
           MobiDoc::IsSupportedFile(fileName);
}

Doc GetDocForWindow(const SumatraWindow& win)
{
    if (win.AsWindowInfo()) {
        WindowInfo *iwin = win.AsWindowInfo();
        if (!iwin->IsDocLoaded())
            return Doc();
        return Doc(iwin->dm->engine, iwin->dm->engineType);
    }
    if (win.AsEbookWindow()) {
        EbookWindow *ewin = win.AsEbookWindow();
        return ewin->ebookController->GetDoc();
    }
    CrashIf(true);
    return Doc();
}
