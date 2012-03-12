#include "MobiWindow.h"

#include "AppTools.h"
#include "BaseEngine.h"
#include "EbookController.h"
#include "EbookControls.h"
#include "FileHistory.h"
#include "Menu.h"
#include "MobiDoc.h"
#include "PageLayout.h"
#include "Resource.h"
#include "SumatraProperties.h"
#include "SumatraAbout.h"
#include "SumatraPDF.h"
#include "Touch.h"
#include "Translations.h"
#include "WindowInfo.h"
#include "WinUtil.h"

#ifdef SHOW_DEBUG_MENU_ITEMS
// A sample text to display if we don't show an actual mobi file
static const char *gSampleHtml = "<html><p align=justify width=1em><b>ClearType</b>, is <b>dependent</b> on the <i>orientation &amp; ordering</i> of the LCD stripes and possibly some other things unknown.</p> <p align='right height=13pt'><em>Currently</em>, ClearType is implemented <hr><br/> only for vertical stripes that are ordered RGB.</p> <p align=center height=8pt>This might be a concern if you are using a tablet PC.</p> <p width='1em'>Where the display can be oriented in any direction, or if you are using a screen that can be turned from landscape to portrait. The <strike>following example</strike> draws text with two <u>different quality</u> settings.</p> <p width=1em>This is a paragraph that should take at least two lines. With study and discreet inquiries, Abagnale picked up airline jargon and discovered that pilots could ride free anywhere in the world on any airline; and that hotels billed airlines direct and cashed checks issued by airline companies.</p><br><p width=1em>    And this is another paragraph tha we wrote today. Hiding out in a southern city, Abagnale learned that the state attorney general was seeking assistants. For nine months he practiced law, but when a real Harvard lawyer appeared on the scene, Abagnale figured it was time to move on.</p> On to the <b>next<mbp:pagebreak>page</b><p>ThisIsAVeryVeryVeryLongWordThatShouldBeBrokenIntoMultiple lines</p><mbp:pagebreak><hr><mbp:pagebreak>blah<br>Foodo.<p>And me</p></html>";
#endif

#define MOBI_FRAME_CLASS_NAME    _T("SUMATRA_MOBI_FRAME")

#define WIN_DX    720
#define WIN_DY    640

static bool gShowTextBoundingBoxes = false;

static MenuDef menuDefMobiFile[] = {
    { _TRN("&Open\tCtrl+O"),                IDM_OPEN ,                  MF_REQ_DISK_ACCESS },
    { _TRN("&Close\tCtrl+W"),               IDM_CLOSE,                  MF_REQ_DISK_ACCESS },
    { SEP_ITEM,                             0,                          MF_REQ_DISK_ACCESS },
    { _TRN("E&xit\tCtrl+Q"),                IDM_EXIT,                   0 }
};

static MenuDef menuDefMobiGoTo[] = {
    { _TRN("&Next Page\tRight Arrow"),      IDM_GOTO_NEXT_PAGE,         0 },
    { _TRN("&Previous Page\tLeft Arrow"),   IDM_GOTO_PREV_PAGE,         0 },
    { _TRN("&First Page\tHome"),            IDM_GOTO_FIRST_PAGE,        0 },
    { _TRN("&Last Page\tEnd"),              IDM_GOTO_LAST_PAGE,         0 },
};

static MenuDef menuDefMobiSettings[] = {
    { _TRN("Change Language"),              IDM_CHANGE_LANGUAGE,        0  },
    { _TRN("&Options..."),                  IDM_SETTINGS,               MF_REQ_PREF_ACCESS },
};

static MenuDef menuDefHelp[] = {
    { _TRN("Visit &Website"),               IDM_VISIT_WEBSITE,          MF_REQ_DISK_ACCESS },
    { _TRN("&Manual"),                      IDM_MANUAL,                 MF_REQ_DISK_ACCESS },
    { _TRN("Check for &Updates"),           IDM_CHECK_UPDATE,           MF_REQ_INET_ACCESS },
    { SEP_ITEM,                             0,                          MF_REQ_DISK_ACCESS },
    { _TRN("&About"),                       IDM_ABOUT,                  0 },
};

#ifdef SHOW_DEBUG_MENU_ITEMS
static MenuDef menuDefDebug[] = {
    { "Show bbox",                          IDM_DEBUG_SHOW_LINKS,       MF_NO_TRANSLATE },
    { "Test page layout",                   IDM_DEBUG_PAGE_LAYOUT,      MF_NO_TRANSLATE },
    { "Toggle ebook UI",                    IDM_DEBUG_EBOOK_UI,         MF_NO_TRANSLATE },
};
#endif

static void RebuildFileMenuForMobiWindow(HMENU menu)
{
    win::menu::Empty(menu);
    BuildMenuFromMenuDef(menuDefMobiFile, dimof(menuDefMobiFile), menu, false);
    AppendRecentFilesToMenu(menu);
}

static HMENU BuildMobiMenu()
{
    HMENU mainMenu = CreateMenu();
    HMENU m = CreateMenu();
    RebuildFileMenuForMobiWindow(m);

    AppendMenu(mainMenu, MF_POPUP | MF_STRING, (UINT_PTR)m, _TR("&File"));
    m = BuildMenuFromMenuDef(menuDefMobiGoTo, dimof(menuDefMobiGoTo), CreateMenu(), false);
    AppendMenu(mainMenu, MF_POPUP | MF_STRING, (UINT_PTR)m, _TR("&Go To"));
    m = BuildMenuFromMenuDef(menuDefMobiSettings, dimof(menuDefMobiSettings), CreateMenu());
    AppendMenu(mainMenu, MF_POPUP | MF_STRING, (UINT_PTR)m, _TR("&Settings"));
    m = BuildMenuFromMenuDef(menuDefHelp, dimof(menuDefHelp), CreateMenu());
    AppendMenu(mainMenu, MF_POPUP | MF_STRING, (UINT_PTR)m, _TR("&Help"));
#ifdef SHOW_DEBUG_MENU_ITEMS
    m = BuildMenuFromMenuDef(menuDefDebug, dimof(menuDefDebug), CreateMenu());
    AppendMenu(mainMenu, MF_POPUP | MF_STRING, (UINT_PTR)m, _T("Debug"));
#endif

    return mainMenu;
}

TCHAR *MobiWindow::LoadedFilePath() const
{
    if (!ebookController || !ebookController->GetMobiDoc())
        return NULL;
    return ebookController->GetMobiDoc()->GetFileName();
}

static MobiWindow* FindMobiWindowByHwnd(HWND hwnd)
{
    for (MobiWindow **w = gMobiWindows.IterStart(); w; w = gMobiWindows.IterNext()) {
        if ((*w)->hwndFrame == hwnd)
            return *w;
    }
    return NULL;
}

MobiWindow* FindMobiWindowByController(EbookController *controller)
{
    for (MobiWindow **w = gMobiWindows.IterStart(); w; w = gMobiWindows.IterNext()) {
        if ((*w)->ebookController == controller)
            return *w;
    }
    return NULL;
}

#define LAYOUT_TIMER_ID 1

void RestartLayoutTimer(EbookController *controller)
{
    MobiWindow *win = FindMobiWindowByController(controller);
    KillTimer(win->hwndFrame, LAYOUT_TIMER_ID);
    SetTimer(win->hwndFrame,  LAYOUT_TIMER_ID, 600, NULL);
}

static void OnTimer(MobiWindow *win, WPARAM timerId)
{
    CrashIf(timerId != LAYOUT_TIMER_ID);
    KillTimer(win->hwndFrame, LAYOUT_TIMER_ID);
    win->ebookController->OnLayoutTimer();
}

static void OnToggleBbox(MobiWindow *win)
{
    gShowTextBoundingBoxes = !gShowTextBoundingBoxes;
    SetDebugPaint(gShowTextBoundingBoxes);
    InvalidateRect(win->hwndFrame, NULL, FALSE);
    win::menu::SetChecked(GetMenu(win->hwndFrame), IDM_DEBUG_SHOW_LINKS, gShowTextBoundingBoxes);
}

// closes a physical window, deletes the MobiWindow object and removes it
// from the global list of windows
void DeleteMobiWindow(MobiWindow *win, bool forceDelete)
{
    if (gPluginMode && !forceDelete)
        return;

    DeletePropertiesWindow(win->hwndFrame);
    delete win->ebookController;
    DestroyEbookControls(win->ebookControls);
    gMobiWindows.Remove(win);
    HWND toDestroy = win->hwndFrame;
    delete win;
    // must be called after removing win from gMobiWindows so that window
    // message processing doesn't pick up a window being destroyed
    DestroyWindow(toDestroy);
}

// if forceClose is true, we force window deletion in plugin mode
// if quitIfLast is true, we quit if we closed the last window, otherwise
// we create an about window
static void CloseMobiWindow(MobiWindow *win, bool quitIfLast, bool forceClose)
{
    DeleteMobiWindow(win, forceClose);
    if (TotalWindowsCount() > 0)
        return;
    if (quitIfLast) {
        PostQuitMessage(0);
        return;
    }
    WindowInfo *w = CreateAndShowWindowInfo();
    if (!w) {
        PostQuitMessage(0);
        return;
    }
}

static LRESULT OnKeyDown(MobiWindow *win, UINT msg, WPARAM key, LPARAM lParam)
{
    switch (key) {
    case VK_LEFT: case VK_PRIOR: case 'P':
        win->ebookController->AdvancePage(-1);
        break;
    case VK_RIGHT: case VK_NEXT: case 'N':
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
        CloseMobiWindow(win, true, true);
        break;
    default:
        return DefWindowProc(win->hwndFrame, msg, key, lParam);
    }
    return 0;
}

static void RebuildMenuBarForMobiWindow(MobiWindow *win)
{
    HMENU oldMenu = GetMenu(win->hwndFrame);
    HMENU newMenu = BuildMobiMenu();
#if 0 // TODO: support fullscreen mode when we have it
    if (!win->presentation && !win->fullScreen)
        SetMenu(win->hwndFrame, win->menu);
#endif
    SetMenu(win->hwndFrame, newMenu);
    DestroyMenu(oldMenu);
}

void UpdateMenuForMobiWindow(MobiWindow *win, HMENU m)
{
    UINT id = GetMenuItemID(m, 0);
    if (id == menuDefMobiFile[0].id)
        RebuildFileMenuForMobiWindow( m);
}

void RebuildMenuBarForMobiWindows()
{
    for (size_t i = 0; i < gMobiWindows.Count(); i++) {
        RebuildMenuBarForMobiWindow(gMobiWindows.At(i));
    }
}

static LRESULT OnGesture(MobiWindow *win, UINT message, WPARAM wParam, LPARAM lParam)
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

static LRESULT OnCommand(MobiWindow *win, UINT msg, WPARAM wParam, LPARAM lParam)
{
    int wmId = LOWORD(wParam);

    // check if the menuId belongs to an entry in the list of
    // recently opened files and load the referenced file if it does
    if ((wmId >= IDM_FILE_HISTORY_FIRST) && (wmId <= IDM_FILE_HISTORY_LAST))
    {
        DisplayState *state = gFileHistory.Get(wmId - IDM_FILE_HISTORY_FIRST);
        if (state && HasPermission(Perm_DiskAccess))
            LoadDocument(state->filePath, MakeSumatraWindow(win));
        return 0;
    }

    switch (wmId)
    {
        case IDM_OPEN:
        case IDT_FILE_OPEN:
            OnMenuOpen(MakeSumatraWindow(win));
            break;

        case IDT_FILE_EXIT:
        case IDM_CLOSE:
            CloseMobiWindow(win, false, false);
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

#if 0
        case IDM_GOTO_PAGE:
            OnMenuGoToPage(*win);
            break;
#endif

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

        case IDM_DEBUG_PAGE_LAYOUT:
            win->ebookController->SetHtml(gSampleHtml);
            break;

        case IDM_DEBUG_EBOOK_UI:
            gUseEbookUI = !gUseEbookUI;
            win::menu::SetChecked(GetMenu(win->hwndFrame), IDM_DEBUG_EBOOK_UI, !gUseEbookUI);
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
            OnMenuProperties(MakeSumatraWindow(win));
            break;

        default:
            return DefWindowProc(win->hwndFrame, msg, wParam, lParam);
    }

    return 0;
}

static LRESULT CALLBACK MobiWndProcFrame(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    // messages that don't require MobiWindow
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

    // messages that do require MobiWindow
    MobiWindow *win = FindMobiWindowByHwnd(hwnd);
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
            CloseMobiWindow(win, true, true);
            break;

        case WM_PAINT:
            win->hwndWrapper->OnPaint(hwnd);
            break;

        case WM_KEYDOWN:
            return OnKeyDown(win, msg, wParam, lParam);

        case WM_COMMAND:
            return OnCommand(win, msg, wParam, lParam);
            break;

        case WM_INITMENUPOPUP:
            UpdateMenuForMobiWindow(win, (HMENU)wParam);
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

RenderedBitmap *RenderFirstMobiPageToBitmap(MobiDoc *mobiDoc, SizeI pageSize, SizeI bmpSize)
{
    PoolAllocator textAllocator;
    LayoutInfo *li = GetLayoutInfo(NULL, mobiDoc, pageSize.dx, pageSize.dy, &textAllocator);
    PageLayout pl;
    PageData *pd = pl.IterStart(li);
    if (!pd)
        return NULL;

    Bitmap *pageBmp = ::new Bitmap(pageSize.dx, pageSize.dy, PixelFormat32bppARGB);
    if (!pageBmp) {
        delete pd;
        return NULL;
    }
    Graphics g((Image*)pageBmp);
    Rect r(0, 0, pageSize.dx, pageSize.dy);
    r.Inflate(1,1);
    SolidBrush br(Color(255, 255, 255));
    g.FillRectangle(&br, r);
    DrawPageLayout(&g, &pd->instructions, 0, 0, false, &Color(Color::Black));

    Bitmap res(bmpSize.dx, bmpSize.dy, PixelFormat24bppRGB);
    Graphics g2(&res);
    g2.SetInterpolationMode(InterpolationModeHighQualityBicubic);
    g2.DrawImage(pageBmp, Rect(0, 0, bmpSize.dx, bmpSize.dy),
                 0, 0, pageSize.dx, pageSize.dy, UnitPixel);

    HBITMAP hbmp;
    Status ok = res.GetHBITMAP(Color::White, &hbmp);
    ::delete pageBmp;
    delete pd;
    if (ok != Ok)
        return NULL;
    return new RenderedBitmap(hbmp, bmpSize);
}

static void CreateThumbnailForMobiDoc(MobiDoc *mobiDoc, DisplayState& ds)
{
    CrashIf(!mobiDoc);
    if (!ShouldSaveThumbnail(ds))
        return;

    SizeI pageSize(THUMBNAIL_DX * 2, THUMBNAIL_DY * 2);
    SizeI bmpSize(THUMBNAIL_DX, THUMBNAIL_DY);
    RenderedBitmap *bmp = RenderFirstMobiPageToBitmap(mobiDoc, pageSize, bmpSize);
    if (bmp && SaveThumbnailForFile(mobiDoc->GetFileName(), bmp))
        bmp = NULL;
    delete bmp;
}

void OpenMobiInWindow(MobiDoc *mobiDoc, SumatraWindow& winToReplace)
{
    TCHAR *fullPath = mobiDoc->GetFileName();

    if (gGlobalPrefs.rememberOpenedFiles) {
        DisplayState *ds = gFileHistory.MarkFileLoaded(fullPath);
        if (gGlobalPrefs.showStartPage && ds) {
            // TODO: do it on a background thread?
            CreateThumbnailForMobiDoc(mobiDoc, *ds);
        }
        SavePrefs();
    }

    // Add the file also to Windows' recently used documents (this doesn't
    // happen automatically on drag&drop, reopening from history, etc.)
    if (HasPermission(Perm_DiskAccess) && !gPluginMode)
        SHAddToRecentDocs(SHARD_PATH, fullPath);

    if (winToReplace.AsMobiWindow()) {
        MobiWindow *mw = winToReplace.AsMobiWindow();
        CrashIf(!mw);
        mw->ebookController->SetMobiDoc(mobiDoc);
        // TODO: if we have window position/last position for this file, restore it
        return;
    }

    RectI windowPos = gGlobalPrefs.windowPos;
    if (!windowPos.IsEmpty())
        EnsureAreaVisibility(windowPos);
    else
        windowPos = GetDefaultWindowPos();

    WindowInfo *winInfo = winToReplace.AsWindowInfo();
    bool wasMaximized = false;
    if (winInfo && winInfo->hwndFrame)
        wasMaximized = IsZoomed(winInfo->hwndFrame);
    CloseDocumentAndDeleteWindowInfo(winInfo);

    HWND hwnd = CreateWindow(
            MOBI_FRAME_CLASS_NAME, SUMATRA_WINDOW_TITLE,
            WS_OVERLAPPEDWINDOW,
            windowPos.x, windowPos.y, windowPos.dx, windowPos.dy,
            NULL, NULL,
            ghinst, NULL);
    if (!hwnd)
        return;
    SetMenu(hwnd, BuildMobiMenu());
    if (HasPermission(Perm_DiskAccess) && !gPluginMode)
        DragAcceptFiles(hwnd, TRUE);
    if (Touch::SupportsGestures()) {
        // TODO: does this do anything without WM_TOUCH handling?
        GESTURECONFIG gc = { 0, GC_ALLGESTURES, 0 };
        Touch::SetGestureConfig(hwnd, 0, 1, &gc, sizeof(GESTURECONFIG));
    }

    MobiWindow *win = new MobiWindow();
    win->ebookControls = CreateEbookControls(hwnd);
    win->hwndWrapper = win->ebookControls->mainWnd;
    win->ebookController = new EbookController(win->ebookControls);
    win->hwndFrame = hwnd;

    gMobiWindows.Append(win);
    ShowWindow(hwnd, wasMaximized ? SW_SHOWMAXIMIZED : SW_SHOW);
    win->ebookController->SetMobiDoc(mobiDoc);
}

bool RegisterMobiWinClass(HINSTANCE hinst)
{
    WNDCLASSEX  wcex;
    FillWndClassEx(wcex, hinst);
    // clear out CS_HREDRAW | CS_VREDRAW style so that resizing doesn't
    // cause the whole window redraw
    wcex.style = 0;
    wcex.lpszClassName  = MOBI_FRAME_CLASS_NAME;
    wcex.hIcon          = LoadIcon(hinst, MAKEINTRESOURCE(IDI_SUMATRAPDF));
    wcex.hbrBackground  = CreateSolidBrush(GetSysColor(COLOR_BTNFACE));
    wcex.lpfnWndProc    = MobiWndProcFrame;

    ATOM atom = RegisterClassEx(&wcex);
    return atom != NULL;
}
