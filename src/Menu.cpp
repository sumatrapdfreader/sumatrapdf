/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "Menu.h"
#include "DisplayModel.h"
#include "translations.h"
#include "WindowInfo.h"
#include "Favorites.h"
#include "WinUtil.h"
#include "FileUtil.h"
#include "FileHistory.h"
#include "ExternalPdfViewer.h"
#include "Selection.h"
#include "SumatraAbout.h"
#include "SumatraDialogs.h"

void MenuUpdateDisplayMode(WindowInfo* win)
{
    bool enabled = win->IsDocLoaded();
    DisplayMode displayMode = enabled ? win->dm->displayMode() : gGlobalPrefs.defaultDisplayMode;

    for (int id = IDM_VIEW_LAYOUT_FIRST; id <= IDM_VIEW_LAYOUT_LAST; id++)
        win::menu::SetEnabled(win->menu, id, enabled);

    UINT id = 0;
    if (displayModeSingle(displayMode))
        id = IDM_VIEW_SINGLE_PAGE;
    else if (displayModeFacing(displayMode))
        id = IDM_VIEW_FACING;
    else if (displayModeShowCover(displayMode))
        id = IDM_VIEW_BOOK;
    else
        assert(!win->dm && DM_AUTOMATIC == displayMode);

    CheckMenuRadioItem(win->menu, IDM_VIEW_LAYOUT_FIRST, IDM_VIEW_LAYOUT_LAST, id, MF_BYCOMMAND);
    win::menu::SetChecked(win->menu, IDM_VIEW_CONTINUOUS, displayModeContinuous(displayMode));
}

#define SEP_ITEM "-----"

MenuDef menuDefFile[] = {
    { _TRN("&Open\tCtrl+O"),                IDM_OPEN ,                  MF_REQ_DISK_ACCESS },
    { _TRN("&Close\tCtrl+W"),               IDM_CLOSE,                  MF_REQ_DISK_ACCESS },
    { _TRN("&Save As...\tCtrl+S"),          IDM_SAVEAS,                 MF_REQ_DISK_ACCESS },
    { _TRN("&Print...\tCtrl+P"),            IDM_PRINT,                  MF_REQ_PRINTER_ACCESS },
    { SEP_ITEM,                             0,                          MF_REQ_DISK_ACCESS },
    { _TRN("Save S&hortcut...\tCtrl+Shift+S"), IDM_SAVEAS_BOOKMARK,     MF_REQ_DISK_ACCESS | MF_NOT_FOR_CHM },
    // PDF specific items are dynamically removed in RebuildFileMenu
    { _TRN("Open in &Adobe Reader"),        IDM_VIEW_WITH_ACROBAT,      MF_REQ_DISK_ACCESS },
    { _TRN("Open in &Foxit Reader"),        IDM_VIEW_WITH_FOXIT,        MF_REQ_DISK_ACCESS },
    { _TRN("Open in PDF-XChange"),          IDM_VIEW_WITH_PDF_XCHANGE,  MF_REQ_DISK_ACCESS },
    { _TRN("Send by &E-mail..."),           IDM_SEND_BY_EMAIL,          MF_REQ_DISK_ACCESS },
    { SEP_ITEM,                             0,                          MF_REQ_DISK_ACCESS },
    { _TRN("P&roperties\tCtrl+D"),          IDM_PROPERTIES,             0 },
    { SEP_ITEM,                             0,                          0 },
    { _TRN("E&xit\tCtrl+Q"),                IDM_EXIT,                   0 }
};

MenuDef menuDefView[] = {
    { _TRN("&Single Page\tCtrl+6"),         IDM_VIEW_SINGLE_PAGE,       MF_NOT_FOR_CHM },
    { _TRN("&Facing\tCtrl+7"),              IDM_VIEW_FACING,            MF_NOT_FOR_CHM },
    { _TRN("&Book View\tCtrl+8"),           IDM_VIEW_BOOK,              MF_NOT_FOR_CHM },
    { _TRN("Show &pages continuously"),     IDM_VIEW_CONTINUOUS,        MF_NOT_FOR_CHM },
    { SEP_ITEM,                             0,                          MF_NOT_FOR_CHM },
    { _TRN("Rotate &Left\tCtrl+Shift+-"),   IDM_VIEW_ROTATE_LEFT,       MF_NOT_FOR_CHM },
    { _TRN("Rotate &Right\tCtrl+Shift++"),  IDM_VIEW_ROTATE_RIGHT,      MF_NOT_FOR_CHM },
    { SEP_ITEM,                             0,                          MF_NOT_FOR_CHM },
    { _TRN("Pr&esentation\tCtrl+L"),        IDM_VIEW_PRESENTATION_MODE, 0 },
    { _TRN("F&ullscreen\tCtrl+Shift+L"),    IDM_VIEW_FULLSCREEN,        0 },
    { SEP_ITEM,                             0,                          0 },
    { _TRN("Book&marks\tF12"),              IDM_VIEW_BOOKMARKS,         0 },
    { _TRN("Show &Toolbar"),                IDM_VIEW_SHOW_HIDE_TOOLBAR, 0 },
    { SEP_ITEM,                             0,                          MF_REQ_ALLOW_COPY },
    { _TRN("Select &All\tCtrl+A"),          IDM_SELECT_ALL,             MF_REQ_ALLOW_COPY },
    { _TRN("&Copy Selection\tCtrl+C"),      IDM_COPY_SELECTION,         MF_REQ_ALLOW_COPY },
};

MenuDef menuDefGoTo[] = {
    { _TRN("&Next Page\tRight Arrow"),      IDM_GOTO_NEXT_PAGE,         0 },
    { _TRN("&Previous Page\tLeft Arrow"),   IDM_GOTO_PREV_PAGE,         0 },
    { _TRN("&First Page\tHome"),            IDM_GOTO_FIRST_PAGE,        0 },
    { _TRN("&Last Page\tEnd"),              IDM_GOTO_LAST_PAGE,         0 },
    { _TRN("Pa&ge...\tCtrl+G"),             IDM_GOTO_PAGE,              0 },
    { SEP_ITEM,                             0,                          0 },
    { _TRN("&Back\tAlt+Left Arrow"),        IDM_GOTO_NAV_BACK,          0 },
    { _TRN("F&orward\tAlt+Right Arrow"),    IDM_GOTO_NAV_FORWARD,       0 },
    { SEP_ITEM,                             0,                          0 },
    { _TRN("Fin&d...\tCtrl+F"),             IDM_FIND_FIRST,             0 },
};

MenuDef menuDefZoom[] = {
    { _TRN("Fit &Page\tCtrl+0"),            IDM_ZOOM_FIT_PAGE,          MF_NOT_FOR_CHM },
    { _TRN("&Actual Size\tCtrl+1"),         IDM_ZOOM_ACTUAL_SIZE,       MF_NOT_FOR_CHM },
    { _TRN("Fit &Width\tCtrl+2"),           IDM_ZOOM_FIT_WIDTH,         MF_NOT_FOR_CHM },
    { _TRN("Fit &Content\tCtrl+3"),         IDM_ZOOM_FIT_CONTENT,       MF_NOT_FOR_CHM },
    { _TRN("Custom &Zoom...\tCtrl+Y"),      IDM_ZOOM_CUSTOM,            0 },
    { SEP_ITEM,                             0,                          0 },
    { "6400%",                              IDM_ZOOM_6400,              MF_NO_TRANSLATE | MF_NOT_FOR_CHM },
    { "3200%",                              IDM_ZOOM_3200,              MF_NO_TRANSLATE | MF_NOT_FOR_CHM },
    { "1600%",                              IDM_ZOOM_1600,              MF_NO_TRANSLATE | MF_NOT_FOR_CHM },
    { "800%",                               IDM_ZOOM_800,               MF_NO_TRANSLATE | MF_NOT_FOR_CHM },
    { "400%",                               IDM_ZOOM_400,               MF_NO_TRANSLATE },
    { "200%",                               IDM_ZOOM_200,               MF_NO_TRANSLATE },
    { "150%",                               IDM_ZOOM_150,               MF_NO_TRANSLATE },
    { "125%",                               IDM_ZOOM_125,               MF_NO_TRANSLATE },
    { "100%",                               IDM_ZOOM_100,               MF_NO_TRANSLATE },
    { "50%",                                IDM_ZOOM_50,                MF_NO_TRANSLATE },
    { "25%",                                IDM_ZOOM_25,                MF_NO_TRANSLATE },
    { "12.5%",                              IDM_ZOOM_12_5,              MF_NO_TRANSLATE | MF_NOT_FOR_CHM },
    { "8.33%",                              IDM_ZOOM_8_33,              MF_NO_TRANSLATE | MF_NOT_FOR_CHM },
};

MenuDef menuDefSettings[] = {
    { _TRN("Change Language"),              IDM_CHANGE_LANGUAGE,        0  },
#if 0
    { _TRN("Contribute Translation"),       IDM_CONTRIBUTE_TRANSLATION, MF_REQ_DISK_ACCESS },
    { SEP_ITEM,                             0,                          MF_REQ_DISK_ACCESS },
#endif
    { _TRN("&Options..."),                  IDM_SETTINGS,               MF_REQ_PREF_ACCESS },
};

MenuDef menuDefFavorites[] = {
    { _TRN("Add to favorites"),             IDM_FAV_ADD,                0 },
    { _TRN("Remove from favorites"),        IDM_FAV_DEL,                0 },
    { _TRN("Show Favorites"),               IDM_FAV_TOGGLE,             MF_REQ_DISK_ACCESS },
};

MenuDef menuDefHelp[] = {
    { _TRN("Visit &Website"),               IDM_VISIT_WEBSITE,          MF_REQ_DISK_ACCESS },
    { _TRN("&Manual"),                      IDM_MANUAL,                 MF_REQ_DISK_ACCESS },
    { _TRN("Check for &Updates"),           IDM_CHECK_UPDATE,           MF_REQ_INET_ACCESS },
    { SEP_ITEM,                             0,                          MF_REQ_DISK_ACCESS },
    { _TRN("&About"),                       IDM_ABOUT,                  0 },
};

#ifdef SHOW_DEBUG_MENU_ITEMS
MenuDef menuDefDebug[] = {
    { "Highlight links",                    IDM_DEBUG_SHOW_LINKS,       MF_NO_TRANSLATE },
    { "Toggle PDF/XPS renderer",            IDM_DEBUG_GDI_RENDERER,     MF_NO_TRANSLATE },
    //{ SEP_ITEM,                             0,                          0 },
    //{ "Crash me",                           IDM_DEBUG_CRASH_ME,         MF_NO_TRANSLATE },
};
#endif

// not used for Chm documents
MenuDef menuDefContext[] = {
    { _TRN("&Copy Selection"),              IDM_COPY_SELECTION,         MF_REQ_ALLOW_COPY },
    { _TRN("Copy &Link Address"),           IDM_COPY_LINK_TARGET,       MF_REQ_ALLOW_COPY },
    { _TRN("Copy Co&mment"),                IDM_COPY_COMMENT,           MF_REQ_ALLOW_COPY },
    { SEP_ITEM,                             0,                          MF_REQ_ALLOW_COPY },
    { _TRN("Select &All"),                  IDM_SELECT_ALL,             MF_REQ_ALLOW_COPY },
    { SEP_ITEM,                             0,                          MF_PLUGIN_MODE_ONLY | MF_REQ_ALLOW_COPY },
    { _TRN("&Save As..."),                  IDM_SAVEAS,                 MF_PLUGIN_MODE_ONLY | MF_REQ_DISK_ACCESS },
    { _TRN("&Print..."),                    IDM_PRINT,                  MF_PLUGIN_MODE_ONLY | MF_REQ_PRINTER_ACCESS },
    { _TRN("P&roperties"),                  IDM_PROPERTIES,             MF_PLUGIN_MODE_ONLY },
};

MenuDef menuDefContextStart[] = {
    { _TRN("&Open Document"),               IDM_OPEN_SELECTED_DOCUMENT, MF_REQ_DISK_ACCESS },
    { _TRN("&Pin Document"),                IDM_PIN_SELECTED_DOCUMENT,  MF_REQ_DISK_ACCESS | MF_REQ_PREF_ACCESS },
    { SEP_ITEM,                             0,                          MF_REQ_DISK_ACCESS | MF_REQ_PREF_ACCESS },
    { _TRN("&Remove Document"),             IDM_FORGET_SELECTED_DOCUMENT, MF_REQ_DISK_ACCESS | MF_REQ_PREF_ACCESS },
};

static void AddFileMenuItem(HMENU menuFile, const TCHAR *filePath, UINT index)
{
    assert(filePath && menuFile);
    if (!filePath || !menuFile) return;

    ScopedMem<TCHAR> fileName(win::menu::ToSafeString(path::GetBaseName(filePath)));
    ScopedMem<TCHAR> menuString(str::Format(_T("&%d) %s"), (index + 1) % 10, fileName));
    UINT menuId = IDM_FILE_HISTORY_FIRST + index;
    InsertMenu(menuFile, IDM_EXIT, MF_BYCOMMAND | MF_ENABLED | MF_STRING, menuId, menuString);
}

HMENU BuildMenuFromMenuDef(MenuDef menuDefs[], int menuLen, HMENU menu, bool forChm)
{
    assert(menu);

    for (int i = 0; i < menuLen; i++) {
        MenuDef md = menuDefs[i];
        const char *title = md.title;
        if (!HasPermission(md.flags >> PERM_FLAG_OFFSET))
            continue;
        if (!gPluginMode && (md.flags & MF_PLUGIN_MODE_ONLY))
            continue;
        if (forChm && (md.flags & MF_NOT_FOR_CHM))
            continue;

        if (str::Eq(title, SEP_ITEM)) {
            AppendMenu(menu, MF_SEPARATOR, 0, NULL);
        } else if (MF_NO_TRANSLATE == (md.flags & MF_NO_TRANSLATE)) {
            ScopedMem<TCHAR> tmp(str::conv::FromUtf8(title));
            AppendMenu(menu, MF_STRING, (UINT_PTR)md.id, tmp);
        } else {
            const TCHAR *tmp = Trans::GetTranslation(title);
            AppendMenu(menu, MF_STRING, (UINT_PTR)md.id, tmp);
        }
    }

    return menu;
}

static void AppendRecentFilesToMenu(HMENU m)
{
    if (!HasPermission(Perm_DiskAccess)) return;
    if (gFileHistory.IsEmpty()) return;

    for (int index = 0; index < FILE_HISTORY_MAX_RECENT; index++) {
        DisplayState *state = gFileHistory.Get(index);
        if (!state)
            break;
        assert(state->filePath);
        if (state->filePath)
            AddFileMenuItem(m, state->filePath, index);
        if (FILE_HISTORY_MAX_RECENT == index)
            DBG_OUT("  not adding, reached max %d items\n", FILE_HISTORY_MAX_RECENT);
    }

    InsertMenu(m, IDM_EXIT, MF_BYCOMMAND | MF_SEPARATOR, 0, NULL);
}

static struct {
    unsigned short itemId;
    float zoom;
} gZoomMenuIds[] = {
    { IDM_ZOOM_6400,    6400.0 },
    { IDM_ZOOM_3200,    3200.0 },
    { IDM_ZOOM_1600,    1600.0 },
    { IDM_ZOOM_800,     800.0  },
    { IDM_ZOOM_400,     400.0  },
    { IDM_ZOOM_200,     200.0  },
    { IDM_ZOOM_150,     150.0  },
    { IDM_ZOOM_125,     125.0  },
    { IDM_ZOOM_100,     100.0  },
    { IDM_ZOOM_50,      50.0   },
    { IDM_ZOOM_25,      25.0   },
    { IDM_ZOOM_12_5,    12.5   },
    { IDM_ZOOM_8_33,    8.33f  },
    { IDM_ZOOM_CUSTOM,  0      },
    { IDM_ZOOM_FIT_PAGE,    ZOOM_FIT_PAGE    },
    { IDM_ZOOM_FIT_WIDTH,   ZOOM_FIT_WIDTH   },
    { IDM_ZOOM_FIT_CONTENT, ZOOM_FIT_CONTENT },
    { IDM_ZOOM_ACTUAL_SIZE, ZOOM_ACTUAL_SIZE },
};

UINT MenuIdFromVirtualZoom(float virtualZoom)
{
    for (int i = 0; i < dimof(gZoomMenuIds); i++) {
        if (virtualZoom == gZoomMenuIds[i].zoom)
            return gZoomMenuIds[i].itemId;
    }
    return IDM_ZOOM_CUSTOM;
}

static float ZoomMenuItemToZoom(UINT menuItemId)
{
    for (int i = 0; i < dimof(gZoomMenuIds); i++) {
        if (menuItemId == gZoomMenuIds[i].itemId)
            return gZoomMenuIds[i].zoom;
    }
    assert(0);
    return 100.0;
}

static void ZoomMenuItemCheck(HMENU m, UINT menuItemId, bool canZoom)
{
    assert(IDM_ZOOM_FIRST <= menuItemId && menuItemId <= IDM_ZOOM_LAST);

    for (int i = 0; i < dimof(gZoomMenuIds); i++)
        win::menu::SetEnabled(m, gZoomMenuIds[i].itemId, canZoom);

    if (IDM_ZOOM_100 == menuItemId)
        menuItemId = IDM_ZOOM_ACTUAL_SIZE;
    CheckMenuRadioItem(m, IDM_ZOOM_FIRST, IDM_ZOOM_LAST, menuItemId, MF_BYCOMMAND);
    if (IDM_ZOOM_ACTUAL_SIZE == menuItemId)
        CheckMenuRadioItem(m, IDM_ZOOM_100, IDM_ZOOM_100, IDM_ZOOM_100, MF_BYCOMMAND);
}

void MenuUpdateZoom(WindowInfo* win)
{
    float zoomVirtual = gGlobalPrefs.defaultZoom;
    if (win->IsDocLoaded())
        zoomVirtual = win->dm->ZoomVirtual();
    UINT menuId = MenuIdFromVirtualZoom(zoomVirtual);
    ZoomMenuItemCheck(win->menu, menuId, win->IsDocLoaded());
}

void MenuUpdatePrintItem(WindowInfo* win, HMENU menu, bool disableOnly=false) {
    bool filePrintEnabled = win->IsDocLoaded();
    bool filePrintAllowed = !filePrintEnabled || win->dm->engine->IsPrintingAllowed();

    int ix;
    for (ix = 0; ix < dimof(menuDefFile) && menuDefFile[ix].id != IDM_PRINT; ix++);
    assert(ix < dimof(menuDefFile));
    if (ix < dimof(menuDefFile)) {
        const TCHAR *printItem = Trans::GetTranslation(menuDefFile[ix].title);
        if (!filePrintAllowed)
            printItem = _TR("&Print... (denied)");
        if (!filePrintAllowed || !disableOnly)
            ModifyMenu(menu, IDM_PRINT, MF_BYCOMMAND | MF_STRING, IDM_PRINT, printItem);
    }

    win::menu::SetEnabled(menu, IDM_PRINT, filePrintEnabled && filePrintAllowed);
}

bool IsFileCloseMenuEnabled()
{
    for (size_t i = 0; i < gWindows.Count(); i++) {
        if (!gWindows.At(i)->IsAboutWindow())
            return true;
    }
    return false;
}

void MenuUpdateStateForWindow(WindowInfo* win) {
    // those menu items will be disabled if no document is opened, enabled otherwise
    static UINT menusToDisableIfNoDocument[] = {
        IDM_VIEW_ROTATE_LEFT, IDM_VIEW_ROTATE_RIGHT, IDM_GOTO_NEXT_PAGE, IDM_GOTO_PREV_PAGE,
        IDM_GOTO_FIRST_PAGE, IDM_GOTO_LAST_PAGE, IDM_GOTO_NAV_BACK, IDM_GOTO_NAV_FORWARD,
        IDM_GOTO_PAGE, IDM_FIND_FIRST, IDM_SAVEAS, IDM_SAVEAS_BOOKMARK, IDM_SEND_BY_EMAIL,
        IDM_VIEW_WITH_ACROBAT, IDM_VIEW_WITH_FOXIT, IDM_VIEW_WITH_PDF_XCHANGE, 
        IDM_SELECT_ALL, IDM_COPY_SELECTION, IDM_PROPERTIES, IDM_VIEW_PRESENTATION_MODE,
        IDM_VIEW_WITH_ACROBAT, IDM_VIEW_WITH_FOXIT, IDM_VIEW_WITH_PDF_XCHANGE,
    };
    static UINT menusToDisableIfDirectory[] = {
        IDM_SAVEAS, IDM_SEND_BY_EMAIL
    };

    assert(IsFileCloseMenuEnabled() == !win->IsAboutWindow());
    win::menu::SetEnabled(win->menu, IDM_CLOSE, IsFileCloseMenuEnabled());

    MenuUpdatePrintItem(win, win->menu);

    bool enabled = win->IsDocLoaded() && win->dm->HasTocTree();
    win::menu::SetEnabled(win->menu, IDM_VIEW_BOOKMARKS, enabled);

    bool documentSpecific = win->IsDocLoaded();
    bool checked = documentSpecific ? win->tocVisible : gGlobalPrefs.tocVisible;
    win::menu::SetChecked(win->menu, IDM_VIEW_BOOKMARKS, checked);

    win::menu::SetChecked(win->menu, IDM_FAV_TOGGLE, gGlobalPrefs.favVisible);
    win::menu::SetChecked(win->menu, IDM_VIEW_SHOW_HIDE_TOOLBAR, gGlobalPrefs.toolbarVisible);
    MenuUpdateDisplayMode(win);
    MenuUpdateZoom(win);

    if (win->IsDocLoaded()) {
        win::menu::SetEnabled(win->menu, IDM_GOTO_NAV_BACK, win->dm->CanNavigate(-1));
        win::menu::SetEnabled(win->menu, IDM_GOTO_NAV_FORWARD, win->dm->CanNavigate(1));
    }

    for (int i = 0; i < dimof(menusToDisableIfNoDocument); i++) {
        UINT id = menusToDisableIfNoDocument[i];
        win::menu::SetEnabled(win->menu, id, win->IsDocLoaded());
    }

    if (win->dm && Engine_ImageDir == win->dm->engineType) {
        for (int i = 0; i < dimof(menusToDisableIfDirectory); i++) {
            UINT id = menusToDisableIfDirectory[i];
            win::menu::SetEnabled(win->menu, id, false);
        }
    }

    if (win->dm && win->dm->engine)
        win::menu::SetEnabled(win->menu, IDM_FIND_FIRST, !win->dm->engine->IsImageCollection());

#ifdef SHOW_DEBUG_MENU_ITEMS
    win::menu::SetChecked(win->menu, IDM_DEBUG_SHOW_LINKS, gDebugShowLinks);
    win::menu::SetChecked(win->menu, IDM_DEBUG_GDI_RENDERER, gUseGdiRenderer);
#endif
}

void OnAboutContextMenu(WindowInfo* win, int x, int y)
{
    if (!HasPermission(Perm_SavePreferences | Perm_DiskAccess) || !gGlobalPrefs.rememberOpenedFiles || !gGlobalPrefs.showStartPage)
        return;

    const TCHAR *filePath = GetStaticLink(win->staticLinks, x, y);
    if (!filePath || *filePath == '<')
        return;

    DisplayState *state = gFileHistory.Find(filePath);
    assert(state);
    if (!state)
        return;

    HMENU popup = BuildMenuFromMenuDef(menuDefContextStart, dimof(menuDefContextStart), CreatePopupMenu());
    win::menu::SetChecked(popup, IDM_PIN_SELECTED_DOCUMENT, state->isPinned);
    POINT pt = { x, y };
    MapWindowPoints(win->hwndCanvas, HWND_DESKTOP, &pt, 1);
    INT cmd = TrackPopupMenu(popup, TPM_RETURNCMD | TPM_RIGHTBUTTON,
                             pt.x, pt.y, 0, win->hwndFrame, NULL);
    switch (cmd) {
    case IDM_OPEN_SELECTED_DOCUMENT:
        LoadDocument(filePath, win);
        break;

    case IDM_PIN_SELECTED_DOCUMENT:
        state->isPinned = !state->isPinned;
        win->DeleteInfotip();
        win->RedrawAll(true);
        break;

    case IDM_FORGET_SELECTED_DOCUMENT:
        gFileHistory.Remove(state);
        delete state;
        CleanUpThumbnailCache(gFileHistory);
        win->DeleteInfotip();
        win->RedrawAll(true);
        break;
    }
    DestroyMenu(popup);
}

void OnContextMenu(WindowInfo* win, int x, int y)
{
    assert(win->IsDocLoaded());
    if (!win->IsDocLoaded())
        return;

    PageElement *pageEl = win->dm->GetElementAtPos(PointI(x, y));
    ScopedMem<TCHAR> value(pageEl ? pageEl->GetValue() : NULL);

    HMENU popup = BuildMenuFromMenuDef(menuDefContext, dimof(menuDefContext), CreatePopupMenu());
    if (!value || NULL == pageEl->AsLink())
        win::menu::Remove(popup, IDM_COPY_LINK_TARGET);
    if (!value || NULL != pageEl->AsLink())
        win::menu::Remove(popup, IDM_COPY_COMMENT);

    if (!win->selectionOnPage)
        win::menu::SetEnabled(popup, IDM_COPY_SELECTION, false);
    MenuUpdatePrintItem(win, popup, true);

    POINT pt = { x, y };
    MapWindowPoints(win->hwndCanvas, HWND_DESKTOP, &pt, 1);
    INT cmd = TrackPopupMenu(popup, TPM_RETURNCMD | TPM_RIGHTBUTTON,
                             pt.x, pt.y, 0, win->hwndFrame, NULL);
    switch (cmd) {
    case IDM_COPY_SELECTION:
    case IDM_SELECT_ALL:
    case IDM_SAVEAS:
    case IDM_PRINT:
    case IDM_PROPERTIES:
        SendMessage(win->hwndFrame, WM_COMMAND, cmd, 0);
        break;

    case IDM_COPY_LINK_TARGET:
    case IDM_COPY_COMMENT:
        CopyTextToClipboard(value);
        break;
    }

    DestroyMenu(popup);
    delete pageEl;
}

/* Zoom document in window 'hwnd' to zoom level 'zoom'.
   'zoom' is given as a floating-point number, 1.0 is 100%, 2.0 is 200% etc.
*/
void OnMenuZoom(WindowInfo* win, UINT menuId)
{
    if (!win->IsDocLoaded())
        return;

    float zoom = ZoomMenuItemToZoom(menuId);
    ZoomToSelection(win, zoom, false);
}

void OnMenuCustomZoom(WindowInfo* win)
{
    if (!win->IsDocLoaded())
        return;

    float zoom = win->dm->ZoomVirtual();
    if (!Dialog_CustomZoom(win->hwndFrame, win->IsChm(), &zoom))
        return;
    ZoomToSelection(win, zoom, false);
}

static void RebuildFileMenu(WindowInfo *win, HMENU menu)
{
    win::menu::Empty(menu);
    BuildMenuFromMenuDef(menuDefFile, dimof(menuDefFile), menu, win->IsChm());
    AppendRecentFilesToMenu(menu);

    // Suppress menu items that depend on specific software being installed:
    // e-mail client, Adobe Reader, Foxit, PDF-XChange
    // Don't hide items here that won't always be hidden
    // (MenuUpdateStateForWindow() is for that)
    if (!CanSendAsEmailAttachment())
        win::menu::Remove(menu, IDM_SEND_BY_EMAIL);

    // Also suppress PDF specific items for non-PDF documents
    bool isNonPdf = win->IsDocLoaded() && !win->IsPdf();
    if (isNonPdf || !CanViewWithAcrobat())
        win::menu::Remove(menu, IDM_VIEW_WITH_ACROBAT);
    if (isNonPdf || !CanViewWithFoxit())
        win::menu::Remove(menu, IDM_VIEW_WITH_FOXIT);
    if (isNonPdf || !CanViewWithPDFXChange())
        win::menu::Remove(menu, IDM_VIEW_WITH_PDF_XCHANGE);
}

HMENU BuildMenu(WindowInfo *win)
{
    HMENU mainMenu = CreateMenu();
    HMENU m = CreateMenu();
    RebuildFileMenu(win, m);
    AppendMenu(mainMenu, MF_POPUP | MF_STRING, (UINT_PTR)m, _TR("&File"));
    m = BuildMenuFromMenuDef(menuDefView, dimof(menuDefView), CreateMenu(), win->IsChm());
    AppendMenu(mainMenu, MF_POPUP | MF_STRING, (UINT_PTR)m, _TR("&View"));
    m = BuildMenuFromMenuDef(menuDefGoTo, dimof(menuDefGoTo), CreateMenu(), win->IsChm());
    AppendMenu(mainMenu, MF_POPUP | MF_STRING, (UINT_PTR)m, _TR("&Go To"));
    m = BuildMenuFromMenuDef(menuDefZoom, dimof(menuDefZoom), CreateMenu(), win->IsChm());
    AppendMenu(mainMenu, MF_POPUP | MF_STRING, (UINT_PTR)m, _TR("&Zoom"));

    if (HasPermission(Perm_SavePreferences)) {
        // I think it makes sense to disable favorites in restricted mode
        // because they wouldn't be persisted, anyway
        m = CreateMenu();
        RebuildFavMenu(win, m);
        AppendMenu(mainMenu, MF_POPUP | MF_STRING, (UINT_PTR)m, _TR("F&avorites"));
    }
    m = BuildMenuFromMenuDef(menuDefSettings, dimof(menuDefSettings), CreateMenu());
    AppendMenu(mainMenu, MF_POPUP | MF_STRING, (UINT_PTR)m, _TR("&Settings"));
    m = BuildMenuFromMenuDef(menuDefHelp, dimof(menuDefHelp), CreateMenu());
    AppendMenu(mainMenu, MF_POPUP | MF_STRING, (UINT_PTR)m, _TR("&Help"));
#ifdef SHOW_DEBUG_MENU_ITEMS
    m = BuildMenuFromMenuDef(menuDefDebug, dimof(menuDefDebug), CreateMenu());
    AppendMenu(mainMenu, MF_POPUP | MF_STRING, (UINT_PTR)m, _T("Debug"));
#endif

    SetMenu(win->hwndFrame, mainMenu);
    return mainMenu;
}

void UpdateMenu(WindowInfo *win, HMENU m)
{
    UINT id = GetMenuItemID(m, 0);
    if (id == menuDefFile[0].id)
        RebuildFileMenu(win, m);
    else if (id == menuDefFavorites[0].id)
        RebuildFavMenu(win, m);
    if (win)
        MenuUpdateStateForWindow(win);
}
