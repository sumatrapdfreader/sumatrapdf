/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/CmdLineParser.h"
#include "utils/FileUtil.h"
#include "utils/HtmlParserLookup.h"
#include "mui/Mui.h"
#include "utils/WinUtil.h"

#include "wingui/TreeModel.h"
#include "EngineBase.h"
#include "EngineManager.h"

#include "SettingsStructs.h"
#include "Controller.h"
#include "DisplayModel.h"
#include "FileHistory.h"
#include "Theme.h"
#include "GlobalPrefs.h"
#include "AppColors.h"
#include "ProgressUpdateUI.h"
#include "Notifications.h"
#include "SumatraConfig.h"
#include "SumatraPDF.h"
#include "WindowInfo.h"
#include "TabInfo.h"
#include "resource.h"
#include "ExternalViewers.h"
#include "Favorites.h"
#include "FileThumbnails.h"
#include "Menu.h"
#include "Selection.h"
#include "SumatraAbout.h"
#include "SumatraDialogs.h"
#include "Translations.h"
#include "utils/BitManip.h"
#include "utils/Dpi.h"

bool gAddCrashMeMenu = false;

void MenuUpdateDisplayMode(WindowInfo* win) {
    bool enabled = win->IsDocLoaded();
    DisplayMode displayMode = gGlobalPrefs->defaultDisplayModeEnum;
    if (enabled) {
        displayMode = win->ctrl->GetDisplayMode();
    }

    for (int id = IDM_VIEW_LAYOUT_FIRST; id <= IDM_VIEW_LAYOUT_LAST; id++) {
        win::menu::SetEnabled(win->menu, id, enabled);
    }

    UINT id = 0;
    if (IsSingle(displayMode)) {
        id = IDM_VIEW_SINGLE_PAGE;
    } else if (IsFacing(displayMode)) {
        id = IDM_VIEW_FACING;
    } else if (IsBookView(displayMode)) {
        id = IDM_VIEW_BOOK;
    } else {
        AssertCrash(!win->ctrl && DM_AUTOMATIC == displayMode);
    }

    CheckMenuRadioItem(win->menu, IDM_VIEW_LAYOUT_FIRST, IDM_VIEW_LAYOUT_LAST, id, MF_BYCOMMAND);
    win::menu::SetChecked(win->menu, IDM_VIEW_CONTINUOUS, IsContinuous(displayMode));

    if (win->currentTab && win->currentTab->GetEngineType() == kindEngineComicBooks) {
        bool mangaMode = win->AsFixed()->GetDisplayR2L();
        win::menu::SetChecked(win->menu, IDM_VIEW_MANGA_MODE, mangaMode);
    }
}

// clang-format off
//[ ACCESSKEY_GROUP File Menu
static MenuDef menuDefFile[] = {
    { _TRN("New &window\tCtrl+N"),          IDM_NEW_WINDOW,             MF_REQ_DISK_ACCESS },
    { _TRN("&Open...\tCtrl+O"),             IDM_OPEN ,                  MF_REQ_DISK_ACCESS },
    { _TRN("&Close\tCtrl+W"),               IDM_CLOSE,                  MF_REQ_DISK_ACCESS },
    { _TRN("Show in &folder"),              IDM_SHOW_IN_FOLDER,         MF_REQ_DISK_ACCESS },
    { _TRN("&Save As...\tCtrl+S"),          IDM_SAVEAS,                 MF_REQ_DISK_ACCESS },
    { _TRN("Save Annotations"),             IDM_SAVE_ANNOTATIONS_SMX,   MF_REQ_DISK_ACCESS },
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
    { _TRN("Show Book&marks\tF12"),         IDM_VIEW_BOOKMARKS,         0 },
    { _TRN("Show &Toolbar\tF8"),            IDM_VIEW_SHOW_HIDE_TOOLBAR, MF_NOT_FOR_EBOOK_UI },
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
    { _TRN("Change Language"),              IDM_CHANGE_LANGUAGE,        0 },
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

//[ ACCESSKEY_GROUP Debug Menu
static MenuDef menuDefDebug[] = {
    { "Highlight links",                    IDM_DEBUG_SHOW_LINKS,       MF_NO_TRANSLATE },
    { "Toggle ebook UI",                    IDM_DEBUG_EBOOK_UI,         MF_NO_TRANSLATE },
    { "Mui debug paint",                    IDM_DEBUG_MUI,              MF_NO_TRANSLATE },
    { "Annotation from Selection",          IDM_DEBUG_ANNOTATION,       MF_NO_TRANSLATE },
    { "Download symbols",                   IDM_DEBUG_DOWNLOAD_SYMBOLS, MF_NO_TRANSLATE },
};
//] ACCESSKEY_GROUP Debug Menu

//[ ACCESSKEY_GROUP Context Menu (Content)
// the entire menu is MF_NOT_FOR_CHM | MF_NOT_FOR_EBOOK_UI
static MenuDef menuDefContext[] = {
    { _TRN("&Copy Selection"),              IDM_COPY_SELECTION,         MF_REQ_ALLOW_COPY },
    { _TRN("Copy &Link Address"),           IDM_COPY_LINK_TARGET,       MF_REQ_ALLOW_COPY },
    { _TRN("Copy Co&mment"),                IDM_COPY_COMMENT,           MF_REQ_ALLOW_COPY },
    { _TRN("Copy &Image"),                  IDM_COPY_IMAGE,             MF_REQ_ALLOW_COPY },
    { _TRN("Select &All"),                  IDM_SELECT_ALL,             MF_REQ_ALLOW_COPY },
    { SEP_ITEM,                             0,                          MF_REQ_ALLOW_COPY },
    // note: strings cannot be "" or else items are not there
    {"add",                                 IDM_FAV_ADD,                MF_NO_TRANSLATE   },
    {"del",                                 IDM_FAV_DEL,                MF_NO_TRANSLATE   },
    { _TRN("Show &Favorites"),              IDM_FAV_TOGGLE,             0                 },
    { _TRN("Show &Bookmarks\tF12"),         IDM_VIEW_BOOKMARKS,         0                 },
    { _TRN("Show &Toolbar\tF8"),            IDM_VIEW_SHOW_HIDE_TOOLBAR, MF_NOT_FOR_EBOOK_UI },
    { _TRN("Save Annotations"),             IDM_SAVE_ANNOTATIONS_SMX,   MF_REQ_DISK_ACCESS },
    { SEP_ITEM,                             0,                          MF_PLUGIN_MODE_ONLY | MF_REQ_ALLOW_COPY },
    { _TRN("&Save As..."),                  IDM_SAVEAS,                 MF_PLUGIN_MODE_ONLY | MF_REQ_DISK_ACCESS },
    { _TRN("&Print..."),                    IDM_PRINT,                  MF_PLUGIN_MODE_ONLY | MF_REQ_PRINTER_ACCESS },
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
// clang-format on

HMENU BuildMenuFromMenuDef(MenuDef menuDefs[], int menuLen, HMENU menu, int flagFilter) {
    CrashIf(!menu);
    bool wasSeparator = true;
    if (!gPluginMode) {
        flagFilter |= MF_PLUGIN_MODE_ONLY;
    }

    for (int i = 0; i < menuLen; i++) {
        MenuDef md = menuDefs[i];
        if ((md.flags & flagFilter)) {
            continue;
        }
        if (!HasPermission(md.flags >> PERM_FLAG_OFFSET)) {
            continue;
        }

        if (str::Eq(md.title, SEP_ITEM)) {
            // prevent two consecutive separators
            if (!wasSeparator) {
                AppendMenu(menu, MF_SEPARATOR, (UINT_PTR)md.id, nullptr);
            }
            wasSeparator = true;
        } else if (MF_NO_TRANSLATE == (md.flags & MF_NO_TRANSLATE)) {
            AutoFreeWstr tmp = strconv::Utf8ToWstr(md.title);
            AppendMenu(menu, MF_STRING, (UINT_PTR)md.id, tmp);
            wasSeparator = false;
        } else {
            const WCHAR* tmp = trans::GetTranslation(md.title);
            AppendMenu(menu, MF_STRING, (UINT_PTR)md.id, tmp);
            wasSeparator = false;
        }
    }

    // TODO: remove trailing separator if there ever is one
    CrashIf(wasSeparator);
    return menu;
}

static void AddFileMenuItem(HMENU menuFile, const WCHAR* filePath, UINT index) {
    CrashIf(!filePath || !menuFile);
    if (!filePath || !menuFile) {
        return;
    }

    AutoFreeWstr menuString;
    menuString.SetCopy(path::GetBaseNameNoFree(filePath));
    auto fileName = win::menu::ToSafeString(menuString);
    int menuIdx = (int)((index + 1) % 10);
    menuString.Set(str::Format(L"&%d) %s", menuIdx, fileName));
    UINT menuId = IDM_FILE_HISTORY_FIRST + index;
    InsertMenu(menuFile, IDM_EXIT, MF_BYCOMMAND | MF_ENABLED | MF_STRING, menuId, menuString);
}

static void AppendRecentFilesToMenu(HMENU m) {
    if (!HasPermission(Perm_DiskAccess)) {
        return;
    }

    int i;
    for (i = 0; i < FILE_HISTORY_MAX_RECENT; i++) {
        DisplayState* state = gFileHistory.Get(i);
        if (!state || state->isMissing) {
            break;
        }
        AddFileMenuItem(m, state->filePath, i);
    }

    if (i > 0) {
        InsertMenu(m, IDM_EXIT, MF_BYCOMMAND | MF_SEPARATOR, 0, nullptr);
    }
}

static void AppendExternalViewersToMenu(HMENU menuFile, const WCHAR* filePath) {
    if (0 == gGlobalPrefs->externalViewers->size()) {
        return;
    }
    if (!HasPermission(Perm_DiskAccess) || (filePath && !file::Exists(filePath))) {
        return;
    }

    const int maxEntries = IDM_OPEN_WITH_EXTERNAL_LAST - IDM_OPEN_WITH_EXTERNAL_FIRST + 1;
    int count = 0;
    for (size_t i = 0; i < gGlobalPrefs->externalViewers->size() && count < maxEntries; i++) {
        ExternalViewer* ev = gGlobalPrefs->externalViewers->at(i);
        if (!ev->commandLine) {
            continue;
        }
        if (ev->filter && !str::Eq(ev->filter, L"*") && !(filePath && path::Match(filePath, ev->filter))) {
            continue;
        }

        AutoFreeWstr appName;
        const WCHAR* name = ev->name;
        if (str::IsEmpty(name)) {
            WStrVec args;
            ParseCmdLine(ev->commandLine, args, 2);
            if (args.size() == 0) {
                continue;
            }
            appName.SetCopy(path::GetBaseNameNoFree(args.at(0)));
            *(WCHAR*)path::GetExtNoFree(appName) = '\0';
        }

        AutoFreeWstr menuString(str::Format(_TR("Open in %s"), appName ? appName.get() : name));
        UINT menuId = IDM_OPEN_WITH_EXTERNAL_FIRST + count;
        InsertMenu(menuFile, IDM_SEND_BY_EMAIL, MF_BYCOMMAND | MF_ENABLED | MF_STRING, menuId, menuString);
        if (!filePath) {
            win::menu::SetEnabled(menuFile, menuId, false);
        }
        count++;
    }
}

// clang-format off
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
// clang-format on

UINT MenuIdFromVirtualZoom(float virtualZoom) {
    for (int i = 0; i < dimof(gZoomMenuIds); i++) {
        if (virtualZoom == gZoomMenuIds[i].zoom) {
            return gZoomMenuIds[i].itemId;
        }
    }
    return IDM_ZOOM_CUSTOM;
}

static float ZoomMenuItemToZoom(UINT menuItemId) {
    for (int i = 0; i < dimof(gZoomMenuIds); i++) {
        if (menuItemId == gZoomMenuIds[i].itemId) {
            return gZoomMenuIds[i].zoom;
        }
    }
    CrashIf(true);
    return 100.0;
}

static void ZoomMenuItemCheck(HMENU m, UINT menuItemId, bool canZoom) {
    CrashIf((IDM_ZOOM_FIRST > menuItemId) || (menuItemId > IDM_ZOOM_LAST));

    for (int i = 0; i < dimof(gZoomMenuIds); i++) {
        win::menu::SetEnabled(m, gZoomMenuIds[i].itemId, canZoom);
    }

    if (IDM_ZOOM_100 == menuItemId) {
        menuItemId = IDM_ZOOM_ACTUAL_SIZE;
    }
    CheckMenuRadioItem(m, IDM_ZOOM_FIRST, IDM_ZOOM_LAST, menuItemId, MF_BYCOMMAND);
    if (IDM_ZOOM_ACTUAL_SIZE == menuItemId) {
        CheckMenuRadioItem(m, IDM_ZOOM_100, IDM_ZOOM_100, IDM_ZOOM_100, MF_BYCOMMAND);
    }
}

void MenuUpdateZoom(WindowInfo* win) {
    float zoomVirtual = gGlobalPrefs->defaultZoomFloat;
    if (win->IsDocLoaded()) {
        zoomVirtual = win->ctrl->GetZoomVirtual();
    }
    UINT menuId = MenuIdFromVirtualZoom(zoomVirtual);
    ZoomMenuItemCheck(win->menu, menuId, win->IsDocLoaded());
}

void MenuUpdatePrintItem(WindowInfo* win, HMENU menu, bool disableOnly = false) {
    bool filePrintEnabled = win->IsDocLoaded();
#ifndef DISABLE_DOCUMENT_RESTRICTIONS
    bool filePrintAllowed = !filePrintEnabled || !win->AsFixed() || win->AsFixed()->GetEngine()->AllowsPrinting();
#else
    bool filePrintAllowed = true;
#endif

    int idx;
    for (idx = 0; idx < dimof(menuDefFile) && menuDefFile[idx].id != IDM_PRINT; idx++) {
        // do nothing
    }
    if (idx < dimof(menuDefFile)) {
        const WCHAR* printItem = trans::GetTranslation(menuDefFile[idx].title);
        if (!filePrintAllowed) {
            printItem = _TR("&Print... (denied)");
        }
        if (!filePrintAllowed || !disableOnly) {
            ModifyMenu(menu, IDM_PRINT, MF_BYCOMMAND | MF_STRING, IDM_PRINT, printItem);
        }
    }

    win::menu::SetEnabled(menu, IDM_PRINT, filePrintEnabled && filePrintAllowed);
}

static bool IsFileCloseMenuEnabled() {
    for (size_t i = 0; i < gWindows.size(); i++) {
        if (!gWindows.at(i)->IsAboutWindow()) {
            return true;
        }
    }
    return false;
}

void MenuUpdateStateForWindow(WindowInfo* win) {
    // those menu items will be disabled if no document is opened, enabled otherwise
    static UINT menusToDisableIfNoDocument[] = {
        IDM_VIEW_ROTATE_LEFT,
        IDM_VIEW_ROTATE_RIGHT,
        IDM_GOTO_NEXT_PAGE,
        IDM_GOTO_PREV_PAGE,
        IDM_GOTO_FIRST_PAGE,
        IDM_GOTO_LAST_PAGE,
        IDM_GOTO_NAV_BACK,
        IDM_GOTO_NAV_FORWARD,
        IDM_GOTO_PAGE,
        IDM_FIND_FIRST,
        IDM_SAVEAS,
        IDM_SAVEAS_BOOKMARK,
        IDM_SEND_BY_EMAIL,
        IDM_SELECT_ALL,
        IDM_COPY_SELECTION,
        IDM_PROPERTIES,
        IDM_VIEW_PRESENTATION_MODE,
        IDM_VIEW_WITH_ACROBAT,
        IDM_VIEW_WITH_FOXIT,
        IDM_VIEW_WITH_PDF_XCHANGE,
        IDM_RENAME_FILE,
        IDM_SHOW_IN_FOLDER,
        IDM_DEBUG_ANNOTATION,
        // IDM_VIEW_WITH_XPS_VIEWER and IDM_VIEW_WITH_HTML_HELP
        // are removed instead of disabled (and can remain enabled
        // for broken XPS/CHM documents)
    };
    // this list coincides with menusToEnableIfBrokenPDF
    static UINT menusToDisableIfDirectory[] = {
        IDM_RENAME_FILE,     IDM_SEND_BY_EMAIL,         IDM_VIEW_WITH_ACROBAT,
        IDM_VIEW_WITH_FOXIT, IDM_VIEW_WITH_PDF_XCHANGE, IDM_SHOW_IN_FOLDER,
    };
#define menusToEnableIfBrokenPDF menusToDisableIfDirectory

    TabInfo* tab = win->currentTab;

    for (int i = 0; i < dimof(menusToDisableIfNoDocument); i++) {
        UINT id = menusToDisableIfNoDocument[i];
        win::menu::SetEnabled(win->menu, id, win->IsDocLoaded());
    }

    // TODO: happens with UseTabs = false with .pdf files
    SubmitCrashIf(IsFileCloseMenuEnabled() == win->IsAboutWindow());
    win::menu::SetEnabled(win->menu, IDM_CLOSE, IsFileCloseMenuEnabled());

    MenuUpdatePrintItem(win, win->menu);

    bool enabled = win->IsDocLoaded() && tab && tab->ctrl->HacToc();
    win::menu::SetEnabled(win->menu, IDM_VIEW_BOOKMARKS, enabled);

    bool documentSpecific = win->IsDocLoaded();
    bool checked = documentSpecific ? win->tocVisible : gGlobalPrefs->showToc;
    win::menu::SetChecked(win->menu, IDM_VIEW_BOOKMARKS, checked);

    win::menu::SetChecked(win->menu, IDM_FAV_TOGGLE, gGlobalPrefs->showFavorites);
    win::menu::SetChecked(win->menu, IDM_VIEW_SHOW_HIDE_TOOLBAR, gGlobalPrefs->showToolbar);
    MenuUpdateDisplayMode(win);
    MenuUpdateZoom(win);

    if (win->IsDocLoaded() && tab) {
        win::menu::SetEnabled(win->menu, IDM_GOTO_NAV_BACK, tab->ctrl->CanNavigate(-1));
        win::menu::SetEnabled(win->menu, IDM_GOTO_NAV_FORWARD, tab->ctrl->CanNavigate(1));
    }

    // TODO: is this check too expensive?
    bool fileExists = tab && file::Exists(tab->filePath);

    if (tab && tab->ctrl && !fileExists && dir::Exists(tab->filePath)) {
        for (int i = 0; i < dimof(menusToDisableIfDirectory); i++) {
            UINT id = menusToDisableIfDirectory[i];
            win::menu::SetEnabled(win->menu, id, false);
        }
    } else if (fileExists && CouldBePDFDoc(tab)) {
        for (int i = 0; i < dimof(menusToEnableIfBrokenPDF); i++) {
            UINT id = menusToEnableIfBrokenPDF[i];
            win::menu::SetEnabled(win->menu, id, true);
        }
    }

    DisplayModel* dm = tab ? tab->AsFixed() : nullptr;
    EngineBase* engine = dm ? dm->GetEngine() : nullptr;
    if (engine) {
        win::menu::SetEnabled(win->menu, IDM_FIND_FIRST, !engine->IsImageCollection());
    }

    if (win->IsDocLoaded() && !fileExists) {
        win::menu::SetEnabled(win->menu, IDM_RENAME_FILE, false);
    }

#if defined(ENABLE_THEME)
    CheckMenuRadioItem(win->menu, IDM_CHANGE_THEME_FIRST, IDM_CHANGE_THEME_LAST,
                       IDM_CHANGE_THEME_FIRST + GetCurrentThemeIndex(), MF_BYCOMMAND);
#endif

    win::menu::SetChecked(win->menu, IDM_DEBUG_SHOW_LINKS, gDebugShowLinks);
    win::menu::SetChecked(win->menu, IDM_DEBUG_EBOOK_UI, gGlobalPrefs->ebookUI.useFixedPageUI);
    win::menu::SetChecked(win->menu, IDM_DEBUG_MUI, mui::IsDebugPaint());
    win::menu::SetEnabled(win->menu, IDM_DEBUG_ANNOTATION,
                          tab && tab->selectionOnPage && win->showSelection && engine && engine->supportsAnnotations);
}

void OnAboutContextMenu(WindowInfo* win, int x, int y) {
    if (!HasPermission(Perm_SavePreferences | Perm_DiskAccess) || !gGlobalPrefs->rememberOpenedFiles ||
        !gGlobalPrefs->showStartPage) {
        return;
    }

    const WCHAR* filePath = GetStaticLink(win->staticLinks, x, y);
    if (!filePath || *filePath == '<') {
        return;
    }

    DisplayState* state = gFileHistory.Find(filePath, nullptr);
    CrashIf(!state);
    if (!state) {
        return;
    }

    HMENU popup = BuildMenuFromMenuDef(menuDefContextStart, dimof(menuDefContextStart), CreatePopupMenu());
    win::menu::SetChecked(popup, IDM_PIN_SELECTED_DOCUMENT, state->isPinned);
    POINT pt = {x, y};
    MapWindowPoints(win->hwndCanvas, HWND_DESKTOP, &pt, 1);
    MarkMenuOwnerDraw(popup);
    INT cmd = TrackPopupMenu(popup, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, win->hwndFrame, nullptr);
    FreeMenuOwnerDrawInfoData(popup);
    DestroyMenu(popup);

    if (IDM_OPEN_SELECTED_DOCUMENT == cmd) {
        LoadArgs args(filePath, win);
        LoadDocument(args);
        return;
    }

    if (IDM_PIN_SELECTED_DOCUMENT == cmd) {
        state->isPinned = !state->isPinned;
        win->HideInfoTip();
        win->RedrawAll(true);
        return;
    }

    if (IDM_FORGET_SELECTED_DOCUMENT == cmd) {
        if (state->favorites->size() > 0) {
            // just hide documents with favorites
            gFileHistory.MarkFileInexistent(state->filePath, true);
        } else {
            gFileHistory.Remove(state);
            DeleteDisplayState(state);
        }
        CleanUpThumbnailCache(gFileHistory);
        win->HideInfoTip();
        win->RedrawAll(true);
        return;
    }
}

void OnWindowContextMenu(WindowInfo* win, int x, int y) {
    DisplayModel* dm = win->AsFixed();
    CrashIf(!dm);
    if (!dm) {
        return;
    }

    PageElement* pageEl = dm->GetElementAtPos({x, y});
    WCHAR* value = nullptr;
    if (pageEl) {
        value = pageEl->GetValue();
    }

    HMENU popup = BuildMenuFromMenuDef(menuDefContext, dimof(menuDefContext), CreatePopupMenu());
    if (!pageEl || pageEl->kind != kindPageElementDest || !value) {
        win::menu::Remove(popup, IDM_COPY_LINK_TARGET);
    }
    if (!pageEl || pageEl->kind != kindPageElementComment || !value) {
        win::menu::Remove(popup, IDM_COPY_COMMENT);
    }
    if (!pageEl || pageEl->kind != kindPageElementImage) {
        win::menu::Remove(popup, IDM_COPY_IMAGE);
    }
    if (!win->currentTab->selectionOnPage) {
        win::menu::SetEnabled(popup, IDM_COPY_SELECTION, false);
    }
    MenuUpdatePrintItem(win, popup, true);
    win::menu::SetEnabled(popup, IDM_VIEW_BOOKMARKS, win->ctrl->HacToc());
    win::menu::SetChecked(popup, IDM_VIEW_BOOKMARKS, win->tocVisible);

    win::menu::SetEnabled(popup, IDM_FAV_TOGGLE, HasFavorites());
    win::menu::SetChecked(popup, IDM_FAV_TOGGLE, gGlobalPrefs->showFavorites);

    bool supportsAnnotations = false;
    EngineBase* engine = dm ? dm->GetEngine() : nullptr;
    if (engine) {
        supportsAnnotations = engine->supportsAnnotations;
    }
    bool canDoAnnotations = gIsDebugBuild || gIsPreReleaseBuild || gIsDailyBuild;
    if (!canDoAnnotations) {
        supportsAnnotations = false;
    }
    if (!supportsAnnotations) {
        win::menu::Remove(popup, IDM_SAVE_ANNOTATIONS_SMX);
    } else {
        win::menu::SetEnabled(popup, IDM_SAVE_ANNOTATIONS_SMX, dm->userAnnotsModified);
    }

    int pageNo = dm->GetPageNoByPoint({x, y});
    const WCHAR* filePath = win->ctrl->FilePath();
    if (pageNo > 0) {
        AutoFreeWstr pageLabel = win->ctrl->GetPageLabel(pageNo);
        bool isBookmarked = gFavorites.IsPageInFavorites(filePath, pageNo);
        if (isBookmarked) {
            win::menu::Remove(popup, IDM_FAV_ADD);

            // %s and not %d because re-using translation from RebuildFavMenu()
            auto tr = _TR("Remove page %s from favorites");
            AutoFreeWstr s = str::Format(tr, pageLabel.Get());
            win::menu::SetText(popup, IDM_FAV_DEL, s);
        } else {
            win::menu::Remove(popup, IDM_FAV_DEL);

            // %s and not %d because re-using translation from RebuildFavMenu()
            auto tr = _TR("Add page %s to favorites\tCtrl+B");
            AutoFreeWstr s = str::Format(tr, pageLabel.Get());
            win::menu::SetText(popup, IDM_FAV_ADD, s);
        }
    } else {
        win::menu::Remove(popup, IDM_FAV_ADD);
        win::menu::Remove(popup, IDM_FAV_DEL);
    }

    // if toolbar is not shown, add option to show it
    if (gGlobalPrefs->showToolbar) {
        win::menu::Remove(popup, IDM_VIEW_SHOW_HIDE_TOOLBAR);
    }

    POINT pt = {x, y};
    MapWindowPoints(win->hwndCanvas, HWND_DESKTOP, &pt, 1);
    MarkMenuOwnerDraw(popup);
    UINT flags = TPM_RETURNCMD | TPM_RIGHTBUTTON;
    INT cmd = TrackPopupMenu(popup, flags, pt.x, pt.y, 0, win->hwndFrame, nullptr);
    FreeMenuOwnerDrawInfoData(popup);
    DestroyMenu(popup);

    switch (cmd) {
        case IDM_COPY_SELECTION:
        case IDM_SELECT_ALL:
        case IDM_SAVEAS:
        case IDM_PRINT:
        case IDM_VIEW_BOOKMARKS:
        case IDM_FAV_TOGGLE:
        case IDM_PROPERTIES:
        case IDM_VIEW_SHOW_HIDE_TOOLBAR:
        case IDM_SAVE_ANNOTATIONS_SMX:
            SendMessage(win->hwndFrame, WM_COMMAND, cmd, 0);
            break;

        case IDM_COPY_LINK_TARGET:
        case IDM_COPY_COMMENT:
            CopyTextToClipboard(value);
            break;

        case IDM_COPY_IMAGE:
            if (pageEl) {
                RenderedBitmap* bmp = dm->GetEngine()->GetImageForPageElement(pageEl);
                if (bmp) {
                    CopyImageToClipboard(bmp->GetBitmap(), false);
                }
                delete bmp;
            }
            break;
        case IDM_FAV_ADD:
            AddFavoriteForCurrentPage(win);
            break;
        case IDM_FAV_DEL:
            DelFavorite(filePath, pageNo);
            break;
    }

    delete pageEl;
}

/* Zoom document in window 'hwnd' to zoom level 'zoom'.
   'zoom' is given as a floating-point number, 1.0 is 100%, 2.0 is 200% etc.
*/
void OnMenuZoom(WindowInfo* win, UINT menuId) {
    if (!win->IsDocLoaded()) {
        return;
    }

    float zoom = ZoomMenuItemToZoom(menuId);
    ZoomToSelection(win, zoom);
}

void OnMenuCustomZoom(WindowInfo* win) {
    if (!win->IsDocLoaded() || win->AsEbook()) {
        return;
    }

    float zoom = win->ctrl->GetZoomVirtual();
    if (!Dialog_CustomZoom(win->hwndFrame, win->AsChm(), &zoom)) {
        return;
    }
    ZoomToSelection(win, zoom);
}

static void RebuildFileMenu(TabInfo* tab, HMENU menu) {
    int filter = 0;
    if (tab && tab->AsChm()) {
        filter |= MF_NOT_FOR_CHM;
    }
    if (tab && tab->AsEbook()) {
        filter |= MF_NOT_FOR_EBOOK_UI;
    }
    if (!tab || tab->GetEngineType() != kindEngineComicBooks) {
        filter |= MF_CBX_ONLY;
    }

    win::menu::Empty(menu);
    BuildMenuFromMenuDef(menuDefFile, dimof(menuDefFile), menu, filter);
    AppendRecentFilesToMenu(menu);
    AppendExternalViewersToMenu(menu, tab ? tab->filePath.Get() : nullptr);

    // Suppress menu items that depend on specific software being installed:
    // e-mail client, Adobe Reader, Foxit, PDF-XChange
    // Don't hide items here that won't always be hidden
    // (MenuUpdateStateForWindow() is for that)
    if (!CanSendAsEmailAttachment()) {
        win::menu::Remove(menu, IDM_SEND_BY_EMAIL);
    }

    // Also suppress PDF specific items for non-PDF documents
    if (!CouldBePDFDoc(tab) || !CanViewWithAcrobat()) {
        win::menu::Remove(menu, IDM_VIEW_WITH_ACROBAT);
    }
    if (!CouldBePDFDoc(tab) || !CanViewWithFoxit()) {
        win::menu::Remove(menu, IDM_VIEW_WITH_FOXIT);
    }
    if (!CouldBePDFDoc(tab) || !CanViewWithPDFXChange()) {
        win::menu::Remove(menu, IDM_VIEW_WITH_PDF_XCHANGE);
    }
    if (!CanViewWithXPSViewer(tab)) {
        win::menu::Remove(menu, IDM_VIEW_WITH_XPS_VIEWER);
    }
    if (!CanViewWithHtmlHelp(tab)) {
        win::menu::Remove(menu, IDM_VIEW_WITH_HTML_HELP);
    }

    bool supportsAnnotations = false;
    DisplayModel* dm = tab ? tab->AsFixed() : nullptr;
    EngineBase* engine = tab ? tab->GetEngine() : nullptr;
    if (engine) {
        supportsAnnotations = engine->supportsAnnotations;
    }
    bool canDoAnnotations = gIsDebugBuild || gIsPreReleaseBuild || gIsDailyBuild;
    if (!canDoAnnotations) {
        supportsAnnotations = false;
    }
    if (!supportsAnnotations) {
        win::menu::Remove(menu, IDM_SAVE_ANNOTATIONS_SMX);
    } else {
        win::menu::SetEnabled(menu, IDM_SAVE_ANNOTATIONS_SMX, dm->userAnnotsModified);
    }
}

// so that we can do free everything at exit
std::vector<MenuOwnerDrawInfo*> g_menuDrawInfos;

void FreeAllMenuDrawInfos() {
    while (g_menuDrawInfos.size() != 0) {
        // Note: could be faster
        FreeMenuOwnerDrawInfo(g_menuDrawInfos[0]);
    }
}

void FreeMenuOwnerDrawInfo(MenuOwnerDrawInfo* modi) {
    auto it = std::remove(begin(g_menuDrawInfos), end(g_menuDrawInfos), modi);
    CrashIf(it == end(g_menuDrawInfos));
    g_menuDrawInfos.erase(it, end(g_menuDrawInfos));
    str::Free(modi->text);
    free(modi);
}

static HFONT gMenuFont = nullptr;

HFONT GetMenuFont() {
    if (!gMenuFont) {
        NONCLIENTMETRICS ncm = {0};
        ncm.cbSize = sizeof(ncm);
        SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
        gMenuFont = CreateFontIndirect(&ncm.lfMenuFont);
    }
    return gMenuFont;
}

struct MenuText {
    WCHAR* menuText;
    int menuTextLen;
    WCHAR* shortcutText;
    int shortcutTextLen;
};

// menu text consists of potentially 2 parts:
// - text of the menu item
// - text for the keyboard shortcut
// They are separated with \t
static void ParseMenuText(WCHAR* s, MenuText& mt) {
    mt.shortcutText = nullptr;
    mt.menuText = s;
    while (*s && *s != L'\t') {
        s++;
    }
    mt.menuTextLen = (int)(s - mt.menuText);
    if (*s != L'\t') {
        return;
    }
    s++;
    mt.shortcutText = s;
    while (*s) {
        s++;
    }
    mt.shortcutTextLen = (int)(s - mt.shortcutText);
}

void FreeMenuOwnerDrawInfoData(HMENU hmenu) {
    MENUITEMINFOW mii = {0};
    mii.cbSize = sizeof(MENUITEMINFOW);

    int n = GetMenuItemCount(hmenu);
    for (int i = 0; i < n; i++) {
        mii.fMask = MIIM_DATA | MIIM_FTYPE | MIIM_SUBMENU;
        BOOL ok = GetMenuItemInfoW(hmenu, (UINT)i, TRUE /* by position */, &mii);
        CrashIf(!ok);
        auto modi = (MenuOwnerDrawInfo*)mii.dwItemData;
        if (modi != nullptr) {
            FreeMenuOwnerDrawInfo(modi);
            mii.dwItemData = 0;
            mii.fType &= ~MFT_OWNERDRAW;
            SetMenuItemInfoW(hmenu, (UINT)i, TRUE /* by position */, &mii);
        }
        if (mii.hSubMenu != nullptr) {
            MarkMenuOwnerDraw(mii.hSubMenu);
        }
    };
}
#if defined(EXP_MENU_OWNER_DRAW)
void MarkMenuOwnerDraw(HMENU hmenu) {
    WCHAR buf[1024];

    MENUITEMINFOW mii = {0};
    mii.cbSize = sizeof(MENUITEMINFOW);

    int n = GetMenuItemCount(hmenu);

    for (int i = 0; i < n; i++) {
        buf[0] = 0;
        mii.fMask = MIIM_BITMAP | MIIM_CHECKMARKS | MIIM_DATA | MIIM_FTYPE | MIIM_STATE | MIIM_SUBMENU | MIIM_STRING;
        mii.dwTypeData = &(buf[0]);
        mii.cch = dimof(buf);
        BOOL ok = GetMenuItemInfoW(hmenu, (UINT)i, TRUE /* by position */, &mii);
        CrashIf(!ok);

        mii.fMask = MIIM_FTYPE | MIIM_DATA;
        mii.fType |= MFT_OWNERDRAW;
        if (mii.dwItemData != 0) {
            auto modi = (MenuOwnerDrawInfo*)mii.dwItemData;
            FreeMenuOwnerDrawInfo(modi);
        }
        auto modi = AllocStruct<MenuOwnerDrawInfo>();
        g_menuDrawInfos.push_back(modi);
        modi->fState = mii.fState;
        modi->fType = mii.fType;
        modi->hbmpItem = mii.hbmpItem;
        modi->hbmpChecked = mii.hbmpChecked;
        modi->hbmpUnchecked = mii.hbmpUnchecked;
        if (str::Len(buf) > 0) {
            modi->text = str::Dup(buf);
        }
        mii.dwItemData = (ULONG_PTR)modi;
        SetMenuItemInfoW(hmenu, (UINT)i, TRUE /* by position */, &mii);

        if (mii.hSubMenu != nullptr) {
            MarkMenuOwnerDraw(mii.hSubMenu);
        }
    }
}
#else
void MarkMenuOwnerDraw(HMENU hmenu) {
    UNUSED(hmenu);
}
#endif

enum {
    kMenuPaddingY = 2,
    kMenuPaddingX = 2,
};

void MenuOwnerDrawnMesureItem(HWND hwnd, MEASUREITEMSTRUCT* mis) {
    if (ODT_MENU != mis->CtlType) {
        return;
    }
    auto modi = (MenuOwnerDrawInfo*)mis->itemData;

    bool isSeparator = bit::IsMaskSet(modi->fType, (UINT)MFT_SEPARATOR);
    if (isSeparator) {
        mis->itemHeight = DpiScale(hwnd, 7);
        mis->itemWidth = DpiScale(hwnd, 33);
        return;
    }

    auto text = modi && modi->text ? modi->text : L"Dummy";
    HFONT font = GetMenuFont();
    MenuText mt;
    ParseMenuText((WCHAR*)text, mt);

    auto size = TextSizeInHwnd(hwnd, mt.menuText, font);
    mis->itemHeight = size.dy;
    int dx = size.dx;
    if (mt.shortcutText != nullptr) {
        // add space betweeen menu text and shortcut
        size = TextSizeInHwnd(hwnd, L"    ", font);
        dx += size.dx;
        size = TextSizeInHwnd(hwnd, mt.shortcutText, font);
        dx += size.dx;
    }
    auto padX = DpiScale(hwnd, kMenuPaddingX);
    auto padY = DpiScale(hwnd, kMenuPaddingY);

    auto cxMenuCheck = GetSystemMetrics(SM_CXMENUCHECK);
    mis->itemHeight += padY * 2;
    mis->itemWidth = UINT(dx + DpiScale(hwnd, cxMenuCheck) + (padX * 2));
}

// https://gist.github.com/kjk/1df108aa126b7d8e298a5092550a53b7
void MenuOwnerDrawnDrawItem(HWND hwnd, DRAWITEMSTRUCT* dis) {
    UNUSED(hwnd);
    if (ODT_MENU != dis->CtlType) {
        return;
    }
    auto modi = (MenuOwnerDrawInfo*)dis->itemData;
    if (!modi) {
        return;
    }

    // https://msdn.microsoft.com/en-us/library/windows/desktop/ms647578(v=vs.85).aspx

    // low-order word of the dwTypeData member is the bitmap handle
    // HBITMAP bmp = (HBITMAP)LOWORD(modi->dwTypeData) ?
    // bool isBitmap = bit::IsMaskSet(modi->fType, (UINT)MFT_BITMAP);

    // ???
    // bool isMenuBarBreak = bit::IsMaskSet(modi->fType, (UINT)MFT_MENUBARBREAK);

    // ??
    // bool isMenuBreak = bit::IsMaskSet(modi->fType, (UINT)MFT_MENUBREAK);

    // bool isRadioCheck = bit::IsMaskSet(modi->fType, (UINT)MFT_RADIOCHECK);

    bool isSeparator = bit::IsMaskSet(modi->fType, (UINT)MFT_SEPARATOR);

    // default should be drawn in bold
    // bool isDefault = bit::IsMaskSet(modi->fState, (UINT)MFS_DEFAULT);

    // disabled should be drawn grayed out
    // bool isDisabled = bit::IsMaskSet(modi->fState, (UINT)MFS_DISABLED);

    // don't know what that means
    // bool isHilited = bit::IsMaskSet(modi->fState, (UINT)MFS_HILITE);

    // checked/unchecked state for check and radio menus?
    // uses hbmpChecked, otherwise use hbmpUnchecked ?
    // bool isChecked = bit::IsMaskSet(modi->fState, (UINT)MFS_CHECKED);

    auto hdc = dis->hDC;
    HFONT font = GetMenuFont();
    auto prevFont = SelectObject(hdc, font);

    COLORREF bgCol = GetAppColor(AppColor::MainWindowBg);
    COLORREF txtCol = GetAppColor(AppColor::MainWindowText);

    bool isSelected = bit::IsMaskSet(dis->itemState, (UINT)ODS_SELECTED);
    if (isSelected) {
        // TODO: probably better colors
        std::swap(bgCol, txtCol);
    }

    RECT rc = dis->rcItem;

    int padY = DpiScale(hwnd, kMenuPaddingY);
    int padX = DpiScale(hwnd, kMenuPaddingX);
    int dxCheckMark = DpiScale(hwnd, GetSystemMetrics(SM_CXMENUCHECK));

    auto hbr = CreateSolidBrush(bgCol);
    FillRect(hdc, &rc, hbr);
    DeleteObject(hbr);

    if (isSeparator) {
        CrashIf(modi->text);
        int sx = rc.left + dxCheckMark;
        int y = rc.top + (RectDy(rc) / 2);
        int ex = rc.right - padX;
        auto pen = CreatePen(PS_SOLID, 1, txtCol);
        auto prevPen = SelectObject(hdc, pen);
        MoveToEx(hdc, sx, y, nullptr);
        LineTo(hdc, ex, y);
        SelectObject(hdc, prevPen);
        DeleteObject(pen);
        return;
    }

    // TODO: probably could be a bitmap etc.
    if (!modi->text) {
        return;
    }

    MenuText mt;
    ParseMenuText((WCHAR*)modi->text, mt);

    // TODO: improve how we paint the menu:
    // - paint checkmark if this is checkbox menu
    // - position text the right way (not just DT_CENTER)
    //   taking into account LTR mode
    // - paint shortcut (part after \t if exists) separately
    // - paint disabled state better
    // - paint icons for system menus
    SetTextColor(hdc, txtCol);
    SetBkColor(hdc, bgCol);

    // DrawTextEx handles & => underscore drawing
    rc.top += padY;
    rc.left += dxCheckMark;
    DrawTextExW(hdc, mt.menuText, mt.menuTextLen, &rc, DT_LEFT, nullptr);
    if (mt.shortcutText != nullptr) {
        rc = dis->rcItem;
        rc.top += padY;
        rc.right -= (padX + dxCheckMark / 2);
        DrawTextExW(hdc, mt.shortcutText, mt.shortcutTextLen, &rc, DT_RIGHT, nullptr);
    }
    SelectObject(hdc, prevFont);
}

//[ ACCESSKEY_GROUP Main Menubar
HMENU BuildMenu(WindowInfo* win) {
    HMENU mainMenu = CreateMenu();

    int filter = 0;
    if (win->AsChm()) {
        filter |= MF_NOT_FOR_CHM;
    } else if (win->AsEbook()) {
        filter |= MF_NOT_FOR_EBOOK_UI;
    }
    if (!win->currentTab || win->currentTab->GetEngineType() != kindEngineComicBooks) {
        filter |= MF_CBX_ONLY;
    }

    HMENU m = CreateMenu();
    RebuildFileMenu(win->currentTab, m);
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
#if defined(ENABLE_THEME)
    // Build the themes sub-menu of the settings menu
    MenuDef menuDefTheme[THEME_COUNT];
    static_assert(IDM_CHANGE_THEME_LAST - IDM_CHANGE_THEME_FIRST + 1 >= THEME_COUNT,
                  "Too many themes. Either remove some or update IDM_CHANGE_THEME_LAST");
    for (UINT i = 0; i < THEME_COUNT; i++) {
        menuDefTheme[i] = {GetThemeByIndex(i)->name, IDM_CHANGE_THEME_FIRST + i, 0};
    }
    HMENU m2 = BuildMenuFromMenuDef(menuDefTheme, dimof(menuDefTheme), CreateMenu(), filter);
    AppendMenu(m, MF_POPUP | MF_STRING, (UINT_PTR)m2, _TR("&Theme"));
#endif
    AppendMenu(mainMenu, MF_POPUP | MF_STRING, (UINT_PTR)m, _TR("&Settings"));

    m = BuildMenuFromMenuDef(menuDefHelp, dimof(menuDefHelp), CreateMenu(), filter);
    AppendMenu(mainMenu, MF_POPUP | MF_STRING, (UINT_PTR)m, _TR("&Help"));
#if 0
    // see MenuBarAsPopupMenu in Caption.cpp
    m = GetSystemMenu(win->hwndFrame, FALSE);
    AppendMenu(mainMenu, MF_POPUP | MF_STRING, (UINT_PTR)m, _TR("&Window"));
#endif
    m = BuildMenuFromMenuDef(menuDefDebug, dimof(menuDefDebug), CreateMenu(), filter);

    if (gAddCrashMeMenu) {
        AppendMenu(m, MF_SEPARATOR, 0, nullptr);
        AppendMenuA(m, MF_STRING, (UINT_PTR)IDM_DEBUG_CRASH_ME, "Crash me");
    }

    AppendMenu(mainMenu, MF_POPUP | MF_STRING, (UINT_PTR)m, L"Debug");

    MarkMenuOwnerDraw(mainMenu);
    return mainMenu;
}
//] ACCESSKEY_GROUP Main Menubar

void UpdateMenu(WindowInfo* win, HMENU m) {
    CrashIf(!win);
    UINT id = GetMenuItemID(m, 0);
    if (id == menuDefFile[0].id) {
        RebuildFileMenu(win->currentTab, m);
    } else if (id == menuDefFavorites[0].id) {
        win::menu::Empty(m);
        BuildMenuFromMenuDef(menuDefFavorites, dimof(menuDefFavorites), m);
        RebuildFavMenu(win, m);
    }
    MenuUpdateStateForWindow(win);
    MarkMenuOwnerDraw(win->menu);
}

// show/hide top-level menu bar. This doesn't persist across launches
// so that accidental removal of the menu isn't catastrophic
void ShowHideMenuBar(WindowInfo* win, bool showTemporarily) {
    CrashIf(!win->menu);
    if (win->presentation || win->isFullScreen) {
        return;
    }

    HWND hwnd = win->hwndFrame;

    if (showTemporarily) {
        SetMenu(hwnd, win->menu);
        return;
    }

    bool hideMenu = !showTemporarily && GetMenu(hwnd) != nullptr;
    SetMenu(hwnd, hideMenu ? nullptr : win->menu);
    win->isMenuHidden = hideMenu;
}
