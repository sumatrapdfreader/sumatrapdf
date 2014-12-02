/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "BaseEngine.h"
#include "resource.h"
#include "SettingsStructs.h"
#include "Controller.h"
#include "AppPrefs.h"
#include "CmdLineParser.h"
#include "EngineManager.h"
#include "DisplayModel.h"
#include "FileUtil.h"
#include "FileHistory.h"
#include "FileThumbnails.h"
#include "HtmlParserLookup.h"
#include "Mui.h"
#include "WindowInfo.h"
#include "Selection.h"
#include "SumatraAbout.h"
#include "SumatraDialogs.h"
#include "SumatraPDF.h"
#include "Translations.h"
#include "WinUtil.h"
#include "ExternalPdfViewer.h"
#include "Favorites.h"
#include "Menu.h"

void MenuUpdateDisplayMode(WindowInfo* win)
{
    bool enabled = win->IsDocLoaded();
    DisplayMode displayMode = enabled ? win->ctrl->GetDisplayMode() : gGlobalPrefs->defaultDisplayModeEnum;

    for (int id = IDM_VIEW_LAYOUT_FIRST; id <= IDM_VIEW_LAYOUT_LAST; id++) {
        win::menu::SetEnabled(win->menu, id, enabled);
    }

    UINT id = 0;
    if (IsSingle(displayMode))
        id = IDM_VIEW_SINGLE_PAGE;
    else if (IsFacing(displayMode))
        id = IDM_VIEW_FACING;
    else if (IsBookView(displayMode))
        id = IDM_VIEW_BOOK;
    else
        AssertCrash(!win->ctrl && DM_AUTOMATIC == displayMode);

    CheckMenuRadioItem(win->menu, IDM_VIEW_LAYOUT_FIRST, IDM_VIEW_LAYOUT_LAST, id, MF_BYCOMMAND);
    win::menu::SetChecked(win->menu, IDM_VIEW_CONTINUOUS, IsContinuous(displayMode));

    if (Engine_ComicBook == win->GetEngineType()) {
        bool mangaMode = win->AsFixed()->GetDisplayR2L();
        win::menu::SetChecked(win->menu, IDM_VIEW_MANGA_MODE, mangaMode);
    }
}

//[ ACCESSKEY_GROUP File Menu
static MenuDef menuDefFile[] = {
    { _TRN("&Open...\tCtrl+O"),             IDM_OPEN ,                  MF_REQ_DISK_ACCESS },
    { _TRN("&Close\tCtrl+W"),               IDM_CLOSE,                  MF_REQ_DISK_ACCESS },
    { _TRN("&Save As...\tCtrl+S"),          IDM_SAVEAS,                 MF_REQ_DISK_ACCESS },
//[ ACCESSKEY_ALTERNATIVE // only one of these two will be shown
#ifdef ENABLE_SAVE_SHORTCUT
    { _TRN("Save S&hortcut...\tCtrl+Shift+S"), IDM_SAVEAS_BOOKMARK,     MF_REQ_DISK_ACCESS | MF_NOT_FOR_CHM | MF_NOT_FOR_EBOOK_UI },
//| ACCESSKEY_ALTERNATIVE
#else
    { _TRN("Re&name...\tF2"),               IDM_RENAME_FILE,            MF_REQ_DISK_ACCESS },
#endif
//] ACCESSKEY_ALTERNATIVE
    { _TRN("&Print...\tCtrl+P"),            IDM_PRINT,                  MF_REQ_PRINTER_ACCESS | MF_NOT_FOR_EBOOK_UI },
    { SEP_ITEM,                             0,                          MF_REQ_DISK_ACCESS },
//[ ACCESSKEY_ALTERNATIVE // PDF/XPS/CHM specific items are dynamically removed in RebuildFileMenu
    { _TRN("Open in &Adobe Reader"),        IDM_VIEW_WITH_ACROBAT,      MF_REQ_DISK_ACCESS | MF_NOT_FOR_EBOOK_UI },
    { _TRN("Open in &Foxit Reader"),        IDM_VIEW_WITH_FOXIT,        MF_REQ_DISK_ACCESS | MF_NOT_FOR_EBOOK_UI },
    { _TRN("Open &in PDF-XChange"),         IDM_VIEW_WITH_PDF_XCHANGE,  MF_REQ_DISK_ACCESS | MF_NOT_FOR_EBOOK_UI },
//| ACCESSKEY_ALTERNATIVE
    { _TRN("Open in &Microsoft XPS-Viewer"),IDM_VIEW_WITH_XPS_VIEWER,   MF_REQ_DISK_ACCESS | MF_NOT_FOR_EBOOK_UI },
//| ACCESSKEY_ALTERNATIVE
    { _TRN("Open in &Microsoft HTML Help"), IDM_VIEW_WITH_HTML_HELP,    MF_REQ_DISK_ACCESS | MF_NOT_FOR_EBOOK_UI },
//] ACCESSKEY_ALTERNATIVE
    // further entries are added if specified in gGlobalPrefs.vecCommandLine
    { _TRN("Send by &E-mail..."),           IDM_SEND_BY_EMAIL,          MF_REQ_DISK_ACCESS },
    { SEP_ITEM,                             0,                          MF_REQ_DISK_ACCESS },
    { _TRN("P&roperties\tCtrl+D"),          IDM_PROPERTIES,             0 },
    { SEP_ITEM,                             0,                          0 },
    { _TRN("E&xit\tCtrl+Q"),                IDM_EXIT,                   0 }
};
//] ACCESSKEY_GROUP File Menu

//[ ACCESSKEY_GROUP View Menu
static MenuDef menuDefView[] = {
    { _TRN("&Single Page\tCtrl+6"),         IDM_VIEW_SINGLE_PAGE,       MF_NOT_FOR_CHM },
    { _TRN("&Facing\tCtrl+7"),              IDM_VIEW_FACING,            MF_NOT_FOR_CHM },
    { _TRN("&Book View\tCtrl+8"),           IDM_VIEW_BOOK,              MF_NOT_FOR_CHM | MF_NOT_FOR_EBOOK_UI },
    { _TRN("Show &Pages Continuously"),     IDM_VIEW_CONTINUOUS,        MF_NOT_FOR_CHM | MF_NOT_FOR_EBOOK_UI },
    // TODO: "&Inverse Reading Direction" (since some Mangas might be read left-to-right)?
    { _TRN("Man&ga Mode"),                  IDM_VIEW_MANGA_MODE,        MF_CBX_ONLY },
    { SEP_ITEM,                             0,                          MF_NOT_FOR_CHM },
    { _TRN("Rotate &Left\tCtrl+Shift+-"),   IDM_VIEW_ROTATE_LEFT,       MF_NOT_FOR_CHM | MF_NOT_FOR_EBOOK_UI },
    { _TRN("Rotate &Right\tCtrl+Shift++"),  IDM_VIEW_ROTATE_RIGHT,      MF_NOT_FOR_CHM | MF_NOT_FOR_EBOOK_UI },
    { SEP_ITEM,                             0,                          MF_NOT_FOR_CHM | MF_NOT_FOR_EBOOK_UI },
    { _TRN("Pr&esentation\tF5"),            IDM_VIEW_PRESENTATION_MODE, MF_REQ_FULLSCREEN | MF_NOT_FOR_CHM | MF_NOT_FOR_EBOOK_UI },
    { _TRN("F&ullscreen\tF11"),             IDM_VIEW_FULLSCREEN,        MF_REQ_FULLSCREEN },
    { SEP_ITEM,                             0,                          MF_REQ_FULLSCREEN },
    { _TRN("Book&marks\tF12"),              IDM_VIEW_BOOKMARKS,         0 },
    { _TRN("Show &Toolbar"),                IDM_VIEW_SHOW_HIDE_TOOLBAR, MF_NOT_FOR_EBOOK_UI },
    { SEP_ITEM,                             0,                          MF_REQ_ALLOW_COPY | MF_NOT_FOR_EBOOK_UI },
    { _TRN("Select &All\tCtrl+A"),          IDM_SELECT_ALL,             MF_REQ_ALLOW_COPY | MF_NOT_FOR_EBOOK_UI },
    { _TRN("&Copy Selection\tCtrl+C"),      IDM_COPY_SELECTION,         MF_REQ_ALLOW_COPY | MF_NOT_FOR_EBOOK_UI },
};
//] ACCESSKEY_GROUP View Menu

//[ ACCESSKEY_GROUP GoTo Menu
static MenuDef menuDefGoTo[] = {
    { _TRN("&Next Page\tRight Arrow"),      IDM_GOTO_NEXT_PAGE,         0 },
    { _TRN("&Previous Page\tLeft Arrow"),   IDM_GOTO_PREV_PAGE,         0 },
    { _TRN("&First Page\tHome"),            IDM_GOTO_FIRST_PAGE,        0 },
    { _TRN("&Last Page\tEnd"),              IDM_GOTO_LAST_PAGE,         0 },
    { _TRN("Pa&ge...\tCtrl+G"),             IDM_GOTO_PAGE,              0 },
    { SEP_ITEM,                             0,                          0 },
    { _TRN("&Back\tAlt+Left Arrow"),        IDM_GOTO_NAV_BACK,          0 },
    { _TRN("F&orward\tAlt+Right Arrow"),    IDM_GOTO_NAV_FORWARD,       0 },
    { SEP_ITEM,                             0,                          MF_NOT_FOR_EBOOK_UI },
    { _TRN("Fin&d...\tCtrl+F"),             IDM_FIND_FIRST,             MF_NOT_FOR_EBOOK_UI },
};
//] ACCESSKEY_GROUP GoTo Menu

//[ ACCESSKEY_GROUP Zoom Menu
// the entire menu is MF_NOT_FOR_EBOOK_UI
static MenuDef menuDefZoom[] = {
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
//] ACCESSKEY_GROUP Zoom Menu

//[ ACCESSKEY_GROUP Settings Menu
static MenuDef menuDefSettings[] = {
    { _TRN("Change Language"),              IDM_CHANGE_LANGUAGE,        0  },
#if 0
    { _TRN("Contribute Translation"),       IDM_CONTRIBUTE_TRANSLATION, MF_REQ_DISK_ACCESS },
    { SEP_ITEM,                             0,                          MF_REQ_DISK_ACCESS },
#endif
    { _TRN("&Options..."),                  IDM_OPTIONS,                MF_REQ_PREF_ACCESS },
    { _TRN("&Advanced Options..."),         IDM_ADVANCED_OPTIONS,       MF_REQ_PREF_ACCESS | MF_REQ_DISK_ACCESS },
};
//] ACCESSKEY_GROUP Settings Menu

//[ ACCESSKEY_GROUP Favorites Menu
// the entire menu is MF_NOT_FOR_EBOOK_UI
MenuDef menuDefFavorites[] = {
    { _TRN("Add to favorites"),             IDM_FAV_ADD,                0 },
    { _TRN("Remove from favorites"),        IDM_FAV_DEL,                0 },
    { _TRN("Show Favorites"),               IDM_FAV_TOGGLE,             MF_REQ_DISK_ACCESS },
};
//] ACCESSKEY_GROUP Favorites Menu

//[ ACCESSKEY_GROUP Help Menu
static MenuDef menuDefHelp[] = {
    { _TRN("Visit &Website"),               IDM_VISIT_WEBSITE,          MF_REQ_DISK_ACCESS },
    { _TRN("&Manual"),                      IDM_MANUAL,                 MF_REQ_DISK_ACCESS },
    { _TRN("Check for &Updates"),           IDM_CHECK_UPDATE,           MF_REQ_INET_ACCESS },
    { SEP_ITEM,                             0,                          MF_REQ_DISK_ACCESS },
    { _TRN("&About"),                       IDM_ABOUT,                  0 },
};
//] ACCESSKEY_GROUP Help Menu

#ifdef SHOW_DEBUG_MENU_ITEMS
//[ ACCESSKEY_GROUP Debug Menu
static MenuDef menuDefDebug[] = {
    { "Highlight links",                    IDM_DEBUG_SHOW_LINKS,       MF_NO_TRANSLATE },
    { "Toggle PDF/XPS renderer",            IDM_DEBUG_GDI_RENDERER,     MF_NO_TRANSLATE },
    { "Toggle ebook UI",                    IDM_DEBUG_EBOOK_UI,         MF_NO_TRANSLATE },
    { "Mui debug paint",                    IDM_DEBUG_MUI,              MF_NO_TRANSLATE },
    { "Annotation from Selection",          IDM_DEBUG_ANNOTATION,       MF_NO_TRANSLATE },
    { SEP_ITEM,                             0,                          0 },
    { "Crash me",                           IDM_DEBUG_CRASH_ME,         MF_NO_TRANSLATE },
};
//] ACCESSKEY_GROUP Debug Menu
#endif

//[ ACCESSKEY_GROUP Context Menu (Content)
// the entire menu is MF_NOT_FOR_CHM | MF_NOT_FOR_EBOOK_UI
static MenuDef menuDefContext[] = {
    { _TRN("&Copy Selection"),              IDM_COPY_SELECTION,         MF_REQ_ALLOW_COPY },
    { _TRN("Copy &Link Address"),           IDM_COPY_LINK_TARGET,       MF_REQ_ALLOW_COPY },
    { _TRN("Copy Co&mment"),                IDM_COPY_COMMENT,           MF_REQ_ALLOW_COPY },
    { _TRN("Copy &Image"),                  IDM_COPY_IMAGE,             MF_REQ_ALLOW_COPY },
    { SEP_ITEM,                             0,                          MF_REQ_ALLOW_COPY },
    { _TRN("Select &All"),                  IDM_SELECT_ALL,             MF_REQ_ALLOW_COPY },
    { SEP_ITEM,                             0,                          MF_PLUGIN_MODE_ONLY | MF_REQ_ALLOW_COPY },
    { _TRN("&Save As..."),                  IDM_SAVEAS,                 MF_PLUGIN_MODE_ONLY | MF_REQ_DISK_ACCESS },
    { _TRN("&Print..."),                    IDM_PRINT,                  MF_PLUGIN_MODE_ONLY | MF_REQ_PRINTER_ACCESS },
    { _TRN("Show &Bookmarks"),              IDM_VIEW_BOOKMARKS,         MF_PLUGIN_MODE_ONLY },
    { _TRN("P&roperties"),                  IDM_PROPERTIES,             MF_PLUGIN_MODE_ONLY },
};
//] ACCESSKEY_GROUP Context Menu (Content)

//[ ACCESSKEY_GROUP Context Menu (Start)
static MenuDef menuDefContextStart[] = {
    { _TRN("&Open Document"),               IDM_OPEN_SELECTED_DOCUMENT, MF_REQ_DISK_ACCESS },
    { _TRN("&Pin Document"),                IDM_PIN_SELECTED_DOCUMENT,  MF_REQ_DISK_ACCESS | MF_REQ_PREF_ACCESS },
    { SEP_ITEM,                             0,                          MF_REQ_DISK_ACCESS | MF_REQ_PREF_ACCESS },
    { _TRN("&Remove Document"),             IDM_FORGET_SELECTED_DOCUMENT, MF_REQ_DISK_ACCESS | MF_REQ_PREF_ACCESS },
};
//] ACCESSKEY_GROUP Context Menu (Start)

HMENU BuildMenuFromMenuDef(MenuDef menuDefs[], int menuLen, HMENU menu, int flagFilter)
{
    assert(menu);
    bool wasSeparator = true;
    if (!gPluginMode)
        flagFilter |= MF_PLUGIN_MODE_ONLY;

    for (int i = 0; i < menuLen; i++) {
        MenuDef md = menuDefs[i];
        if ((md.flags & flagFilter))
            continue;
        if (!HasPermission(md.flags >> PERM_FLAG_OFFSET))
            continue;

        if (str::Eq(md.title, SEP_ITEM)) {
            // prevent two consecutive separators
            if (!wasSeparator)
                AppendMenu(menu, MF_SEPARATOR, 0, NULL);
            wasSeparator = true;
        } else if (MF_NO_TRANSLATE == (md.flags & MF_NO_TRANSLATE)) {
            ScopedMem<WCHAR> tmp(str::conv::FromUtf8(md.title));
            AppendMenu(menu, MF_STRING, (UINT_PTR)md.id, tmp);
            wasSeparator = false;
        } else {
            const WCHAR *tmp = trans::GetTranslation(md.title);
            AppendMenu(menu, MF_STRING, (UINT_PTR)md.id, tmp);
            wasSeparator = false;
        }
    }

    // TODO: remove trailing separator if there ever is one
    CrashIf(wasSeparator);
    return menu;
}

static void AddFileMenuItem(HMENU menuFile, const WCHAR *filePath, UINT index)
{
    assert(filePath && menuFile);
    if (!filePath || !menuFile) return;

    ScopedMem<WCHAR> fileName(win::menu::ToSafeString(path::GetBaseName(filePath)));
    ScopedMem<WCHAR> menuString(str::Format(L"&%d) %s", (index + 1) % 10, fileName.Get()));
    UINT menuId = IDM_FILE_HISTORY_FIRST + index;
    InsertMenu(menuFile, IDM_EXIT, MF_BYCOMMAND | MF_ENABLED | MF_STRING, menuId, menuString);
}

static void AppendRecentFilesToMenu(HMENU m)
{
    if (!HasPermission(Perm_DiskAccess)) return;

    int i;
    for (i = 0; i < FILE_HISTORY_MAX_RECENT; i++) {
        DisplayState *state = gFileHistory.Get(i);
        if (!state || state->isMissing)
            break;
        assert(state->filePath);
        if (state->filePath)
            AddFileMenuItem(m, state->filePath, i);
    }

    if (i > 0)
        InsertMenu(m, IDM_EXIT, MF_BYCOMMAND | MF_SEPARATOR, 0, NULL);
}

static void AppendExternalViewersToMenu(HMENU menuFile, const WCHAR *filePath)
{
    if (0 == gGlobalPrefs->externalViewers->Count())
        return;
    if (!HasPermission(Perm_DiskAccess) || !file::Exists(filePath))
        return;

    const int maxEntries = IDM_OPEN_WITH_EXTERNAL_LAST - IDM_OPEN_WITH_EXTERNAL_FIRST + 1;
    int count = 0;
    for (size_t i = 0; i < gGlobalPrefs->externalViewers->Count() && count < maxEntries; i++) {
        ExternalViewer *ev = gGlobalPrefs->externalViewers->At(i);
        if (!ev->commandLine)
            continue;
        if (ev->filter && !str::Eq(ev->filter, L"*") && !(filePath && path::Match(filePath, ev->filter)))
            continue;

        ScopedMem<WCHAR> appName;
        const WCHAR *name = ev->name;
        if (str::IsEmpty(name)) {
            WStrVec args;
            ParseCmdLine(ev->commandLine, args, 2);
            if (args.Count() == 0)
                continue;
            appName.Set(str::Dup(path::GetBaseName(args.At(0))));
            *(WCHAR *)path::GetExt(appName) = '\0';
        }

        ScopedMem<WCHAR> menuString(str::Format(_TR("Open in %s"), appName ? appName : name));
        UINT menuId = IDM_OPEN_WITH_EXTERNAL_FIRST + count;
        InsertMenu(menuFile, IDM_SEND_BY_EMAIL, MF_BYCOMMAND | MF_ENABLED | MF_STRING, menuId, menuString);
        if (!filePath)
            win::menu::SetEnabled(menuFile, menuId, false);
        count++;
    }
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
    float zoomVirtual = gGlobalPrefs->defaultZoomFloat;
    if (win->IsDocLoaded())
        zoomVirtual = win->ctrl->GetZoomVirtual();
    UINT menuId = MenuIdFromVirtualZoom(zoomVirtual);
    ZoomMenuItemCheck(win->menu, menuId, win->IsDocLoaded());
}

void MenuUpdatePrintItem(WindowInfo* win, HMENU menu, bool disableOnly=false) {
    bool filePrintEnabled = win->IsDocLoaded();
#ifndef DISABLE_DOCUMENT_RESTRICTIONS
    bool filePrintAllowed = !filePrintEnabled || !win->AsFixed() || win->AsFixed()->GetEngine()->AllowsPrinting();
#else
    bool filePrintAllowed = true;
#endif

    int ix;
    for (ix = 0; ix < dimof(menuDefFile) && menuDefFile[ix].id != IDM_PRINT; ix++);
    assert(ix < dimof(menuDefFile));
    if (ix < dimof(menuDefFile)) {
        const WCHAR *printItem = trans::GetTranslation(menuDefFile[ix].title);
        if (!filePrintAllowed)
            printItem = _TR("&Print... (denied)");
        if (!filePrintAllowed || !disableOnly)
            ModifyMenu(menu, IDM_PRINT, MF_BYCOMMAND | MF_STRING, IDM_PRINT, printItem);
    }

    win::menu::SetEnabled(menu, IDM_PRINT, filePrintEnabled && filePrintAllowed);
}

static bool IsFileCloseMenuEnabled()
{
    for (size_t i = 0; i < gWindows.Count(); i++) {
        if (!gWindows.At(i)->IsAboutWindow())
            return true;
    }
    return false;
}

void MenuUpdateStateForWindow(WindowInfo* win)
{
    // those menu items will be disabled if no document is opened, enabled otherwise
    static UINT menusToDisableIfNoDocument[] = {
        IDM_VIEW_ROTATE_LEFT, IDM_VIEW_ROTATE_RIGHT, IDM_GOTO_NEXT_PAGE, IDM_GOTO_PREV_PAGE,
        IDM_GOTO_FIRST_PAGE, IDM_GOTO_LAST_PAGE, IDM_GOTO_NAV_BACK, IDM_GOTO_NAV_FORWARD,
        IDM_GOTO_PAGE, IDM_FIND_FIRST, IDM_SAVEAS, IDM_SAVEAS_BOOKMARK, IDM_SEND_BY_EMAIL,
        IDM_SELECT_ALL, IDM_COPY_SELECTION, IDM_PROPERTIES, IDM_VIEW_PRESENTATION_MODE,
        IDM_VIEW_WITH_ACROBAT, IDM_VIEW_WITH_FOXIT, IDM_VIEW_WITH_PDF_XCHANGE,
        IDM_RENAME_FILE, IDM_DEBUG_ANNOTATION,
        // IDM_VIEW_WITH_XPS_VIEWER and IDM_VIEW_WITH_HTML_HELP
        // are removed instead of disabled (and can remain enabled
        // for broken XPS/CHM documents)
    };
    static UINT menusToDisableIfDirectory[] = {
        IDM_RENAME_FILE, IDM_SEND_BY_EMAIL
    };
    static UINT menusToEnableIfBrokenPDF[] = {
        IDM_VIEW_WITH_ACROBAT, IDM_VIEW_WITH_FOXIT, IDM_VIEW_WITH_PDF_XCHANGE,
    };

    for (int i = 0; i < dimof(menusToDisableIfNoDocument); i++) {
        UINT id = menusToDisableIfNoDocument[i];
        win::menu::SetEnabled(win->menu, id, win->IsDocLoaded());
    }

    CrashIf(IsFileCloseMenuEnabled() == win->IsAboutWindow());
    win::menu::SetEnabled(win->menu, IDM_CLOSE, IsFileCloseMenuEnabled());

    MenuUpdatePrintItem(win, win->menu);

    bool enabled = win->IsDocLoaded() && win->ctrl->HasTocTree();
    win::menu::SetEnabled(win->menu, IDM_VIEW_BOOKMARKS, enabled);

    bool documentSpecific = win->IsDocLoaded();
    bool checked = documentSpecific ? win->tocVisible : gGlobalPrefs->showToc;
    win::menu::SetChecked(win->menu, IDM_VIEW_BOOKMARKS, checked);

    win::menu::SetChecked(win->menu, IDM_FAV_TOGGLE, gGlobalPrefs->showFavorites);
    win::menu::SetChecked(win->menu, IDM_VIEW_SHOW_HIDE_TOOLBAR, gGlobalPrefs->showToolbar);
    MenuUpdateDisplayMode(win);
    MenuUpdateZoom(win);

    if (win->IsDocLoaded()) {
        win::menu::SetEnabled(win->menu, IDM_GOTO_NAV_BACK, win->ctrl->CanNavigate(-1));
        win::menu::SetEnabled(win->menu, IDM_GOTO_NAV_FORWARD, win->ctrl->CanNavigate(1));
    }

    if (win->GetEngineType() == Engine_ImageDir) {
        for (int i = 0; i < dimof(menusToDisableIfDirectory); i++) {
            UINT id = menusToDisableIfDirectory[i];
            win::menu::SetEnabled(win->menu, id, false);
        }
    }

    if (!win->IsDocLoaded() && !win->IsAboutWindow() && str::EndsWithI(win->loadedFilePath, L".pdf")) {
        for (int i = 0; i < dimof(menusToEnableIfBrokenPDF); i++) {
            UINT id = menusToEnableIfBrokenPDF[i];
            win::menu::SetEnabled(win->menu, id, true);
        }
    }

    if (win->AsFixed())
        win::menu::SetEnabled(win->menu, IDM_FIND_FIRST, !win->AsFixed()->GetEngine()->IsImageCollection());

    // TODO: is this check too expensive?
    if (win->IsDocLoaded() && !file::Exists(win->ctrl->FilePath()))
        win::menu::SetEnabled(win->menu, IDM_RENAME_FILE, false);

#ifdef SHOW_DEBUG_MENU_ITEMS
    win::menu::SetChecked(win->menu, IDM_DEBUG_SHOW_LINKS, gDebugShowLinks);
    win::menu::SetChecked(win->menu, IDM_DEBUG_GDI_RENDERER, gUseGdiRenderer);
    win::menu::SetChecked(win->menu, IDM_DEBUG_EBOOK_UI, gGlobalPrefs->ebookUI.useFixedPageUI);
    win::menu::SetChecked(win->menu, IDM_DEBUG_MUI, mui::IsDebugPaint());
    win::menu::SetEnabled(win->menu, IDM_DEBUG_ANNOTATION, win->AsFixed() && win->AsFixed()->GetEngine()->SupportsAnnotation() &&
                                                           win->showSelection && win->currentTab->selectionOnPage);
#endif
}

void OnAboutContextMenu(WindowInfo* win, int x, int y)
{
    if (!HasPermission(Perm_SavePreferences | Perm_DiskAccess) || !gGlobalPrefs->rememberOpenedFiles || !gGlobalPrefs->showStartPage)
        return;

    const WCHAR *filePath = GetStaticLink(win->staticLinks, x, y);
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
        {
            LoadArgs args(filePath, win);
            LoadDocument(args);
        }
        break;

    case IDM_PIN_SELECTED_DOCUMENT:
        state->isPinned = !state->isPinned;
        win->DeleteInfotip();
        win->RedrawAll(true);
        break;

    case IDM_FORGET_SELECTED_DOCUMENT:
        if (state->favorites->Count() > 0) {
            // just hide documents with favorites
            gFileHistory.MarkFileInexistent(state->filePath, true);
        }
        else {
            gFileHistory.Remove(state);
            DeleteDisplayState(state);
        }
        CleanUpThumbnailCache(gFileHistory);
        win->DeleteInfotip();
        win->RedrawAll(true);
        break;
    }
    DestroyMenu(popup);
}

void OnContextMenu(WindowInfo* win, int x, int y)
{
    CrashIf(!win->AsFixed());
    if (!win->AsFixed())
        return;

    PageElement *pageEl = win->AsFixed()->GetElementAtPos(PointI(x, y));
    ScopedMem<WCHAR> value;
    if (pageEl)
        value.Set(pageEl->GetValue());

    HMENU popup = BuildMenuFromMenuDef(menuDefContext, dimof(menuDefContext), CreatePopupMenu());
    if (!pageEl || pageEl->GetType() != Element_Link || !value)
        win::menu::Remove(popup, IDM_COPY_LINK_TARGET);
    if (!pageEl || pageEl->GetType() != Element_Comment || !value)
        win::menu::Remove(popup, IDM_COPY_COMMENT);
    if (!pageEl || pageEl->GetType() != Element_Image)
        win::menu::Remove(popup, IDM_COPY_IMAGE);

    if (!win->currentTab->selectionOnPage)
        win::menu::SetEnabled(popup, IDM_COPY_SELECTION, false);
    MenuUpdatePrintItem(win, popup, true);
    win::menu::SetEnabled(popup, IDM_VIEW_BOOKMARKS, win->ctrl->HasTocTree());
    win::menu::SetChecked(popup, IDM_VIEW_BOOKMARKS, win->tocVisible);

    POINT pt = { x, y };
    MapWindowPoints(win->hwndCanvas, HWND_DESKTOP, &pt, 1);
    INT cmd = TrackPopupMenu(popup, TPM_RETURNCMD | TPM_RIGHTBUTTON,
                             pt.x, pt.y, 0, win->hwndFrame, NULL);
    switch (cmd) {
    case IDM_COPY_SELECTION:
    case IDM_SELECT_ALL:
    case IDM_SAVEAS:
    case IDM_PRINT:
    case IDM_VIEW_BOOKMARKS:
    case IDM_PROPERTIES:
        SendMessage(win->hwndFrame, WM_COMMAND, cmd, 0);
        break;

    case IDM_COPY_LINK_TARGET:
    case IDM_COPY_COMMENT:
        CopyTextToClipboard(value);
        break;

    case IDM_COPY_IMAGE:
        if (pageEl) {
            RenderedBitmap *bmp = pageEl->GetImage();
            if (bmp)
                CopyImageToClipboard(bmp->GetBitmap());
            delete bmp;
        }
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
    ZoomToSelection(win, zoom);
}

void OnMenuCustomZoom(WindowInfo* win)
{
    if (!win->IsDocLoaded() || win->AsEbook())
        return;

    float zoom = win->ctrl->GetZoomVirtual();
    if (!Dialog_CustomZoom(win->hwndFrame, win->AsChm(), &zoom))
        return;
    ZoomToSelection(win, zoom);
}

static void RebuildFileMenu(WindowInfo *win, HMENU menu)
{
    win::menu::Empty(menu);
    int filter = win->AsChm() ? MF_NOT_FOR_CHM : win->AsEbook() ? MF_NOT_FOR_EBOOK_UI : 0;
    BuildMenuFromMenuDef(menuDefFile, dimof(menuDefFile), menu, filter);
    AppendRecentFilesToMenu(menu);
    AppendExternalViewersToMenu(menu, win->loadedFilePath);

    // Suppress menu items that depend on specific software being installed:
    // e-mail client, Adobe Reader, Foxit, PDF-XChange
    // Don't hide items here that won't always be hidden
    // (MenuUpdateStateForWindow() is for that)
    if (!CanSendAsEmailAttachment())
        win::menu::Remove(menu, IDM_SEND_BY_EMAIL);

    // Also suppress PDF specific items for non-PDF documents
    if (!CouldBePDFDoc(win) || !CanViewWithAcrobat())
        win::menu::Remove(menu, IDM_VIEW_WITH_ACROBAT);
    if (!CouldBePDFDoc(win) || !CanViewWithFoxit())
        win::menu::Remove(menu, IDM_VIEW_WITH_FOXIT);
    if (!CouldBePDFDoc(win) || !CanViewWithPDFXChange())
        win::menu::Remove(menu, IDM_VIEW_WITH_PDF_XCHANGE);
    if (!CanViewWithXPSViewer(win))
        win::menu::Remove(menu, IDM_VIEW_WITH_XPS_VIEWER);
    if (!CanViewWithHtmlHelp(win))
        win::menu::Remove(menu, IDM_VIEW_WITH_HTML_HELP);
}

//[ ACCESSKEY_GROUP Main Menubar
HMENU BuildMenu(WindowInfo *win)
{
    HMENU mainMenu = CreateMenu();
    int filter = 0;
    if (win->AsChm())
        filter |= MF_NOT_FOR_CHM;
    else if (win->AsEbook())
        filter |= MF_NOT_FOR_EBOOK_UI;
    if (win->GetEngineType() != Engine_ComicBook)
        filter |= MF_CBX_ONLY;

    HMENU m = CreateMenu();
    RebuildFileMenu(win, m);
    AppendMenu(mainMenu, MF_POPUP | MF_STRING, (UINT_PTR)m, _TR("&File"));
    m = BuildMenuFromMenuDef(menuDefView, dimof(menuDefView), CreateMenu(), filter);
    AppendMenu(mainMenu, MF_POPUP | MF_STRING, (UINT_PTR)m, _TR("&View"));
    m = BuildMenuFromMenuDef(menuDefGoTo, dimof(menuDefGoTo), CreateMenu(), filter);
    AppendMenu(mainMenu, MF_POPUP | MF_STRING, (UINT_PTR)m, _TR("&Go To"));
    if (!win->AsEbook()) {
        m = BuildMenuFromMenuDef(menuDefZoom, dimof(menuDefZoom), CreateMenu(), filter);
        AppendMenu(mainMenu, MF_POPUP | MF_STRING, (UINT_PTR)m, _TR("&Zoom"));
    }

    // TODO: implement Favorites for ebooks
    if (HasPermission(Perm_SavePreferences) && !win->AsEbook()) {
        // I think it makes sense to disable favorites in restricted mode
        // because they wouldn't be persisted, anyway
        m = BuildMenuFromMenuDef(menuDefFavorites, dimof(menuDefFavorites), CreateMenu());
        RebuildFavMenu(win, m);
        AppendMenu(mainMenu, MF_POPUP | MF_STRING, (UINT_PTR)m, _TR("F&avorites"));
    }
    m = BuildMenuFromMenuDef(menuDefSettings, dimof(menuDefSettings), CreateMenu(), filter);
    AppendMenu(mainMenu, MF_POPUP | MF_STRING, (UINT_PTR)m, _TR("&Settings"));
    m = BuildMenuFromMenuDef(menuDefHelp, dimof(menuDefHelp), CreateMenu(), filter);
    AppendMenu(mainMenu, MF_POPUP | MF_STRING, (UINT_PTR)m, _TR("&Help"));
#if 0
    // cf. MenuBarAsPopupMenu in Caption.cpp
    m = GetSystemMenu(win->hwndFrame, FALSE);
    AppendMenu(mainMenu, MF_POPUP | MF_STRING, (UINT_PTR)m, _TR("&Window"));
#endif
#ifdef SHOW_DEBUG_MENU_ITEMS
    m = BuildMenuFromMenuDef(menuDefDebug, dimof(menuDefDebug), CreateMenu(), filter);
    AppendMenu(mainMenu, MF_POPUP | MF_STRING, (UINT_PTR)m, L"Debug");
#endif

    return mainMenu;
}
//] ACCESSKEY_GROUP Main Menubar

void UpdateMenu(WindowInfo *win, HMENU m)
{
    CrashIf(!win);
    UINT id = GetMenuItemID(m, 0);
    if (id == menuDefFile[0].id)
        RebuildFileMenu(win, m);
    else if (id == menuDefFavorites[0].id) {
        win::menu::Empty(m);
        BuildMenuFromMenuDef(menuDefFavorites, dimof(menuDefFavorites), m);
        RebuildFavMenu(win, m);
    }
    MenuUpdateStateForWindow(win);
}

// show/hide top-level menu bar. This doesn't persist across launches
// so that accidental removal of the menu isn't catastrophic
void ShowHideMenuBar(WindowInfo *win, bool showTemporarily)
{
    CrashIf(!win->menu);
    if (win->presentation || win->isFullScreen)
        return;

    HWND hwnd = win->hwndFrame;

    if (showTemporarily) {
        SetMenu(hwnd, win->menu);
        return;
    }

    bool hideMenu = !showTemporarily && GetMenu(hwnd) != NULL;
    SetMenu(hwnd, hideMenu ? NULL : win->menu);
    win->isMenuHidden = hideMenu;
}
