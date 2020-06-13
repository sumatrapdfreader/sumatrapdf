/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/CmdLineParser.h"
#include "utils/FileUtil.h"
#include "utils/HtmlParserLookup.h"
#include "utils/BitManip.h"
#include "utils/Dpi.h"
#include "mui/Mui.h"
#include "utils/WinUtil.h"

#include "wingui/TreeModel.h"

#include "Annotation.h"
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
#include "ResourceIds.h"
#include "ExternalViewers.h"
#include "Favorites.h"
#include "FileThumbnails.h"
#include "Menu.h"
#include "Selection.h"
#include "SumatraAbout.h"
#include "SumatraDialogs.h"
#include "Translations.h"
#include "TocEditor.h"
#include "EditAnnotations.h"

// note: IDM_VIEW_SINGLE_PAGE - IDM_VIEW_CONTINUOUS and also
//       Cmd::ZoomFIT_PAGE - Cmd::ZoomCUSTOM must be in a continuous range!
static_assert((int)Cmd::ViewLayoutLast - (int)Cmd::ViewLayoutFirst == 4,
              "view layout ids are not in a continuous range");
static_assert((int)Cmd::ZoomLast - (int)Cmd::ZoomFirst == 17, "zoom ids are not in a continuous range");

bool gAddCrashMeMenu = false;

void MenuUpdateDisplayMode(WindowInfo* win) {
    bool enabled = win->IsDocLoaded();
    DisplayMode displayMode = gGlobalPrefs->defaultDisplayModeEnum;
    if (enabled) {
        displayMode = win->ctrl->GetDisplayMode();
    }

    for (int id = (int)Cmd::ViewLayoutFirst; id <= (int)Cmd::ViewLayoutLast; id++) {
        win::menu::SetEnabled(win->menu, id, enabled);
    }

    UINT id = 0;
    if (IsSingle(displayMode)) {
        id = (UINT)Cmd::ViewSinglePage;
    } else if (IsFacing(displayMode)) {
        id = (UINT)Cmd::ViewFacing;
    } else if (IsBookView(displayMode)) {
        id = (UINT)Cmd::ViewBook;
    } else {
        CrashIf(win->ctrl || DM_AUTOMATIC != displayMode);
    }

    CheckMenuRadioItem(win->menu, (UINT)Cmd::ViewLayoutFirst, (UINT)Cmd::ViewLayoutLast, id, MF_BYCOMMAND);
    win::menu::SetChecked(win->menu, (UINT)Cmd::ViewContinuous, IsContinuous(displayMode));

    if (win->currentTab && win->currentTab->GetEngineType() == kindEngineComicBooks) {
        bool mangaMode = win->AsFixed()->GetDisplayR2L();
        win::menu::SetChecked(win->menu, (UINT)Cmd::ViewMangaMode, mangaMode);
    }
}

// clang-format off
//[ ACCESSKEY_GROUP File Menu
static MenuDef menuDefFile[] = {
    { _TRN("New &window\tCtrl+N"),          (UINT)Cmd::NewWindow,             MF_REQ_DISK_ACCESS },
    { _TRN("&Open...\tCtrl+O"),             (UINT)Cmd::Open,                   MF_REQ_DISK_ACCESS },
    { "Open Folder",                        (UINT)Cmd::OpenFolder,            MF_REQ_DISK_ACCESS | MF_RAMICRO_ONLY },
    { _TRN("&Close\tCtrl+W"),               (UINT)Cmd::Close,                  MF_REQ_DISK_ACCESS },
    { _TRN("Show in &folder"),              (UINT)Cmd::ShowInFolder,         MF_REQ_DISK_ACCESS },
    { _TRN("&Save As...\tCtrl+S"),          (UINT)Cmd::SaveAs,                 MF_REQ_DISK_ACCESS },
    { _TRN("Save Annotations"),             (UINT)Cmd::SaveAnnotationsSmx,   MF_REQ_DISK_ACCESS },
 //[ ACCESSKEY_ALTERNATIVE // only one of these two will be shown
#ifdef ENABLE_SAVE_SHORTCUT
    { _TRN("Save S&hortcut...\tCtrl+Shift+S"), (UINT)IDM_SAVEAS_BOOKMARK,     MF_REQ_DISK_ACCESS | MF_NOT_FOR_CHM | MF_NOT_FOR_EBOOK_UI },
//| ACCESSKEY_ALTERNATIVE
#else
    { _TRN("Re&name...\tF2"),               (UINT)Cmd::RenameFile,            MF_REQ_DISK_ACCESS },
#endif
//] ACCESSKEY_ALTERNATIVE
    { _TRN("&Print...\tCtrl+P"),            (UINT)Cmd::Print,                  MF_REQ_PRINTER_ACCESS | MF_NOT_FOR_EBOOK_UI },
    { SEP_ITEM,                             0,                          MF_REQ_DISK_ACCESS },
//[ ACCESSKEY_ALTERNATIVE // PDF/XPS/CHM specific items are dynamically removed in RebuildFileMenu
    { _TRN("Open in &Adobe Reader"),        (UINT)Cmd::ViewWithAcrobat,      MF_REQ_DISK_ACCESS | MF_NOT_FOR_EBOOK_UI },
    { _TRN("Open in &Foxit Reader"),        (UINT)Cmd::ViewWithFoxIt,        MF_REQ_DISK_ACCESS | MF_NOT_FOR_EBOOK_UI },
    { _TRN("Open &in PDF-XChange"),         (UINT)Cmd::ViewWithPdfXchange,  MF_REQ_DISK_ACCESS | MF_NOT_FOR_EBOOK_UI },
//| ACCESSKEY_ALTERNATIVE
    { _TRN("Open in &Microsoft XPS-Viewer"),(UINT)Cmd::ViewWithXpsViewer,   MF_REQ_DISK_ACCESS | MF_NOT_FOR_EBOOK_UI },
//| ACCESSKEY_ALTERNATIVE
    { _TRN("Open in &Microsoft HTML Help"), (UINT)Cmd::ViewWithHtmlHelp,    MF_REQ_DISK_ACCESS | MF_NOT_FOR_EBOOK_UI },
//] ACCESSKEY_ALTERNATIVE
    // further entries are added if specified in gGlobalPrefs.vecCommandLine
    { _TRN("Send by &E-mail..."),           (UINT)Cmd::SendByEmail,          MF_REQ_DISK_ACCESS },
    { SEP_ITEM,                             0,                          MF_REQ_DISK_ACCESS },
    { _TRN("P&roperties\tCtrl+D"),          (UINT)Cmd::Properties,             0 },
    { SEP_ITEM,                             0,                          0 },
    { _TRN("E&xit\tCtrl+Q"),                (UINT)Cmd::Exit,                   0 },
    { 0, 0, 0 },
};
//] ACCESSKEY_GROUP File Menu

//[ ACCESSKEY_GROUP View Menu
static MenuDef menuDefView[] = {
    { _TRN("&Single Page\tCtrl+6"),         (UINT)Cmd::ViewSinglePage,        MF_NOT_FOR_CHM },
    { _TRN("&Facing\tCtrl+7"),              (UINT)Cmd::ViewFacing,            MF_NOT_FOR_CHM },
    { _TRN("&Book View\tCtrl+8"),           (UINT)Cmd::ViewBook,              MF_NOT_FOR_CHM | MF_NOT_FOR_EBOOK_UI },
    { _TRN("Show &Pages Continuously"),     (UINT)Cmd::ViewContinuous,        MF_NOT_FOR_CHM | MF_NOT_FOR_EBOOK_UI },
    // TODO: "&Inverse Reading Direction" (since some Mangas might be read left-to-right)?
    { _TRN("Man&ga Mode"),                  (UINT)Cmd::ViewMangaMode,         MF_CBX_ONLY },
    { SEP_ITEM,                             0,                                MF_NOT_FOR_CHM },
    { _TRN("Rotate &Left\tCtrl+Shift+-"),   (UINT)Cmd::ViewRotateLeft,        MF_NOT_FOR_CHM | MF_NOT_FOR_EBOOK_UI },
    { _TRN("Rotate &Right\tCtrl+Shift++"),  (UINT)Cmd::ViewRotateRight,       MF_NOT_FOR_CHM | MF_NOT_FOR_EBOOK_UI },
    { SEP_ITEM,                             0,                                MF_NOT_FOR_CHM | MF_NOT_FOR_EBOOK_UI },
    { _TRN("Pr&esentation\tF5"),            (UINT)Cmd::ViewPresentationMode,  MF_REQ_FULLSCREEN | MF_NOT_FOR_CHM | MF_NOT_FOR_EBOOK_UI },
    { _TRN("F&ullscreen\tF11"),             (UINT)Cmd::ViewFullScreen,        MF_REQ_FULLSCREEN },
    { SEP_ITEM,                             0,                                MF_REQ_FULLSCREEN },
    { _TRN("Show Book&marks\tF12"),         (UINT)Cmd::ViewBookmarks,         0 },
    { _TRN("Show &Toolbar\tF8"),            (UINT)Cmd::ViewShowHideToolbar,   MF_NOT_FOR_EBOOK_UI },
    { SEP_ITEM,                             0,                                MF_REQ_ALLOW_COPY | MF_NOT_FOR_EBOOK_UI },
    { _TRN("Select &All\tCtrl+A"),          (UINT)Cmd::SelectAll,             MF_REQ_ALLOW_COPY | MF_NOT_FOR_EBOOK_UI },
    { _TRN("&Copy Selection\tCtrl+C"),      (UINT)Cmd::CopySelection,         MF_REQ_ALLOW_COPY | MF_NOT_FOR_EBOOK_UI },
    { 0, 0, 0 },
};
//] ACCESSKEY_GROUP View Menu

//[ ACCESSKEY_GROUP GoTo Menu
static MenuDef menuDefGoTo[] = {
    { _TRN("&Next Page\tRight Arrow"),      (UINT)Cmd::GoToNextPage,         0 },
    { _TRN("&Previous Page\tLeft Arrow"),   (UINT)Cmd::GoToPrevPage,         0 },
    { _TRN("&First Page\tHome"),            (UINT)Cmd::GoToFirstPage,        0 },
    { _TRN("&Last Page\tEnd"),              (UINT)Cmd::GoToLastPage,         0 },
    { _TRN("Pa&ge...\tCtrl+G"),             (UINT)Cmd::GoToPage,              0 },
    { SEP_ITEM,                             0,                          0 },
    { _TRN("&Back\tAlt+Left Arrow"),        (UINT)Cmd::GoToNavBack,          0 },
    { _TRN("F&orward\tAlt+Right Arrow"),    (UINT)Cmd::GoToNavForward,       0 },
    { SEP_ITEM,                             0,                          MF_NOT_FOR_EBOOK_UI },
    { _TRN("Fin&d...\tCtrl+F"),             (UINT)Cmd::FindFirst,             MF_NOT_FOR_EBOOK_UI },
    { 0, 0, 0 },
};
//] ACCESSKEY_GROUP GoTo Menu

//[ ACCESSKEY_GROUP Zoom Menu
// the entire menu is MF_NOT_FOR_EBOOK_UI
static MenuDef menuDefZoom[] = {
    { _TRN("Fit &Page\tCtrl+0"),            (UINT)Cmd::ZoomFitPage,          MF_NOT_FOR_CHM },
    { _TRN("&Actual Size\tCtrl+1"),         (UINT)Cmd::ZoomActualSize,       MF_NOT_FOR_CHM },
    { _TRN("Fit &Width\tCtrl+2"),           (UINT)Cmd::ZoomFitWidth,         MF_NOT_FOR_CHM },
    { _TRN("Fit &Content\tCtrl+3"),         (UINT)Cmd::ZoomFitContent,       MF_NOT_FOR_CHM },
    { _TRN("Custom &Zoom...\tCtrl+Y"),      (UINT)Cmd::ZoomCustom,            0 },
    { SEP_ITEM,                             0,                          0 },
    { "6400%",                              (UINT)Cmd::Zoom6400,              MF_NO_TRANSLATE | MF_NOT_FOR_CHM },
    { "3200%",                              (UINT)Cmd::Zoom3200,              MF_NO_TRANSLATE | MF_NOT_FOR_CHM },
    { "1600%",                              (UINT)Cmd::Zoom1600,              MF_NO_TRANSLATE | MF_NOT_FOR_CHM },
    { "800%",                               (UINT)Cmd::Zoom800,               MF_NO_TRANSLATE | MF_NOT_FOR_CHM },
    { "400%",                               (UINT)Cmd::Zoom400,               MF_NO_TRANSLATE },
    { "200%",                               (UINT)Cmd::Zoom200,               MF_NO_TRANSLATE },
    { "150%",                               (UINT)Cmd::Zoom150,               MF_NO_TRANSLATE },
    { "125%",                               (UINT)Cmd::Zoom125,               MF_NO_TRANSLATE },
    { "100%",                               (UINT)Cmd::Zoom100,               MF_NO_TRANSLATE },
    { "50%",                                (UINT)Cmd::Zoom50,                MF_NO_TRANSLATE },
    { "25%",                                (UINT)Cmd::Zoom25,                MF_NO_TRANSLATE },
    { "12.5%",                              (UINT)Cmd::Zoom12_5,              MF_NO_TRANSLATE | MF_NOT_FOR_CHM },
    { "8.33%",                              (UINT)Cmd::Zoom8_33,              MF_NO_TRANSLATE | MF_NOT_FOR_CHM },
    { 0, 0, 0 },
};
//] ACCESSKEY_GROUP Zoom Menu

//[ ACCESSKEY_GROUP Settings Menu
static MenuDef menuDefSettings[] = {
    { _TRN("Change Language"),              (UINT)Cmd::ChangeLanguage,        0 },
#if 0
    { _TRN("Contribute Translation"),       (UINT)IDM_CONTRIBUTE_TRANSLATION, MF_REQ_DISK_ACCESS },
    { SEP_ITEM,                             0,                          MF_REQ_DISK_ACCESS },
#endif
    { _TRN("&Options..."),                  (UINT)Cmd::Options,                MF_REQ_PREF_ACCESS },
    { _TRN("&Advanced Options..."),         (UINT)Cmd::AdvancedOptions,       MF_REQ_PREF_ACCESS | MF_REQ_DISK_ACCESS },
    { 0, 0, 0 },
};
//] ACCESSKEY_GROUP Settings Menu

//[ ACCESSKEY_GROUP Favorites Menu
// the entire menu is MF_NOT_FOR_EBOOK_UI
MenuDef menuDefFavorites[] = {
    { _TRN("Add to favorites"),             (UINT)Cmd::FavAdd,                0 },
    { _TRN("Remove from favorites"),        (UINT)Cmd::FavDel,                0 },
    { _TRN("Show Favorites"),               (UINT)Cmd::FavToggle,             MF_REQ_DISK_ACCESS },
    { 0, 0, 0 },
};
//] ACCESSKEY_GROUP Favorites Menu

//[ ACCESSKEY_GROUP Help Menu
static MenuDef menuDefHelp[] = {
    { _TRN("Visit &Website"),               (UINT)Cmd::VisitWebsite,          MF_REQ_DISK_ACCESS },
    { _TRN("&Manual"),                      (UINT)Cmd::Manual,                 MF_REQ_DISK_ACCESS },
    { _TRN("Check for &Updates"),           (UINT)Cmd::CheckUpdate,           MF_REQ_INET_ACCESS },
    { SEP_ITEM,                             0,                          MF_REQ_DISK_ACCESS },
    { _TRN("&About"),                       (UINT)Cmd::About,                  0 },
    { 0, 0, 0 },
};
//] ACCESSKEY_GROUP Help Menu

//[ ACCESSKEY_GROUP Debug Menu
static MenuDef menuDefDebug[] = {
    { _TRN("&Advanced Options..."),         (UINT)Cmd::AdvancedOptions,       MF_REQ_PREF_ACCESS | MF_REQ_DISK_ACCESS },
    { "Highlight links",                    (UINT)Cmd::DebugShowLinks,       MF_NO_TRANSLATE },
    { "Toggle ebook UI",                    (UINT)Cmd::DebugEbookUI,         MF_NO_TRANSLATE },
    { "Mui debug paint",                    (UINT)Cmd::DebugMui,              MF_NO_TRANSLATE },
    { "Annotation from Selection",          (UINT)Cmd::DebugAnnotations,       MF_NO_TRANSLATE },
    { "Download symbols",                   (UINT)Cmd::DebugDownloadSymbols, MF_NO_TRANSLATE },
    { "Test app",                           (UINT)Cmd::DebugTestApp,         MF_NO_TRANSLATE },
    { "Show notification",                  (UINT)Cmd::DebugShowNotif,       MF_NO_TRANSLATE },
    { 0, 0, 0 },
};
//] ACCESSKEY_GROUP Debug Menu

//[ ACCESSKEY_GROUP Context Menu (Content)
// the entire menu is MF_NOT_FOR_CHM | MF_NOT_FOR_EBOOK_UI
static MenuDef menuDefContext[] = {
    { _TRN("&Copy Selection"),              (UINT)Cmd::CopySelection,         MF_REQ_ALLOW_COPY },
    { _TRN("Copy &Link Address"),           (UINT)Cmd::CopyLinkTarget,        MF_REQ_ALLOW_COPY },
    { _TRN("Copy Co&mment"),                (UINT)Cmd::CopyComment,           MF_REQ_ALLOW_COPY },
    { _TRN("Copy &Image"),                  (UINT)Cmd::CopyImage,             MF_REQ_ALLOW_COPY },
    { _TRN("Select &All"),                  (UINT)Cmd::SelectAll,             MF_REQ_ALLOW_COPY },
    { SEP_ITEM,                             0,                                MF_REQ_ALLOW_COPY },
    // note: strings cannot be "" or else items are not there
    {"add",                                 (UINT)Cmd::FavAdd,                MF_NO_TRANSLATE   },
    {"del",                                 (UINT)Cmd::FavDel,                MF_NO_TRANSLATE   },
    { _TRN("Show &Favorites"),              (UINT)Cmd::FavToggle,             0                 },
    { _TRN("Show &Bookmarks\tF12"),         (UINT)Cmd::ViewBookmarks,         0                 },
    { _TRN("Show &Toolbar\tF8"),            (UINT)Cmd::ViewShowHideToolbar,   MF_NOT_FOR_EBOOK_UI },
    { _TRN("Save Annotations"),             (UINT)Cmd::SaveAnnotationsSmx,    MF_REQ_DISK_ACCESS },
    { _TR_TODON("Edit Annotations"),        (UINT)Cmd::EditAnnotations,       MF_REQ_DISK_ACCESS },
    {"New Bookmarks",                       (UINT)Cmd::NewBookmarks,          MF_NO_TRANSLATE},
    { SEP_ITEM,                             0,                                MF_PLUGIN_MODE_ONLY | MF_REQ_ALLOW_COPY },
    { _TRN("&Save As..."),                  (UINT)Cmd::SaveAs,                MF_PLUGIN_MODE_ONLY | MF_REQ_DISK_ACCESS },
    { _TRN("&Print..."),                    (UINT)Cmd::Print,                 MF_PLUGIN_MODE_ONLY | MF_REQ_PRINTER_ACCESS },
    { _TRN("P&roperties"),                  (UINT)Cmd::Properties,            MF_PLUGIN_MODE_ONLY },
    { _TRN("E&xit Fullscreen"),             (UINT)Cmd::ExitFullScreen,        0 },
    { 0, 0, 0 },
};
//] ACCESSKEY_GROUP Context Menu (Content)

//[ ACCESSKEY_GROUP Context Menu (Start)
static MenuDef menuDefContextStart[] = {
    { _TRN("&Open Document"),               (UINT)Cmd::OpenSelectedDocument, MF_REQ_DISK_ACCESS },
    { _TRN("&Pin Document"),                (UINT)Cmd::PinSelectedDocument,  MF_REQ_DISK_ACCESS | MF_REQ_PREF_ACCESS },
    { SEP_ITEM,                             0,                          MF_REQ_DISK_ACCESS | MF_REQ_PREF_ACCESS },
    { _TRN("&Remove Document"),             (UINT)Cmd::ForgetSelectedDocument, MF_REQ_DISK_ACCESS | MF_REQ_PREF_ACCESS },
    { 0, 0, 0 },
};
//] ACCESSKEY_GROUP Context Menu (Start)
// clang-format on

HMENU BuildMenuFromMenuDef(MenuDef menuDefs[], HMENU menu, int flagFilter) {
    CrashIf(!menu);
    bool wasSeparator = true;

    int i = 0;
    while (true) {
        MenuDef md = menuDefs[i];
        if (md.title == nullptr) {
            // sentinel
            break;
        }
        i++;
        if ((md.flags & MF_PLUGIN_MODE_ONLY) != 0) {
            if (!gPluginMode) {
                continue;
            }
        }
        if ((md.flags & MF_RAMICRO_ONLY) != 0) {
            if (!gIsRaMicroBuild) {
                continue;
            }
        }
        if (md.flags & flagFilter) {
            continue;
        }

        if (!HasPermission(md.flags >> PERM_FLAG_OFFSET)) {
            continue;
        }

        if (str::Eq(md.title, SEP_ITEM)) {
            // prevent two consecutive separators
            if (!wasSeparator) {
                AppendMenuW(menu, MF_SEPARATOR, (UINT_PTR)md.id, nullptr);
            }
            wasSeparator = true;
        } else if (MF_NO_TRANSLATE == (md.flags & MF_NO_TRANSLATE)) {
            AutoFreeWstr tmp = strconv::Utf8ToWstr(md.title);
            AppendMenuW(menu, MF_STRING, (UINT_PTR)md.id, tmp);
            wasSeparator = false;
        } else {
            const WCHAR* tmp = trans::GetTranslation(md.title);
            AppendMenuW(menu, MF_STRING, (UINT_PTR)md.id, tmp);
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
    UINT menuId = (UINT)Cmd::FileHistoryFirst + index;
    UINT flags = MF_BYCOMMAND | MF_ENABLED | MF_STRING;
    InsertMenuW(menuFile, (UINT)Cmd::Exit, flags, menuId, menuString);
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
        InsertMenu(m, (UINT)Cmd::Exit, MF_BYCOMMAND | MF_SEPARATOR, 0, nullptr);
    }
}

static void AppendExternalViewersToMenu(HMENU menuFile, const WCHAR* filePath) {
    if (0 == gGlobalPrefs->externalViewers->size()) {
        return;
    }
    if (!HasPermission(Perm_DiskAccess) || (filePath && !file::Exists(filePath))) {
        return;
    }

    int maxEntries = (int)Cmd::OpenWithExternalLast - (int)Cmd::OpenWithExternalFirst + 1;
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
        UINT menuId = (UINT)Cmd::OpenWithExternalFirst + count;
        InsertMenuW(menuFile, (UINT)Cmd::SendByEmail, MF_BYCOMMAND | MF_ENABLED | MF_STRING, menuId, menuString);
        if (!filePath) {
            win::menu::SetEnabled(menuFile, menuId, false);
        }
        count++;
    }
}

// clang-format off
static struct {
    uint itemId;
    float zoom;
} gZoomMenuIds[] = {
    { (uint)Cmd::Zoom6400,        6400.0 },
    { (uint)Cmd::Zoom3200,        3200.0 },
    { (uint)Cmd::Zoom1600,        1600.0 },
    { (uint)Cmd::Zoom800,         800.0  },
    { (uint)Cmd::Zoom400,         400.0  },
    { (uint)Cmd::Zoom200,         200.0  },
    { (uint)Cmd::Zoom150,         150.0  },
    { (uint)Cmd::Zoom125,         125.0  },
    { (uint)Cmd::Zoom100,         100.0  },
    { (uint)Cmd::Zoom50,          50.0   },
    { (uint)Cmd::Zoom25,          25.0   },
    { (uint)Cmd::Zoom12_5,        12.5   },
    { (uint)Cmd::Zoom8_33,        8.33f  },
    { (uint)Cmd::ZoomCustom,      0      },
    { (uint)Cmd::ZoomFitPage,    ZOOM_FIT_PAGE    },
    { (uint)Cmd::ZoomFitWidth,   ZOOM_FIT_WIDTH   },
    { (uint)Cmd::ZoomFitContent, ZOOM_FIT_CONTENT },
    { (uint)Cmd::ZoomActualSize, ZOOM_ACTUAL_SIZE },
};
// clang-format on

UINT MenuIdFromVirtualZoom(float virtualZoom) {
    int n = (int)dimof(gZoomMenuIds);
    for (int i = 0; i < n; i++) {
        if (virtualZoom == gZoomMenuIds[i].zoom) {
            return (UINT)gZoomMenuIds[i].itemId;
        }
    }
    return (UINT)Cmd::ZoomCustom;
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
    CrashIf(((UINT)Cmd::ZoomFirst > menuItemId) || (menuItemId > (UINT)Cmd::ZoomLast));

    for (int i = 0; i < dimof(gZoomMenuIds); i++) {
        win::menu::SetEnabled(m, gZoomMenuIds[i].itemId, canZoom);
    }

    if ((UINT)Cmd::Zoom100 == menuItemId) {
        menuItemId = (UINT)Cmd::ZoomActualSize;
    }
    CheckMenuRadioItem(m, (UINT)Cmd::ZoomFirst, (UINT)Cmd::ZoomLast, menuItemId, MF_BYCOMMAND);
    if ((UINT)Cmd::ZoomActualSize == menuItemId) {
        CheckMenuRadioItem(m, (UINT)Cmd::Zoom100, (UINT)Cmd::Zoom100, (UINT)Cmd::Zoom100, MF_BYCOMMAND);
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
    for (idx = 0; idx < dimof(menuDefFile) && menuDefFile[idx].id != (UINT)Cmd::Print; idx++) {
        // do nothing
    }
    if (idx < dimof(menuDefFile)) {
        const WCHAR* printItem = trans::GetTranslation(menuDefFile[idx].title);
        if (!filePrintAllowed) {
            printItem = _TR("&Print... (denied)");
        }
        if (!filePrintAllowed || !disableOnly) {
            ModifyMenuW(menu, (UINT)Cmd::Print, MF_BYCOMMAND | MF_STRING, (UINT_PTR)Cmd::Print, printItem);
        }
    }

    win::menu::SetEnabled(menu, (UINT)Cmd::Print, filePrintEnabled && filePrintAllowed);
}

static bool IsFileCloseMenuEnabled() {
    for (size_t i = 0; i < gWindows.size(); i++) {
        if (!gWindows.at(i)->IsAboutWindow()) {
            return true;
        }
    }
    return false;
}

static void MenuUpdateStateForWindow(WindowInfo* win) {
    // those menu items will be disabled if no document is opened, enabled otherwise
    static UINT menusToDisableIfNoDocument[] = {
        (UINT)Cmd::ViewRotateLeft,
        (UINT)Cmd::ViewRotateRight,
        (UINT)Cmd::GoToNextPage,
        (UINT)Cmd::GoToPrevPage,
        (UINT)Cmd::GoToFirstPage,
        (UINT)Cmd::GoToLastPage,
        (UINT)Cmd::GoToNavBack,
        (UINT)Cmd::GoToNavForward,
        (UINT)Cmd::GoToPage,
        (UINT)Cmd::FindFirst,
        (UINT)Cmd::SaveAs,
        (UINT)Cmd::SaveAsBookmark,
        (UINT)Cmd::SendByEmail,
        (UINT)Cmd::SelectAll,
        (UINT)Cmd::CopySelection,
        (UINT)Cmd::Properties,
        (UINT)Cmd::ViewPresentationMode,
        (UINT)Cmd::ViewWithAcrobat,
        (UINT)Cmd::ViewWithFoxIt,
        (UINT)Cmd::ViewWithPdfXchange,
        (UINT)Cmd::RenameFile,
        (UINT)Cmd::ShowInFolder,
        (UINT)Cmd::DebugAnnotations,
        // IDM_VIEW_WITH_XPS_VIEWER and IDM_VIEW_WITH_HTML_HELP
        // are removed instead of disabled (and can remain enabled
        // for broken XPS/CHM documents)
    };
    // this list coincides with menusToEnableIfBrokenPDF
    static UINT menusToDisableIfDirectory[] = {
        (UINT)Cmd::RenameFile, (UINT)Cmd::SendByEmail,        (UINT)Cmd::ViewWithAcrobat,
        (UINT)Cmd::ViewWithFoxIt, (UINT)Cmd::ViewWithPdfXchange, (UINT)Cmd::ShowInFolder,
    };
#define menusToEnableIfBrokenPDF menusToDisableIfDirectory

    TabInfo* tab = win->currentTab;

    for (int i = 0; i < dimof(menusToDisableIfNoDocument); i++) {
        UINT id = menusToDisableIfNoDocument[i];
        win::menu::SetEnabled(win->menu, id, win->IsDocLoaded());
    }

    // TODO: happens with UseTabs = false with .pdf files
    SubmitCrashIf(IsFileCloseMenuEnabled() == win->IsAboutWindow());
    win::menu::SetEnabled(win->menu, (UINT)Cmd::Close, IsFileCloseMenuEnabled());

    MenuUpdatePrintItem(win, win->menu);

    bool enabled = win->IsDocLoaded() && tab && tab->ctrl->HacToc();
    win::menu::SetEnabled(win->menu, (UINT)Cmd::ViewBookmarks, enabled);

    bool documentSpecific = win->IsDocLoaded();
    bool checked = documentSpecific ? win->tocVisible : gGlobalPrefs->showToc;
    win::menu::SetChecked(win->menu, (UINT)Cmd::ViewBookmarks, checked);

    win::menu::SetChecked(win->menu, (UINT)Cmd::FavToggle, gGlobalPrefs->showFavorites);
    win::menu::SetChecked(win->menu, (UINT)Cmd::ViewShowHideToolbar, gGlobalPrefs->showToolbar);
    MenuUpdateDisplayMode(win);
    MenuUpdateZoom(win);

    if (win->IsDocLoaded() && tab) {
        win::menu::SetEnabled(win->menu, (UINT)Cmd::GoToNavBack, tab->ctrl->CanNavigate(-1));
        win::menu::SetEnabled(win->menu, (UINT)Cmd::GoToNavForward, tab->ctrl->CanNavigate(1));
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
        win::menu::SetEnabled(win->menu, (UINT)Cmd::FindFirst, !engine->IsImageCollection());
    }

    if (win->IsDocLoaded() && !fileExists) {
        win::menu::SetEnabled(win->menu, (UINT)Cmd::RenameFile, false);
    }

#if defined(ENABLE_THEME)
    CheckMenuRadioItem(win->menu, IDM_CHANGE_THEME_FIRST, IDM_CHANGE_THEME_LAST,
                       IDM_CHANGE_THEME_FIRST + GetCurrentThemeIndex(), MF_BYCOMMAND);
#endif

    win::menu::SetChecked(win->menu, (UINT)Cmd::DebugShowLinks, gDebugShowLinks);
    win::menu::SetChecked(win->menu, (UINT)Cmd::DebugEbookUI, gGlobalPrefs->ebookUI.useFixedPageUI);
    win::menu::SetChecked(win->menu, (UINT)Cmd::DebugMui, mui::IsDebugPaint());
    win::menu::SetEnabled(win->menu, (UINT)Cmd::DebugAnnotations,
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

    HMENU popup = BuildMenuFromMenuDef(menuDefContextStart, CreatePopupMenu());
    win::menu::SetChecked(popup, (UINT)Cmd::PinSelectedDocument, state->isPinned);
    POINT pt = {x, y};
    MapWindowPoints(win->hwndCanvas, HWND_DESKTOP, &pt, 1);
    MarkMenuOwnerDraw(popup);
    INT cmd = TrackPopupMenu(popup, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, win->hwndFrame, nullptr);
    FreeMenuOwnerDrawInfoData(popup);
    DestroyMenu(popup);

    if ((UINT)Cmd::OpenSelectedDocument == cmd) {
        LoadArgs args(filePath, win);
        LoadDocument(args);
        return;
    }

    if ((UINT)Cmd::PinSelectedDocument == cmd) {
        state->isPinned = !state->isPinned;
        win->HideToolTip();
        win->RedrawAll(true);
        return;
    }

    if ((UINT)Cmd::ForgetSelectedDocument == cmd) {
        if (state->favorites->size() > 0) {
            // just hide documents with favorites
            gFileHistory.MarkFileInexistent(state->filePath, true);
        } else {
            gFileHistory.Remove(state);
            DeleteDisplayState(state);
        }
        CleanUpThumbnailCache(gFileHistory);
        win->HideToolTip();
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

    TabInfo* tab = win->currentTab;
    PageElement* pageEl = dm->GetElementAtPos({x, y});
    WCHAR* value = nullptr;
    if (pageEl) {
        value = pageEl->GetValue();
    }

    HMENU popup = BuildMenuFromMenuDef(menuDefContext, CreatePopupMenu());

    bool showBookmarksMenu = IsTocEditorEnabledForWindowInfo(win);
    if (!showBookmarksMenu) {
        win::menu::Remove(popup, (UINT)Cmd::NewBookmarks);
    } else {
        auto path = tab->filePath.get();
        if (str::EndsWithI(path, L".vbkm")) {
            // for .vbkm change wording from "New Bookmarks" => "Edit Bookmarks"
            win::menu::SetText(popup, (UINT)Cmd::NewBookmarks, _TR_TODO("Edit Bookmarks"));
        }
    }

    if (!pageEl || pageEl->kind != kindPageElementDest || !value) {
        win::menu::Remove(popup, (UINT)Cmd::CopyLinkTarget);
    }
    if (!pageEl || pageEl->kind != kindPageElementComment || !value) {
        win::menu::Remove(popup, (UINT)Cmd::CopyComment);
    }
    if (!pageEl || pageEl->kind != kindPageElementImage) {
        win::menu::Remove(popup, (UINT)Cmd::CopyImage);
    }

    bool isFullScreen = win->isFullScreen || win->presentation;
    if (!isFullScreen) {
        win::menu::Remove(popup, (UINT)Cmd::ExitFullScreen);
    }
    if (!tab->selectionOnPage) {
        win::menu::SetEnabled(popup, (UINT)Cmd::CopySelection, false);
    }
    MenuUpdatePrintItem(win, popup, true);
    win::menu::SetEnabled(popup, (UINT)Cmd::ViewBookmarks, win->ctrl->HacToc());
    win::menu::SetChecked(popup, (UINT)Cmd::ViewBookmarks, win->tocVisible);

    win::menu::SetEnabled(popup, (UINT)Cmd::FavToggle, HasFavorites());
    win::menu::SetChecked(popup, (UINT)Cmd::FavToggle, gGlobalPrefs->showFavorites);

    bool supportsAnnotations = false;
    EngineBase* engine = dm->GetEngine();
    if (engine) {
        supportsAnnotations = engine->supportsAnnotations;
    }
    bool canDoAnnotations = gIsDebugBuild || gIsPreReleaseBuild || gIsDailyBuild;
    if (!canDoAnnotations) {
        supportsAnnotations = false;
    }
    if (!supportsAnnotations) {
        win::menu::Remove(popup, (UINT)Cmd::SaveAnnotationsSmx);
    } else {
        win::menu::SetEnabled(popup, (UINT)Cmd::SaveAnnotationsSmx, dm->HasUnsavedAnnots());
    }

    int pageNo = dm->GetPageNoByPoint({x, y});
    const WCHAR* filePath = win->ctrl->FilePath();
    if (pageNo > 0) {
        AutoFreeWstr pageLabel = win->ctrl->GetPageLabel(pageNo);
        bool isBookmarked = gFavorites.IsPageInFavorites(filePath, pageNo);
        if (isBookmarked) {
            win::menu::Remove(popup, (UINT)Cmd::FavAdd);

            // %s and not %d because re-using translation from RebuildFavMenu()
            auto tr = _TR("Remove page %s from favorites");
            AutoFreeWstr s = str::Format(tr, pageLabel.Get());
            win::menu::SetText(popup, (UINT)Cmd::FavDel, s);
        } else {
            win::menu::Remove(popup, (UINT)Cmd::FavDel);

            // %s and not %d because re-using translation from RebuildFavMenu()
            auto tr = _TR("Add page %s to favorites\tCtrl+B");
            AutoFreeWstr s = str::Format(tr, pageLabel.Get());
            win::menu::SetText(popup, (UINT)Cmd::FavAdd, s);
        }
    } else {
        win::menu::Remove(popup, (UINT)Cmd::FavAdd);
        win::menu::Remove(popup, (UINT)Cmd::FavDel);
    }

    // if toolbar is not shown, add option to show it
    if (gGlobalPrefs->showToolbar) {
        win::menu::Remove(popup, (UINT)Cmd::ViewShowHideToolbar);
    }

    POINT pt = {x, y};
    MapWindowPoints(win->hwndCanvas, HWND_DESKTOP, &pt, 1);
    MarkMenuOwnerDraw(popup);
    UINT flags = TPM_RETURNCMD | TPM_RIGHTBUTTON;
    INT cmd = TrackPopupMenu(popup, flags, pt.x, pt.y, 0, win->hwndFrame, nullptr);
    FreeMenuOwnerDrawInfoData(popup);
    DestroyMenu(popup);

    switch (cmd) {
        case (UINT)Cmd::CopySelection:
        case (UINT)Cmd::SelectAll:
        case (UINT)Cmd::SaveAs:
        case (UINT)Cmd::Print:
        case (UINT)Cmd::ViewBookmarks:
        case (UINT)Cmd::FavToggle:
        case (UINT)Cmd::Properties:
        case (UINT)Cmd::ViewShowHideToolbar:
        case (UINT)Cmd::SaveAnnotationsSmx:
        case (UINT)Cmd::NewBookmarks:
            SendMessageW(win->hwndFrame, WM_COMMAND, cmd, 0);
            break;
        case (UINT)Cmd::EditAnnotations:
            StartEditAnnotations(tab);
            break;
        case (UINT)Cmd::CopyLinkTarget:
        case (UINT)Cmd::CopyComment:
            CopyTextToClipboard(value);
            break;

        case (UINT)Cmd::CopyImage:
            if (pageEl) {
                RenderedBitmap* bmp = dm->GetEngine()->GetImageForPageElement(pageEl);
                if (bmp) {
                    CopyImageToClipboard(bmp->GetBitmap(), false);
                }
                delete bmp;
            }
            break;
        case (UINT)Cmd::FavAdd:
            AddFavoriteForCurrentPage(win);
            break;
        case (UINT)Cmd::FavDel:
            DelFavorite(filePath, pageNo);
            break;
        case (UINT)Cmd::ExitFullScreen:
            ExitFullScreen(win);
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
    HMENU m = BuildMenuFromMenuDef(menuDefFile, menu, filter);
    if (gIsRaMicroBuild) {
        win::menu::Remove(m, (UINT)Cmd::OpenFolder);
    }
    AppendRecentFilesToMenu(menu);
    AppendExternalViewersToMenu(menu, tab ? tab->filePath.Get() : nullptr);

    // Suppress menu items that depend on specific software being installed:
    // e-mail client, Adobe Reader, Foxit, PDF-XChange
    // Don't hide items here that won't always be hidden
    // (MenuUpdateStateForWindow() is for that)
    if (!CanSendAsEmailAttachment()) {
        win::menu::Remove(menu, (UINT)Cmd::SendByEmail);
    }

    // Also suppress PDF specific items for non-PDF documents
    if (!CouldBePDFDoc(tab) || !CanViewWithAcrobat()) {
        win::menu::Remove(menu, (UINT)Cmd::ViewWithAcrobat);
    }
    if (!CouldBePDFDoc(tab) || !CanViewWithFoxit()) {
        win::menu::Remove(menu, (UINT)Cmd::ViewWithFoxIt);
    }
    if (!CouldBePDFDoc(tab) || !CanViewWithPDFXChange()) {
        win::menu::Remove(menu, (UINT)Cmd::ViewWithPdfXchange);
    }
    if (!CanViewWithXPSViewer(tab)) {
        win::menu::Remove(menu, (UINT)Cmd::ViewWithXpsViewer);
    }
    if (!CanViewWithHtmlHelp(tab)) {
        win::menu::Remove(menu, (UINT)Cmd::ViewWithHtmlHelp);
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
        win::menu::Remove(menu, (UINT)Cmd::SaveAnnotationsSmx);
    } else {
        win::menu::SetEnabled(menu, (UINT)Cmd::SaveAnnotationsSmx, dm && dm->HasUnsavedAnnots());
    }
}

// so that we can do free everything at exit
Vec<MenuOwnerDrawInfo*> g_menuDrawInfos;

void FreeAllMenuDrawInfos() {
    while (g_menuDrawInfos.size() != 0) {
        // Note: could be faster
        FreeMenuOwnerDrawInfo(g_menuDrawInfos[0]);
    }
}

void FreeMenuOwnerDrawInfo(MenuOwnerDrawInfo* modi) {
    g_menuDrawInfos.Remove(modi);
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
void MarkMenuOwnerDraw(HMENU hmenu) {
    if (!gOwnerDrawMenu) {
        return;
    }
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
        g_menuDrawInfos.Append(modi);
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
    TabInfo* tab = win->currentTab;
    HMENU mainMenu = CreateMenu();

    int filter = 0;
    if (win->AsChm()) {
        filter |= MF_NOT_FOR_CHM;
    } else if (win->AsEbook()) {
        filter |= MF_NOT_FOR_EBOOK_UI;
    }
    if (!tab || tab->GetEngineType() != kindEngineComicBooks) {
        filter |= MF_CBX_ONLY;
    }

    HMENU m = CreateMenu();
    RebuildFileMenu(tab, m);
    AppendMenu(mainMenu, MF_POPUP | MF_STRING, (UINT_PTR)m, _TR("&File"));
    m = BuildMenuFromMenuDef(menuDefView, CreateMenu(), filter);
    AppendMenu(mainMenu, MF_POPUP | MF_STRING, (UINT_PTR)m, _TR("&View"));
    m = BuildMenuFromMenuDef(menuDefGoTo, CreateMenu(), filter);
    AppendMenu(mainMenu, MF_POPUP | MF_STRING, (UINT_PTR)m, _TR("&Go To"));
    if (!win->AsEbook()) {
        m = BuildMenuFromMenuDef(menuDefZoom, CreateMenu(), filter);
        AppendMenu(mainMenu, MF_POPUP | MF_STRING, (UINT_PTR)m, _TR("&Zoom"));
    }

    // TODO: implement Favorites for ebooks
    if (HasPermission(Perm_SavePreferences) && !win->AsEbook()) {
        // I think it makes sense to disable favorites in restricted mode
        // because they wouldn't be persisted, anyway
        m = BuildMenuFromMenuDef(menuDefFavorites, CreateMenu());
        RebuildFavMenu(win, m);
        AppendMenu(mainMenu, MF_POPUP | MF_STRING, (UINT_PTR)m, _TR("F&avorites"));
    }

    m = BuildMenuFromMenuDef(menuDefSettings, CreateMenu(), filter);
    if (gIsRaMicroBuild) {
        win::menu::Remove(m, (uint)Cmd::ChangeLanguage);
        win::menu::Remove(m, (UINT)Cmd::AdvancedOptions);
    }
#if defined(ENABLE_THEME)
    // Build the themes sub-menu of the settings menu
    MenuDef menuDefTheme[THEME_COUNT + 1];
    static_assert(IDM_CHANGE_THEME_LAST - IDM_CHANGE_THEME_FIRST + 1 >= THEME_COUNT,
                  "Too many themes. Either remove some or update IDM_CHANGE_THEME_LAST");
    for (UINT i = 0; i < THEME_COUNT; i++) {
        menuDefTheme[i] = {GetThemeByIndex(i)->name, IDM_CHANGE_THEME_FIRST + i, 0};
    }
    HMENU m2 = BuildMenuFromMenuDef(menuDefTheme, CreateMenu(), filter);
    AppendMenu(m, MF_POPUP | MF_STRING, (UINT_PTR)m2, _TR("&Theme"));
#endif
    AppendMenu(mainMenu, MF_POPUP | MF_STRING, (UINT_PTR)m, _TR("&Settings"));

    m = BuildMenuFromMenuDef(menuDefHelp, CreateMenu(), filter);
    AppendMenu(mainMenu, MF_POPUP | MF_STRING, (UINT_PTR)m, _TR("&Help"));
#if 0
    // see MenuBarAsPopupMenu in Caption.cpp
    m = GetSystemMenu(win->hwndFrame, FALSE);
    AppendMenu(mainMenu, MF_POPUP | MF_STRING, (UINT_PTR)m, _TR("&Window"));
#endif

    if (gShowDebugMenu) {
        m = BuildMenuFromMenuDef(menuDefDebug, CreateMenu(), filter);
        if (!gIsRaMicroBuild) {
            win::menu::Remove(m, (UINT)Cmd::AdvancedOptions);
        }

        if (!gIsDebugBuild) {
            RemoveMenu(m, (UINT)Cmd::DebugTestApp, MF_BYCOMMAND);
        }

        if (gAddCrashMeMenu) {
            AppendMenu(m, MF_SEPARATOR, 0, nullptr);
            AppendMenuA(m, MF_STRING, (UINT_PTR)(UINT)Cmd::DebugCrashMe, "Crash me");
        }

        AppendMenu(mainMenu, MF_POPUP | MF_STRING, (UINT_PTR)m, L"Debug");
    }

    MarkMenuOwnerDraw(mainMenu);
    return mainMenu;
}
//] ACCESSKEY_GROUP Main Menubar

void UpdateAppMenu(WindowInfo* win, HMENU m) {
    CrashIf(!win);
    UINT id = GetMenuItemID(m, 0);
    if (id == menuDefFile[0].id) {
        RebuildFileMenu(win->currentTab, m);
    } else if (id == menuDefFavorites[0].id) {
        win::menu::Empty(m);
        BuildMenuFromMenuDef(menuDefFavorites, m);
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
