/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/CmdLineArgsIter.h"
#include "utils/FileUtil.h"
#include "utils/HtmlParserLookup.h"
#include "utils/BitManip.h"
#include "utils/Dpi.h"
#include "utils/GdiPlusUtil.h"
#include "mui/Mui.h"
#include "utils/WinUtil.h"

#include "wingui/UIModels.h"

#include "Settings.h"
#include "DisplayMode.h"
#include "DocController.h"
#include "EngineBase.h"
#include "EngineAll.h"
#include "DisplayModel.h"
#include "FileHistory.h"
#include "Theme.h"
#include "GlobalPrefs.h"
#include "Annotation.h"
#include "AppColors.h"
#include "SumatraConfig.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "resource.h"
#include "Commands.h"
#include "ExternalViewers.h"
#include "Favorites.h"
#include "FileThumbnails.h"
#include "Selection.h"
#include "HomePage.h"
#include "Translations.h"
#include "Toolbar.h"
#include "EditAnnotations.h"
#include "Accelerators.h"
#include "Menu.h"

#include "utils/Log.h"

// SumatraPDF.cpp
extern Annotation* MakeAnnotationsFromSelection(WindowTab* tab, AnnotationType annotType);

struct BuildMenuCtx {
    WindowTab* tab = nullptr;
    bool isCbx = false;
    bool hasSelection = false;
    bool supportsAnnotations = false;
    Annotation* annotationUnderCursor = nullptr;
    bool hasUnsavedAnnotations = false;
    bool isCursorOnPage = false;
    bool canSendEmail = false;
    ~BuildMenuCtx();
};

BuildMenuCtx::~BuildMenuCtx() {
}

// value associated with menu item for owner-drawn purposes
struct MenuOwnerDrawInfo {
    const WCHAR* text = nullptr;
    // copy of MENUITEMINFO fields
    uint fType = 0;
    uint fState = 0;
    HBITMAP hbmpChecked = nullptr;
    HBITMAP hbmpUnchecked = nullptr;
    HBITMAP hbmpItem = nullptr;
};

constexpr UINT kMenuSeparatorID = (UINT)-13;

bool gAddCrashMeMenu = false;

#if defined(DEBUG) || defined(PRE_RELEASE_VER)
bool gShowDebugMenu = true;
#else
bool gShowDebugMenu = false;
#endif

// note: IDM_VIEW_SINGLE_PAGE - IDM_VIEW_CONTINUOUS and also
//       CmdZoomFIT_PAGE - CmdZoomCUSTOM must be in a continuous range!
static_assert(CmdViewLayoutLast - CmdViewLayoutFirst == 4, "view layout ids are not in a continuous range");
static_assert(CmdZoomLast - CmdZoomFirst == 17, "zoom ids are not in a continuous range");

// clang-format off
//[ ACCESSKEY_GROUP File Menu
static MenuDef menuDefFile[] = {
    {
        _TRN("New &window"),
        CmdNewWindow,
    },
    {
        _TRN("&Open..."),
        CmdOpenFile,
    },
    // TODO: should make it available for everyone?
    //{ "Open Folder",                        CmdOpenFolder,             },
    {
        _TRN("&Close"),
        CmdClose,
    },
    {
        _TRN("Show in &folder"),
        CmdShowInFolder,
    },
    {
        _TRN("&Save As..."),
        CmdSaveAs,
    },
    {
        _TRN("Save Annotations to existing PDF"),
        CmdSaveAnnotations,
    },
//[ ACCESSKEY_ALTERNATIVE // only one of these two will be shown
#ifdef ENABLE_SAVE_SHORTCUT
    {
        _TRN("Save S&hortcut..."),
        CmdCreateShortcutToFile,
    },
//| ACCESSKEY_ALTERNATIVE
#else
    {
        _TRN("Re&name..."),
        CmdRenameFile,
    },
#endif
    //] ACCESSKEY_ALTERNATIVE
    {
        _TRN("&Print..."),
        CmdPrint,
    },
    {
        kMenuSeparator,
        0,
    },
    //[ ACCESSKEY_ALTERNATIVE // PDF/XPS/CHM specific items are dynamically removed in RebuildFileMenu
    {
        _TRN("Open Directory in &Explorer"),
        CmdOpenWithExplorer,
    },
    {
        _TRN("Open Directory in Directory &Opus"),
        CmdOpenWithDirectoryOpus,
    },
    {
        _TRN("Open Directory in &Total Commander"),
        CmdOpenWithTotalCommander,
    },
    {
        _TRN("Open Directory in &Double Commander"),
        CmdOpenWithDoubleCommander,
    },
    {
        _TRN("Open in &Adobe Reader"),
        CmdOpenWithAcrobat,
    },
    {
        _TRN("Open in &Foxit Reader"),
        CmdOpenWithFoxIt,
    },
    {
        _TRN("Open &in PDF-XChange"),
        CmdOpenWithPdfXchange,
    },
    //| ACCESSKEY_ALTERNATIVE
    {
        _TRN("Open in &Microsoft XPS-Viewer"),
        CmdOpenWithXpsViewer,
    },
    //| ACCESSKEY_ALTERNATIVE
    {
        _TRN("Open in Microsoft &HTML Help"),
        CmdOpenWithHtmlHelp,
    },
    //] ACCESSKEY_ALTERNATIVE
    // further entries are added if specified in gGlobalPrefs.vecCommandLine
    {
        _TRN("Send by &E-mail..."),
        CmdSendByEmail,
    },
    {
        kMenuSeparator,
        0,
    },
    {
        _TRN("P&roperties"),
        CmdProperties,
    },
    {
        kMenuSeparator,
        0,
    },
    {
        _TRN("E&xit"),
        CmdExit,
    },
    {
        nullptr,
        0,
    },
};
//] ACCESSKEY_GROUP File Menu

//[ ACCESSKEY_GROUP View Menu
static MenuDef menuDefView[] = {
    {
        _TRN("Command Palette"),
        CmdCommandPalette,
    },
    {
        _TRN("&Single Page"),
        CmdSinglePageView,
    },
    {
        _TRN("&Facing"),
        CmdFacingView,
    },
    {
        _TRN("&Book View"),
        CmdBookView,
    },
    {
        _TRN("Show &Pages Continuously"),
        CmdToggleContinuousView,
    },
    // TODO: "&Inverse Reading Direction" (since some Mangas might be read left-to-right)?
    {
        _TRN("Man&ga Mode"),
        CmdToggleMangaMode,
    },
    {
        kMenuSeparator,
        0,
    },
    {
        _TRN("Rotate &Left"),
        CmdRotateLeft,
    },
    {
        _TRN("Rotate &Right"),
        CmdRotateRight,
    },
    {
        kMenuSeparator,
        0,
    },
    {
        _TRN("Pr&esentation"),
        CmdTogglePresentationMode,
    },
    {
        _TRN("F&ullscreen"),
        CmdToggleFullscreen,
    },
    {
        kMenuSeparator,
        0,
    },
    {
        _TRN("Show Book&marks"),
        CmdToggleBookmarks,
    },
    {
        _TRN("Show &Toolbar"),
        CmdToggleToolbar,
    },
    {
        _TRN("Show Scr&ollbars"),
        CmdToggleScrollbars,
    },
    {
        nullptr,
        0,
    },
};
//] ACCESSKEY_GROUP View Menu

//[ ACCESSKEY_GROUP GoTo Menu
static MenuDef menuDefGoTo[] = {
    {
        _TRN("&Next Page"),
        CmdGoToNextPage,
    },
    {
        _TRN("&Previous Page"),
        CmdGoToPrevPage,
    },
    {
        _TRN("&First Page"),
        CmdGoToFirstPage,
    },
    {
        _TRN("&Last Page"),
        CmdGoToLastPage,
    },
    {
        _TRN("Pa&ge..."),
        CmdGoToPage,
    },
    {
        kMenuSeparator,
        0,
    },
    {
        _TRN("&Back"),
        CmdNavigateBack,
    },
    {
        _TRN("F&orward"),
        CmdNavigateForward,
    },
    {
        kMenuSeparator,
        0,
    },
    {
        _TRN("Fin&d..."),
        CmdFindFirst,
    },
    {
        nullptr,
        0,
    },
};
//] ACCESSKEY_GROUP GoTo Menu

//[ ACCESSKEY_GROUP Zoom Menu
static MenuDef menuDefZoom[] = {
    {
        _TRN("Fit &Page"),
        CmdZoomFitPage,
    },
    {
        _TRN("&Actual Size"),
        CmdZoomActualSize,
    },
    {
        _TRN("Fit &Width"),
        CmdZoomFitWidth,
    },
    {
        _TRN("Fit &Content"),
        CmdZoomFitContent,
    },
    {
        _TRN("Custom &Zoom..."),
        CmdZoomCustom,
    },
    {
        kMenuSeparator,
        0,
    },
    {
        "6400%",
        CmdZoom6400,
    },
    {
        "3200%",
        CmdZoom3200,
    },
    {
        "1600%",
        CmdZoom1600,
    },
    {
        "800%",
        CmdZoom800,
    },
    {
        "400%",
        CmdZoom400,
    },
    {
        "200%",
        CmdZoom200,
    },
    {
        "150%",
        CmdZoom150,
    },
    {
        "125%",
        CmdZoom125,
    },
    {
        "100%",
        CmdZoom100,
    },
    {
        "50%",
        CmdZoom50,
    },
    {
        "25%",
        CmdZoom25,
    },
    {
        "12.5%",
        CmdZoom12_5,
    },
    {
        "8.33%",
        CmdZoom8_33,
    },
    {
        nullptr,
        0,
    },
};
//] ACCESSKEY_GROUP Zoom Menu

// TODO: build this dynamically
//[ ACCESSKEY_GROUP Themes Menu
MenuDef menuDefThemes[] = {
    {
        _TRN("Light"),
        CmdThemeFirst,
    },
    {
        _TRN("Dark"),
        CmdThemeFirst+1,
    },
    {
        _TRN("Darker"),
        CmdThemeFirst+2,
    },
    {
        nullptr,
        0,
    },
};
//] ACCESSKEY_GROUP Favorites Menu

//[ ACCESSKEY_GROUP Settings Menu
static MenuDef menuDefSettings[] = {
    {
        _TRN("Change Language"),
        CmdChangeLanguage,
    },
#if 0
    { _TRN("Contribute Translation"),       CmdContributeTranslation },
    { kMenuSeparator,                             0                  },
#endif
    {
        _TRN("&Options..."),
        CmdOptions,
    },
    {
        _TRN("&Advanced Options..."),
        CmdAdvancedOptions,
    },
    {
        _TRN("&Theme"),
        (UINT_PTR)menuDefThemes,
    },
    {
        nullptr,
        0,
    },
};
//] ACCESSKEY_GROUP Settings Menu

//[ ACCESSKEY_GROUP Favorites Menu
MenuDef menuDefFavorites[] = {
    {
        _TRN("Add to favorites"),
        CmdFavoriteAdd,
    },
    {
        _TRN("Remove from favorites"),
        CmdFavoriteDel,
    },
    {
        _TRN("Show Favorites"),
        CmdFavoriteToggle,
    },
    {
        nullptr,
        0,
    },
};
//] ACCESSKEY_GROUP Favorites Menu

//[ ACCESSKEY_GROUP Help Menu
static MenuDef menuDefHelp[] = {
    {
        _TRN("Visit &Website"),
        CmdHelpVisitWebsite,
    },
    {
        _TRN("&Manual"),
        CmdHelpOpenManualInBrowser,
    },
    {
        _TRN("&Keyboard Shortcuts"),
        CmdHelpOpenKeyboardShortcutsInBrowser
    },
    {
        _TRN("Check for &Updates"),
        CmdCheckUpdate,
    },
    {
        kMenuSeparator,
        0,
    },
    {
        _TRN("&About"),
        CmdHelpAbout,
    },
    {
        nullptr,
        0,
    },
};
//] ACCESSKEY_GROUP Help Menu

//[ ACCESSKEY_GROUP Debug Menu
static MenuDef menuDefDebug[] = {
    {
        "Show links",
        CmdToggleLinks,
    },
    {
        "Download symbols",
        CmdDebugDownloadSymbols,
    },
    {
        "Test app",
        CmdDebugTestApp,
    },
    {
        "Show notification",
        CmdDebugShowNotif,
    },
    {
        nullptr,
        0,
    },
};
//] ACCESSKEY_GROUP Debug Menu

//[ ACCESSKEY_GROUP Context Menu (Selection)
static MenuDef menuDefSelection[] = {
    {
        _TRN("&Translate With Google"),
        CmdTranslateSelectionWithGoogle,
    },
    {
        _TRN("Translate with &DeepL"),
        CmdTranslateSelectionWithDeepL,
    },
    {
        _TRN("Search With &Google"),
        CmdSearchSelectionWithGoogle,
    },
    {
        _TRN("Search With &Bing"),
        CmdSearchSelectionWithBing,
    },
    {
        _TRN("Select &All"),
        CmdSelectAll,
    },
    {
        nullptr,
        0,
    },
};
//] ACCESSKEY_GROUP Context Menu (Selection)

//[ ACCESSKEY_GROUP Menu (Selection)
static MenuDef menuDefMainSelection[] = {
    {
        _TRN("&Copy To Clipboard"),
        CmdCopySelection,
    },
    {
        _TRN("&Translate With Google"),
        CmdTranslateSelectionWithGoogle,
    },
    {
        _TRN("Translate with &DeepL"),
        CmdTranslateSelectionWithDeepL,
    },
    {
        _TRN("&Search With Google"),
        CmdSearchSelectionWithGoogle,
    },
    {
        _TRN("Search With &Bing"),
        CmdSearchSelectionWithBing,
    },
    {
        _TRN("Select &All"),
        CmdSelectAll,
    },
    {
        nullptr,
        0,
    },
};
//] ACCESSKEY_GROUP Menu (Selection)

//[ ACCESSKEY_GROUP Menubar
static MenuDef menuDefMenubar[] = {
    {
        _TRN("&File"),
        (UINT_PTR)menuDefFile,
    },
    {
        _TRN("&View"),
        (UINT_PTR)menuDefView,
    },
    {
        _TRN("&Go To"),
        (UINT_PTR)menuDefGoTo,
    },
    {
        _TRN("&Zoom"),
        (UINT_PTR)menuDefZoom,
    },
    {
        _TRN("S&election"),
        (UINT_PTR)menuDefMainSelection,
    },
    {
        _TRN("F&avorites"),
        (UINT_PTR)menuDefFavorites,
    },
    {
        _TRN("&Settings"),
        (UINT_PTR)menuDefSettings,
    },
    {
        _TRN("&Help"),
        (UINT_PTR)menuDefHelp,
    },
    {
        "Debug",
        (UINT_PTR)menuDefDebug,
    },
    {
        nullptr,
        0,
    },
};
//] ACCESSKEY_GROUP Menubar

//[ ACCESSKEY_GROUP Context Menu (Create annot from selection)
static MenuDef menuDefCreateAnnotFromSelection[] = {
    {
        _TRN("&Highlight"),
        CmdCreateAnnotHighlight,
    },
    {
        _TRN("&Underline"),
        CmdCreateAnnotUnderline,
    },
    {
        _TRN("&Strike Out"),
        CmdCreateAnnotStrikeOut,
    },
    {
        _TRN("S&quiggly"),
        CmdCreateAnnotSquiggly,
    },
    //{ _TRN("Redact"), CmdCreateAnnotRedact, },
    {
        nullptr,
        0,
    },
};
//] ACCESSKEY_GROUP Context Menu (Create annot from selection)

//[ ACCESSKEY_GROUP Context Menu (Create annot under cursor)
static MenuDef menuDefCreateAnnotUnderCursor[] = {
    {
        _TRN("&Text"),
        CmdCreateAnnotText,
    },
    {
        _TRN("&Free Text"),
        CmdCreateAnnotFreeText,
    },
    {
        _TRN("&Stamp"),
        CmdCreateAnnotStamp,
    },
    {
        _TRN("&Caret"),
        CmdCreateAnnotCaret,
    },
    //{ _TRN("Ink"), CmdCreateAnnotInk, },
    //{ _TRN("Square"), CmdCreateAnnotSquare, },
    //{ _TRN("Circle"), CmdCreateAnnotCircle, },
    //{ _TRN("Line"), CmdCreateAnnotLine, },
    //{ _TRN("Polygon"), CmdCreateAnnotPolygon, },
    //{ _TRN("Poly Line"), CmdCreateAnnotPolyLine, },
    //{ _TRN("File Attachment"), CmdCreateAnnotFileAttachment, },
    {
        nullptr,
        0,
    },
};
//] ACCESSKEY_GROUP Context Menu (Create annot under cursor)

//[ ACCESSKEY_GROUP Context Menu (Content)
static MenuDef menuDefContext[] = {
    {
        _TRN("&Copy Selection"),
        CmdCopySelection,
    },
    {
        _TRN("S&election"),
        (UINT_PTR)menuDefSelection,
    },
    {
        _TRN("Copy &Link Address"),
        CmdCopyLinkTarget,
    },
    {
        _TRN("Copy Co&mment"),
        CmdCopyComment,
    },
    {
        _TRN("Copy &Image"),
        CmdCopyImage,
    },
    // note: strings cannot be "" or else items are not there
    {
        "Add to favorites",
        CmdFavoriteAdd,
    },
    {
        "Remove from favorites",
        CmdFavoriteDel,
    },
    {
        _TRN("Show &Favorites"),
        CmdFavoriteToggle,
    },
    {
        _TRN("Show &Bookmarks"),
        CmdToggleBookmarks,
    },
    {
        _TRN("Show &Toolbar"),
        CmdToggleToolbar,
    },
    {
        _TRN("Show &Scrollbars"),
        CmdToggleScrollbars,
    },
    {
        kMenuSeparator,
        kMenuSeparatorID,
    },
    {
        _TRN("Edit Annotations"),
        CmdEditAnnotations,
    },
    {
        _TRN("Create Annotation From Selection"),
        (UINT_PTR)menuDefCreateAnnotFromSelection,
    },
    {
        _TRN("Create Annotation &Under Cursor"),
        (UINT_PTR)menuDefCreateAnnotUnderCursor,
    },
    {
        _TRN("Delete Annotation"),
        CmdDeleteAnnotation,
    },
    {
        _TRN("Save Annotations to existing PDF"),
        CmdSaveAnnotations,
    },
    {
        _TRN("E&xit Fullscreen"),
        CmdToggleFullscreen, // only seen in full-screen mode
    },
    {
        nullptr,
        0,
    },
};
//] ACCESSKEY_GROUP Context Menu (Content)

//[ ACCESSKEY_GROUP Context Menu (Start)
static MenuDef menuDefContextStart[] = {
    {
        _TRN("&Open Document"),
        CmdOpenSelectedDocument,
    },
    {
        _TRN("Show in folder"),
        CmdShowInFolder,
    },
    {
        _TRN("&Pin Document"),
        CmdPinSelectedDocument,
    },
    {
        kMenuSeparator,
        0,
    },
    {
        _TRN("&Remove From History"),
        CmdForgetSelectedDocument,
    },
    {
        nullptr,
        0,
    },
};

//] ACCESSKEY_GROUP Context Menu (Start)
// clang-format on

// clang-format off
// those menu items will be disabled if no document is opened, enabled otherwise
static UINT_PTR disableIfNoDocument[] = {
    CmdRotateLeft,
    CmdRotateRight,
    CmdGoToNextPage,
    CmdGoToPrevPage,
    CmdGoToFirstPage,
    CmdGoToLastPage,
    CmdNavigateBack,
    CmdNavigateForward,
    CmdGoToPage,
    CmdFindFirst,
    CmdSaveAs,
    CmdCreateShortcutToFile,
    CmdSendByEmail,
    CmdSelectAll,
    CmdProperties,
    CmdTogglePresentationMode,
    CmdRenameFile,
    CmdShowInFolder,
    // IDM_VIEW_WITH_XPS_VIEWER and IDM_VIEW_WITH_HTML_HELP
    // are removed instead of disabled (and can remain enabled
    // for broken XPS/CHM documents)
};

static UINT_PTR disableIfDirectoryOrBrokenPDF[] = {
    CmdRenameFile,
    CmdSendByEmail,
    CmdOpenWithAcrobat,
    CmdOpenWithFoxIt,
    CmdOpenWithPdfXchange,
    CmdShowInFolder, // TODO: why?
};

UINT_PTR disableIfNoSelection[] = {
    CmdCopySelection,
    CmdTranslateSelectionWithDeepL,
    CmdTranslateSelectionWithGoogle,
    CmdSearchSelectionWithBing,
    CmdSearchSelectionWithGoogle,
    CmdCreateAnnotHighlight,
    CmdCreateAnnotSquiggly,
    CmdCreateAnnotStrikeOut,
    CmdCreateAnnotUnderline,
    0,
};

static UINT_PTR menusNoTranslate[] = {
    CmdZoom6400,
    CmdZoom3200,
    CmdZoom1600,
    CmdZoom800,
    CmdZoom400,
    CmdZoom200,
    CmdZoom150,
    CmdZoom125,
    CmdZoom100,
    CmdZoom50,
    CmdZoom25,
    CmdZoom12_5,
    CmdZoom8_33,
};

UINT_PTR removeIfNoInternetPerms[] = {
    CmdCheckUpdate,
    CmdTranslateSelectionWithGoogle,
    CmdTranslateSelectionWithDeepL,
    CmdSearchSelectionWithGoogle,
    CmdSearchSelectionWithBing,
    CmdHelpVisitWebsite,
    CmdHelpOpenManualInBrowser,
    CmdHelpOpenKeyboardShortcutsInBrowser,
    CmdContributeTranslation,
    0,
};

UINT_PTR removeIfNoFullscreenPerms[] = {
    CmdTogglePresentationMode,
    CmdToggleFullscreen,
    0,
};

UINT_PTR removeIfNoPrefsPerms[] = {
    CmdOptions,
    CmdAdvancedOptions,
    CmdPinSelectedDocument,
    CmdForgetSelectedDocument,
    CmdFavoriteAdd,
    CmdFavoriteDel,
    CmdFavoriteToggle,
    0,
};

UINT_PTR removeIfNoCopyPerms[] = {
    // TODO: probably those are covered by menuDefSelection
    CmdTranslateSelectionWithGoogle,
    CmdTranslateSelectionWithDeepL,
    CmdSearchSelectionWithGoogle,
    CmdSearchSelectionWithBing,
    CmdSelectAll,

    CmdCopySelection,
    CmdCopyLinkTarget,
    CmdCopyComment,
    CmdCopyImage,
    (UINT_PTR)menuDefSelection,
    (UINT_PTR)menuDefMainSelection,
    0,
};

// TODO: all prefs params also fall under disk access
UINT_PTR removeIfNoDiskAccessPerm[] = {
    CmdNewWindow, // ???
    CmdOpenFile,
    CmdOpenFolder,
    CmdClose, // ???
    CmdShowInFolder,
    CmdSaveAs,
    CmdRenameFile,
    CmdSendByEmail, // ???
    CmdContributeTranslation, // ???
    CmdAdvancedOptions,
    CmdAdvancedSettings,
    CmdFavoriteAdd,
    CmdFavoriteDel,
    CmdFavoriteToggle,
    CmdOpenSelectedDocument,
    CmdPinSelectedDocument,
    CmdForgetSelectedDocument,
    0,
};

UINT_PTR removeIfAnnotsNotSupported[] = {
    CmdSaveAnnotations,
    CmdSaveAnnotationsNewFile,
    //CmdSelectAnnotation,
    CmdEditAnnotations,
    CmdDeleteAnnotation,
    (UINT_PTR)menuDefCreateAnnotFromSelection,
    (UINT_PTR)menuDefCreateAnnotUnderCursor,
    0,
};

UINT_PTR removeIfChm[] = {
    CmdSinglePageView,
    CmdFacingView,
    CmdBookView,
    CmdToggleContinuousView,
    CmdRotateLeft,
    CmdRotateRight,
    CmdTogglePresentationMode,
    CmdToggleScrollbars,
    CmdZoomFitPage,
    CmdZoomActualSize,
    CmdZoomFitWidth,
    CmdZoomFitContent,
    CmdZoom6400,
    CmdZoom3200,
    CmdZoom1600,
    CmdZoom800,
    CmdZoom12_5,
    CmdZoom8_33,
    (UINT_PTR)menuDefContext,
    0,
};
// clang-format on

static bool __cmdIdInList(UINT_PTR cmdId, UINT_PTR* idsList, int n) {
    for (int i = 0; i < n; i++) {
        UINT_PTR id = idsList[i];
        if (id == cmdId) {
            return true;
        }
    }
    return false;
}

#define cmdIdInList(name) __cmdIdInList(md.idOrSubmenu, name, dimof(name))

static void AddFileMenuItem(HMENU menuFile, const char* filePath, int index) {
    ReportIf(!filePath || !menuFile);
    if (!filePath || !menuFile) {
        return;
    }

    TempStr menuString = (TempStr)path::GetBaseNameTemp(filePath);

    // If the name is too long, save only the ends glued together
    // E.g. 'Very Long PDF Name (3).pdf' -> 'Very Long...e (3).pdf'
    const size_t MAX_LEN = 70;
    size_t menuStrLen = str::Len(menuString);
    if (menuStrLen > MAX_LEN) {
        char* newStr = AllocArrayTemp<char>(MAX_LEN);
        const size_t half = MAX_LEN / 2;
        const size_t strSize = menuStrLen + 1; // size()+1 because wcslen() doesn't include \0
        // Copy first N/2 characters, move last N/2 characters to the halfway point
        for (size_t i = 0; i < half; i++) {
            newStr[i] = menuString[i];
            newStr[i + half] = menuString[strSize - half + i];
        }
        // Add ellipsis
        newStr[half - 2] = newStr[half - 1] = newStr[half] = '.';
        // Ensure null-terminated string
        newStr[MAX_LEN - 1] = '\0';
        // Save truncated string
        menuString = newStr;
    }

    TempStr fileName = MenuToSafeStringTemp(menuString);
    int menuIdx = (int)((index + 1) % 10);
    menuString = str::FormatTemp("&%d) %s", menuIdx, fileName);
    uint menuId = CmdFileHistoryFirst + index;
    uint flags = MF_BYCOMMAND | MF_ENABLED | MF_STRING;
    InsertMenuW(menuFile, CmdExit, flags, menuId, ToWStrTemp(menuString));
}

static void AppendRecentFilesToMenu(HMENU m) {
    if (!HasPermission(Perm::DiskAccess)) {
        return;
    }

    int i;
    for (i = 0; i < kFileHistoryMaxRecent; i++) {
        FileState* fs = gFileHistory.Get(i);
        if (!fs || fs->isMissing) {
            break;
        }
        const char* fp = fs->filePath;
        if (!fp) {
            // comes from settings file so can be missing due to user modifications
            continue;
        }
        AddFileMenuItem(m, fp, i);
    }

    if (i > 0) {
        InsertMenuW(m, CmdExit, MF_BYCOMMAND | MF_SEPARATOR, 0, nullptr);
    }
}

void FillBuildMenuCtx(WindowTab* tab, BuildMenuCtx* ctx, Point pt) {
    if (!tab) {
        return;
    }
    ctx->tab = tab;
    EngineBase* engine = tab->GetEngine();
    if (engine && (engine->kind == kindEngineComicBooks)) {
        ctx->isCbx = true;
    }
    ctx->supportsAnnotations = EngineSupportsAnnotations(engine) && !tab->win->isFullScreen;
    ctx->hasUnsavedAnnotations = EngineHasUnsavedAnnotations(engine);
    ctx->canSendEmail = CanSendAsEmailAttachment(tab);

    DisplayModel* dm = tab->AsFixed();
    if (dm && ctx->supportsAnnotations) {
        int pageNoUnderCursor = dm->GetPageNoByPoint(pt);
        if (pageNoUnderCursor > 0) {
            ctx->isCursorOnPage = true;
        }
        ctx->annotationUnderCursor = dm->GetAnnotationAtPos(pt, nullptr);
    }
    ctx->hasSelection = tab->win->showSelection && tab->selectionOnPage;
}

static void AppendSelectionHandlersToMenu(HMENU m, bool isEnabled) {
    if (!HasPermission(Perm::InternetAccess) || !HasPermission(Perm::CopySelection)) {
        // TODO: when we add exe handlers, only filter the URL ones
        return;
    }
    int maxEntries = CmdSelectionHandlerLast - CmdSelectionHandlerFirst;
    int n = 0;
    for (auto& sh : *gGlobalPrefs->selectionHandlers) {
        if (!sh || !sh->url || !sh->name) {
            // can happen for bad selection handler definition
            continue;
        }
        if (str::EmptyOrWhiteSpaceOnly(sh->url) || str::EmptyOrWhiteSpaceOnly(sh->name)) {
            continue;
        }
        if (n >= maxEntries) {
            break;
        }
        WCHAR* name = ToWStrTemp(sh->name);
        sh->cmdID = (int)CmdSelectionHandlerFirst + n;
        UINT flags = MF_STRING;
        flags |= isEnabled ? MF_ENABLED : MF_DISABLED;
        AppendMenuW(m, flags, (UINT_PTR)sh->cmdID, name);
        n++;
    }
}

static void AppendExternalViewersToMenu(HMENU menuFile, const char* filePath) {
    if (0 == gGlobalPrefs->externalViewers->size()) {
        return;
    }
    if (!HasPermission(Perm::DiskAccess) || (filePath && !file::Exists(filePath))) {
        return;
    }

    int maxEntries = CmdOpenWithExternalLast - CmdOpenWithExternalFirst;
    int count = 0;

    for (ExternalViewer* ev : *gGlobalPrefs->externalViewers) {
        if (count >= maxEntries) {
            break;
        }
        if (str::EmptyOrWhiteSpaceOnly(ev->commandLine)) {
            continue;
        }
        if (ev->filter && !(filePath && PathMatchFilter(filePath, ev->filter))) {
            continue;
        }

        char* name = ev->name;
        if (str::EmptyOrWhiteSpaceOnly(name)) {
            CmdLineArgsIter args(ToWStrTemp(ev->commandLine));
            int nArgs = args.nArgs - 2;
            if (nArgs <= 0) {
                continue;
            }
            char* arg0 = args.at(2 + 0);
            name = str::DupTemp(path::GetBaseNameTemp(arg0));
            char* pos = str::FindChar(name, '.');
            if (pos) {
                *pos = 0;
            }
        }

        TempStr menuString = str::FormatTemp(_TRA("Open in %s"), name);
        uint menuId = CmdOpenWithExternalFirst + count;
        TempWStr ws = ToWStrTemp(menuString);
        InsertMenuW(menuFile, menuId, MF_BYCOMMAND | MF_ENABLED | MF_STRING, menuId, ws);
        if (!filePath) {
            MenuSetEnabled(menuFile, menuId, false);
        }
        count++;
    }
}

// shows duplicate separator if no external viewers
static void DynamicPartOfFileMenu(HMENU menu, BuildMenuCtx* ctx) {
    AppendRecentFilesToMenu(menu);

    // Suppress menu items that depend on specific software being installed:
    // e-mail client, Adobe Reader, Foxit, PDF-XChange
    // Don't hide items here that won't always be hidden
    // (MenuUpdateStateForWindow() is for that)
    WindowTab* tab = ctx->tab;
    for (int cmd = CmdOpenWithFirst + 1; cmd < CmdOpenWithLast; cmd++) {
        if (!CanViewWithKnownExternalViewer(tab, cmd)) {
            MenuRemove(menu, cmd);
        }
    }
}

void RemoveBadMenuSeparators(HMENU menu) {
    int nMenus;
    // remove separator items at the beginning
again1:
    nMenus = GetMenuItemCount(menu);
    if (nMenus == 0) {
        return;
    }
    UINT id = GetMenuItemID(menu, 0);
    if (id == kMenuSeparatorID) {
        RemoveMenu(menu, 0, MF_BYPOSITION);
        goto again1;
    }
    // remove separator items at the end
again2:
    nMenus = GetMenuItemCount(menu);
    if (nMenus == 0) {
        return;
    }
    id = GetMenuItemID(menu, nMenus - 1);
    if (id == kMenuSeparatorID) {
        RemoveMenu(menu, nMenus - 1, MF_BYPOSITION);
        goto again2;
    }
    // remove 2 or more consequitive separator items
again3:
    nMenus = GetMenuItemCount(menu);
    for (int i = 1; i < nMenus; i++) {
        id = GetMenuItemID(menu, i);
        UINT idPrev = GetMenuItemID(menu, i - 1);
        if ((id == idPrev) && (id == kMenuSeparatorID)) {
            RemoveMenu(menu, i, MF_BYPOSITION);
            goto again3;
        }
    }
}

static void RebuildFileMenu(WindowTab* tab, HMENU menu) {
    MenuEmpty(menu);
    BuildMenuCtx buildCtx;
    FillBuildMenuCtx(tab, &buildCtx, Point{0, 0});
    BuildMenuFromMenuDef(menuDefFile, menu, &buildCtx);
    DynamicPartOfFileMenu(menu, &buildCtx);
    RemoveBadMenuSeparators(menu);
}

HMENU BuildMenuFromMenuDef(MenuDef* menuDef, HMENU menu, BuildMenuCtx* ctx) {
    CrashIf(!menu);

    bool isDebugMenu = menuDef == menuDefDebug;
    int i = 0;

    // insert before built-in selection handlers
    if (menuDef == menuDefSelection) {
        AppendSelectionHandlersToMenu(menu, ctx ? ctx->hasSelection : false);
    }

    ACCEL accel;
    while (true) {
        MenuDef md = menuDef[i];
        if (md.title == nullptr) { // sentinel
            break;
        }
        i++;

        int cmdId = (int)md.idOrSubmenu;
        if (menuDef == menuDefMainSelection && cmdId == CmdTranslateSelectionWithGoogle) {
            AppendSelectionHandlersToMenu(menu, true);
        }

        MenuDef* subMenuDef = (MenuDef*)md.idOrSubmenu;
        // hacky but works: small number is command id, large is submenu (a pointer)
        bool isSubMenu = md.idOrSubmenu > CmdLast + 10000;

        bool disableMenu = false;
        bool removeMenu = false;
        if (!HasPermission(Perm::InternetAccess)) {
            removeMenu |= cmdIdInList(removeIfNoInternetPerms);
        }
        if (!HasPermission(Perm::FullscreenAccess)) {
            removeMenu |= cmdIdInList(removeIfNoFullscreenPerms);
        }
        if (!HasPermission(Perm::SavePreferences)) {
            removeMenu |= cmdIdInList(removeIfNoPrefsPerms);
        }
        if (!HasPermission(Perm::PrinterAccess)) {
            removeMenu |= (cmdId == CmdPrint);
        }
        if (!HasPermission(Perm::DiskAccess)) {
            removeMenu |= cmdIdInList(removeIfNoDiskAccessPerm);
            // editing annotations also requires disk access
            removeMenu |= cmdIdInList(removeIfAnnotsNotSupported);
            if (cmdId >= CmdOpenWithFirst && cmdId <= CmdOpenWithLast) {
                removeMenu = true;
            }
        }
        if (!HasPermission(Perm::CopySelection)) {
            removeMenu |= cmdIdInList(removeIfNoCopyPerms);
        }
        if ((cmdId == CmdCheckUpdate) && gIsStoreBuild) {
            removeMenu = true;
        }

        if (ctx) {
            removeMenu |= (ctx->tab && ctx->tab->AsChm() && cmdIdInList(removeIfChm));
            removeMenu |= (!ctx->isCbx && (cmdId == CmdToggleMangaMode));
            removeMenu |= (!ctx->supportsAnnotations && cmdIdInList(removeIfAnnotsNotSupported));
            removeMenu |= !ctx->canSendEmail && (cmdId == CmdSendByEmail);

            disableMenu |= (!ctx->hasSelection && cmdIdInList(disableIfNoSelection));
            // disableMenu |= (!ctx->annotationUnderCursor && (cmdId == CmdSelectAnnotation));
            disableMenu |= (!ctx->annotationUnderCursor && (cmdId == CmdDeleteAnnotation));
            disableMenu |= !ctx->hasUnsavedAnnotations && (cmdId == CmdSaveAnnotations);

            removeMenu |= !ctx->isCursorOnPage && (subMenuDef == menuDefCreateAnnotUnderCursor);
            removeMenu |= !ctx->hasSelection && (subMenuDef == menuDefCreateAnnotFromSelection);
        }
        removeMenu |= ((subMenuDef == menuDefDebug) && !gShowDebugMenu);
        if (removeMenu) {
            continue;
        }

        // prevent two consecutive separators
        if (str::Eq(md.title, kMenuSeparator)) {
            AppendMenuW(menu, MF_SEPARATOR, kMenuSeparatorID, nullptr);
            continue;
        }

        bool noTranslate = isDebugMenu || cmdIdInList(menusNoTranslate);
        noTranslate |= (subMenuDef == menuDefDebug);
        const char* title = md.title;
        if (!noTranslate) {
            title = trans::GetTranslationA(md.title);
        }

        if (isSubMenu) {
            HMENU subMenu = BuildMenuFromMenuDef(subMenuDef, CreatePopupMenu(), ctx);
            UINT flags = MF_POPUP | (disableMenu ? MF_DISABLED : MF_ENABLED);
            if (subMenuDef == menuDefFile) {
                DynamicPartOfFileMenu(subMenu, ctx);
            }
            WCHAR* ws = ToWStrTemp(title);
            AppendMenuW(menu, flags, (UINT_PTR)subMenu, ws);
        } else {
            str::Str title2 = title;
            if (GetAccelByCmd(cmdId, accel)) {
                // if this is an accelerator, append it to menu
                if (!str::Find(title, "\t")) {
                    AppendAccelKeyToMenuString(title2, accel);
                }
            }
            UINT flags = MF_STRING | (disableMenu ? MF_DISABLED : MF_ENABLED);
            WCHAR* ws = ToWStrTemp(title2.Get());
            AppendMenuW(menu, flags, md.idOrSubmenu, ws);
        }

        if (cmdId == CmdOpenWithHtmlHelp && ctx) {
            WindowTab* tab = ctx->tab;
            char* path = tab ? tab->filePath : nullptr;
            AppendExternalViewersToMenu(menu, path);
        }
    }
    RemoveBadMenuSeparators(menu);
    return menu;
}

// clang-format off
static struct {
    int itemId;
    float zoom;
} gZoomMenuIds[] = {
    { CmdZoom6400,        6400.0 },
    { CmdZoom3200,        3200.0 },
    { CmdZoom1600,        1600.0 },
    { CmdZoom800,         800.0  },
    { CmdZoom400,         400.0  },
    { CmdZoom200,         200.0  },
    { CmdZoom150,         150.0  },
    { CmdZoom125,         125.0  },
    { CmdZoom100,         100.0  },
    { CmdZoom50,          50.0   },
    { CmdZoom25,          25.0   },
    { CmdZoom12_5,        12.5   },
    { CmdZoom8_33,        8.33f  },
    { CmdZoomCustom,      0      },
    { CmdZoomFitPage,    kZoomFitPage    },
    { CmdZoomFitWidth,   kZoomFitWidth   },
    { CmdZoomFitContent, kZoomFitContent },
    { CmdZoomActualSize, kZoomActualSize },
};
// clang-format on

int MenuIdFromVirtualZoom(float virtualZoom) {
    for (auto&& it : gZoomMenuIds) {
        if (virtualZoom == it.zoom) {
            return it.itemId;
        }
    }
    return CmdZoomCustom;
}

float ZoomMenuItemToZoom(int menuItemId) {
    for (auto&& it : gZoomMenuIds) {
        if (menuItemId == it.itemId) {
            return it.zoom;
        }
    }
    CrashIf(true);
    return 100.0;
}

static void ZoomMenuItemCheck(HMENU m, int menuItemId, bool canZoom) {
    CrashIf((CmdZoomFirst > menuItemId) || (menuItemId > CmdZoomLast));

    for (auto&& it : gZoomMenuIds) {
        MenuSetEnabled(m, it.itemId, canZoom);
    }

    if (CmdZoom100 == menuItemId) {
        menuItemId = CmdZoomActualSize;
    }
    CheckMenuRadioItem(m, CmdZoomFirst, CmdZoomLast, menuItemId, MF_BYCOMMAND);
    if (CmdZoomActualSize == menuItemId) {
        CheckMenuRadioItem(m, CmdZoom100, CmdZoom100, CmdZoom100, MF_BYCOMMAND);
    }
}

void MenuUpdateZoom(MainWindow* win) {
    float zoomVirtual = gGlobalPrefs->defaultZoomFloat;
    if (win->IsDocLoaded()) {
        zoomVirtual = win->ctrl->GetZoomVirtual();
    }
    int menuId = MenuIdFromVirtualZoom(zoomVirtual);
    ZoomMenuItemCheck(win->menu, menuId, win->IsDocLoaded());
}

void MenuUpdatePrintItem(MainWindow* win, HMENU menu, bool disableOnly = false) {
    bool filePrintEnabled = win->IsDocLoaded();
#ifndef DISABLE_DOCUMENT_RESTRICTIONS
    bool filePrintAllowed = !filePrintEnabled || !win->AsFixed() || win->AsFixed()->GetEngine()->AllowsPrinting();
#else
    bool filePrintAllowed = true;
#endif

    for (auto& def : menuDefFile) {
        if (def.idOrSubmenu != CmdPrint) {
            continue;
        }
        str::Str printItem = trans::GetTranslationA(def.title);
        if (!filePrintAllowed) {
            printItem = _TRA("&Print... (denied)");
        } else {
            ACCEL accel;
            if (GetAccelByCmd(CmdPrint, accel)) {
                AppendAccelKeyToMenuString(printItem, accel);
            }
        }
        if (!filePrintAllowed || !disableOnly) {
            WCHAR* ws = ToWStrTemp(printItem.Get());
            ModifyMenuW(menu, CmdPrint, MF_BYCOMMAND | MF_STRING, (UINT_PTR)CmdPrint, ws);
        }
        MenuSetEnabled(menu, CmdPrint, filePrintEnabled && filePrintAllowed);
    }
}

static bool IsFileCloseMenuEnabled() {
    for (size_t i = 0; i < gWindows.size(); i++) {
        if (gWindows.at(i)->IsDocLoaded()) {
            return true;
        }
    }
    return false;
}

static void SetMenuStateForSelection(WindowTab* tab, HMENU menu) {
    bool isTextSelected = tab && tab->win && tab->win->showSelection && tab->selectionOnPage;
    for (int id : disableIfNoSelection) {
        MenuSetEnabled(menu, id, isTextSelected);
    }
    for (int id = CmdSelectionHandlerFirst; id < CmdSelectionHandlerLast; id++) {
        MenuSetEnabled(menu, id, isTextSelected);
    }
}

void MenuUpdateDisplayMode(MainWindow* win) {
    bool enabled = win->IsDocLoaded();
    DisplayMode displayMode = gGlobalPrefs->defaultDisplayModeEnum;
    if (enabled) {
        displayMode = win->ctrl->GetDisplayMode();
    }

    for (int id = CmdViewLayoutFirst; id <= CmdViewLayoutLast; id++) {
        MenuSetEnabled(win->menu, id, enabled);
    }

    int id = 0;
    if (IsSingle(displayMode)) {
        id = CmdSinglePageView;
    } else if (IsFacing(displayMode)) {
        id = CmdFacingView;
    } else if (IsBookView(displayMode)) {
        id = CmdBookView;
    } else {
        CrashIf(win->ctrl || DisplayMode::Automatic != displayMode);
    }

    CheckMenuRadioItem(win->menu, CmdViewLayoutFirst, CmdViewLayoutLast, id, MF_BYCOMMAND);
    MenuSetChecked(win->menu, CmdToggleContinuousView, IsContinuous(displayMode));

    if (win->CurrentTab() && win->CurrentTab()->GetEngineType() == kindEngineComicBooks) {
        bool mangaMode = win->AsFixed()->GetDisplayR2L();
        MenuSetChecked(win->menu, CmdToggleMangaMode, mangaMode);
    }
}

static void MenuUpdateStateForWindow(MainWindow* win) {
    WindowTab* tab = win->CurrentTab();

    bool hasDocument = tab && tab->IsDocLoaded();
    for (UINT_PTR id = CmdOpenWithFirst; id < CmdOpenWithLast; id++) {
        MenuSetEnabled(win->menu, id, hasDocument);
    }
    for (int id : disableIfNoDocument) {
        MenuSetEnabled(win->menu, id, hasDocument);
    }

    SetMenuStateForSelection(tab, win->menu);
    MenuSetEnabled(win->menu, CmdClose, IsFileCloseMenuEnabled());

    MenuUpdatePrintItem(win, win->menu);

    bool enabled = win->IsDocLoaded() && tab && tab->ctrl->HasToc();
    MenuSetEnabled(win->menu, CmdToggleBookmarks, enabled);

    bool documentSpecific = win->IsDocLoaded();
    bool checked = documentSpecific ? win->tocVisible : gGlobalPrefs->showToc;
    MenuSetChecked(win->menu, CmdToggleBookmarks, checked);

    MenuSetChecked(win->menu, CmdFavoriteToggle, gGlobalPrefs->showFavorites);
    MenuSetChecked(win->menu, CmdToggleToolbar, gGlobalPrefs->showToolbar);
    MenuSetChecked(win->menu, CmdToggleScrollbars, !gGlobalPrefs->fixedPageUI.hideScrollbars);
    MenuUpdateDisplayMode(win);
    MenuUpdateZoom(win);

    if (win->IsDocLoaded() && tab) {
        MenuSetEnabled(win->menu, CmdNavigateBack, tab->ctrl->CanNavigate(-1));
        MenuSetEnabled(win->menu, CmdNavigateForward, tab->ctrl->CanNavigate(1));
    }

    // TODO: is this check too expensive?
    bool fileExists = tab && file::Exists(tab->filePath);

    if (tab && tab->ctrl && !fileExists && dir::Exists(tab->filePath)) {
        for (int id : disableIfDirectoryOrBrokenPDF) {
            MenuSetEnabled(win->menu, id, false);
        }
    } else if (fileExists && CouldBePDFDoc(tab)) {
        for (int id : disableIfDirectoryOrBrokenPDF) {
            MenuSetEnabled(win->menu, id, true);
        }
    }

    DisplayModel* dm = tab ? tab->AsFixed() : nullptr;
    EngineBase* engine = dm ? dm->GetEngine() : nullptr;
    if (engine) {
        MenuSetEnabled(win->menu, CmdFindFirst, !engine->IsImageCollection());
    }

    if (win->IsDocLoaded() && !fileExists) {
        MenuSetEnabled(win->menu, CmdRenameFile, false);
    }

    int themeCmdId = CmdThemeFirst + GetCurrentThemeIndex();
    CheckMenuRadioItem(win->menu, CmdThemeFirst, CmdThemeLast, themeCmdId, MF_BYCOMMAND);

    MenuSetChecked(win->menu, CmdToggleLinks, gGlobalPrefs->showLinks);
}

// TODO: not the best file for this
void ShowFileInFolder(const char* path) {
    if (!HasPermission(Perm::DiskAccess)) {
        return;
    }
    const char* process = "explorer.exe";
    TempStr args = str::FormatTemp("/select,\"%s\"", path);
    CreateProcessHelper(process, args);
}

void OnAboutContextMenu(MainWindow* win, int x, int y) {
    if (!HasPermission(Perm::SavePreferences | Perm::DiskAccess) || !gGlobalPrefs->rememberOpenedFiles ||
        !gGlobalPrefs->showStartPage) {
        return;
    }

    char* path = GetStaticLinkTemp(win->staticLinks, x, y, nullptr);
    if (!path || *path == '<' || str::StartsWith(path, "http://") || str::StartsWith(path, "https://")) {
        return;
    }

    FileState* fs = gFileHistory.FindByPath(path);
    CrashIf(!fs);
    if (!fs) {
        return;
    }

    HMENU popup = BuildMenuFromMenuDef(menuDefContextStart, CreatePopupMenu(), nullptr);
    MenuSetChecked(popup, CmdPinSelectedDocument, fs->isPinned);
    POINT pt = {x, y};
    MapWindowPoints(win->hwndCanvas, HWND_DESKTOP, &pt, 1);
    MarkMenuOwnerDraw(popup);
    INT cmd = TrackPopupMenu(popup, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, win->hwndFrame, nullptr);
    FreeMenuOwnerDrawInfoData(popup);
    DestroyMenu(popup);

    if (CmdOpenSelectedDocument == cmd) {
        LoadArgs args(path, win);
        args.activateExisting = !IsCtrlPressed();
        LoadDocument(&args);
        return;
    }

    if (CmdShowInFolder == cmd) {
        ShowFileInFolder(path);
        return;
    }

    if (CmdPinSelectedDocument == cmd) {
        fs->isPinned = !fs->isPinned;
        win->DeleteToolTip();
        win->RedrawAll(true);
        return;
    }

    if (CmdForgetSelectedDocument == cmd) {
        if (fs->favorites->size() > 0) {
            // just hide documents with favorites
            gFileHistory.MarkFileInexistent(fs->filePath, true);
        } else {
            gFileHistory.Remove(fs);
            DeleteDisplayState(fs);
        }
        CleanUpThumbnailCache(gFileHistory);
        win->DeleteToolTip();
        win->RedrawAll(true);
        return;
    }
}

void OnWindowContextMenu(MainWindow* win, int x, int y) {
    DisplayModel* dm = win->AsFixed();
    CrashIf(!dm);
    if (!dm) {
        return;
    }

    Point cursorPos{x, y};
    WindowTab* tab = win->CurrentTab();
    IPageElement* pageEl = dm->GetElementAtPos(cursorPos, nullptr);

    char* value = nullptr;
    if (pageEl) {
        value = pageEl->GetValue();
    }

    BuildMenuCtx buildCtx;
    FillBuildMenuCtx(tab, &buildCtx, cursorPos);
    HMENU popup = BuildMenuFromMenuDef(menuDefContext, CreatePopupMenu(), &buildCtx);

    int pageNoUnderCursor = dm->GetPageNoByPoint(cursorPos);
    PointF ptOnPage = dm->CvtFromScreen(cursorPos, pageNoUnderCursor);
    EngineBase* engine = dm->GetEngine();

    if (!pageEl || !pageEl->Is(kindPageElementDest) || !value) {
        MenuRemove(popup, CmdCopyLinkTarget);
    }
    if (!pageEl || !pageEl->Is(kindPageElementComment) || !value) {
        MenuRemove(popup, CmdCopyComment);
    }
    if (!pageEl || !pageEl->Is(kindPageElementImage)) {
        MenuRemove(popup, CmdCopyImage);
    }

    bool isFullScreen = win->isFullScreen || win->presentation;
    if (!isFullScreen) {
        MenuRemove(popup, CmdToggleFullscreen);
    }
    SetMenuStateForSelection(tab, popup);

    MenuUpdatePrintItem(win, popup, true);
    MenuSetEnabled(popup, CmdToggleBookmarks, win->ctrl->HasToc());
    MenuSetChecked(popup, CmdToggleBookmarks, win->tocVisible);

    MenuSetChecked(popup, CmdToggleScrollbars, !gGlobalPrefs->fixedPageUI.hideScrollbars);

    MenuSetEnabled(popup, CmdFavoriteToggle, HasFavorites());
    MenuSetChecked(popup, CmdFavoriteToggle, gGlobalPrefs->showFavorites);

    if (buildCtx.annotationUnderCursor) {
        // change from generic "Edit Annotations" to more specific
        // "Edit ${annotType} Annotation"
        TempStr t = AnnotationReadableNameTemp(buildCtx.annotationUnderCursor->type);
        TempStr s = str::FormatTemp(_TRN("Edit %s Annotation"), t);
        MenuSetText(popup, CmdEditAnnotations, s);
    }

    const char* filePath = win->ctrl->GetFilePath();
    bool favsSupported = HasPermission(Perm::SavePreferences) && HasPermission(Perm::DiskAccess);
    if (favsSupported) {
        if (pageNoUnderCursor > 0) {
            TempStr pageLabel = win->ctrl->GetPageLabeTemp(pageNoUnderCursor);
            bool isBookmarked = gFavorites.IsPageInFavorites(filePath, pageNoUnderCursor);
            if (isBookmarked) {
                MenuRemove(popup, CmdFavoriteAdd);

                // %s and not %d because re-using translation from RebuildFavMenu()
                const char* tr = _TRA("Remove page %s from favorites");
                TempStr s = str::FormatTemp(tr, pageLabel);
                MenuSetText(popup, CmdFavoriteDel, s);
            } else {
                MenuRemove(popup, CmdFavoriteDel);

                // %s and not %d because re-using translation from RebuildFavMenu()
                str::Str str = _TRA("Add page %s to favorites");
                ACCEL a;
                bool ok = GetAccelByCmd(CmdFavoriteAdd, a);
                if (ok) {
                    AppendAccelKeyToMenuString(str, a);
                }
                TempStr s = str::FormatTemp(str.Get(), pageLabel);
                MenuSetText(popup, CmdFavoriteAdd, s);
            }
        } else {
            MenuRemove(popup, CmdFavoriteAdd);
            MenuRemove(popup, CmdFavoriteDel);
        }
    }

    // if toolbar is not shown, add option to show it
    if (gGlobalPrefs->showToolbar) {
        MenuRemove(popup, CmdToggleToolbar);
    }
    RemoveBadMenuSeparators(popup);

    POINT pt = {x, y};
    MapWindowPoints(win->hwndCanvas, HWND_DESKTOP, &pt, 1);
    MarkMenuOwnerDraw(popup);
    UINT flags = TPM_RETURNCMD | TPM_RIGHTBUTTON;
    int cmd = TrackPopupMenu(popup, flags, pt.x, pt.y, 0, win->hwndFrame, nullptr);
    FreeMenuOwnerDrawInfoData(popup);
    DestroyMenu(popup);

    if (cmd >= CmdSelectionHandlerFirst && cmd < CmdSelectionHandlerLast) {
        HwndSendCommand(win->hwndFrame, cmd);
        return;
    }

    AnnotationType annotType = (AnnotationType)(cmd - CmdCreateAnnotText);
    Annotation* annot = nullptr;
    switch (cmd) {
        case CmdCopySelection:
        case CmdTranslateSelectionWithGoogle:
        case CmdTranslateSelectionWithDeepL:
        case CmdSearchSelectionWithGoogle:
        case CmdSearchSelectionWithBing:
        case CmdSelectAll:
        case CmdSaveAs:
        case CmdPrint:
        case CmdToggleBookmarks:
        case CmdToggleTableOfContents:
        case CmdFavoriteToggle:
        case CmdProperties:
        case CmdToggleToolbar:
        case CmdToggleScrollbars:
        case CmdSaveAnnotations:
        case CmdSaveAnnotationsNewFile:
        case CmdFavoriteAdd:
        case CmdToggleFullscreen:
            // handle in FrameOnCommand() in SumatraPDF.cpp
            HwndSendCommand(win->hwndFrame, cmd);
            break;

            // note: those are duplicated in SumatraPDF.cpp to enable keyboard shortcuts for them
#if 0
        case CmdSelectAnnotation:
            CrashIf(!buildCtx.annotationUnderCursor);
            [[fallthrough]];
#endif

        case CmdEditAnnotations:
            ShowEditAnnotationsWindow(tab);
            SetSelectedAnnotation(tab, buildCtx.annotationUnderCursor);
            break;
        case CmdDeleteAnnotation: {
            DeleteAnnotationAndUpdateUI(tab, buildCtx.annotationUnderCursor);
            break;
        }
        case CmdCopyLinkTarget: {
            TempStr tmp = CleanupURLForClipbardCopyTemp(value);
            CopyTextToClipboard(tmp);
        } break;
        case CmdCopyComment:
            CopyTextToClipboard(value);
            break;

        case CmdCopyImage: {
            if (pageEl) {
                RenderedBitmap* bmp = dm->GetEngine()->GetImageForPageElement(pageEl);
                if (bmp) {
                    CopyImageToClipboard(bmp->GetBitmap(), false);
                }
                delete bmp;
            }
            break;
        }
        case CmdFavoriteDel: {
            DelFavorite(filePath, pageNoUnderCursor);
            break;
        }

        // Note: duplicated in OnWindowContextMenu because slightly different handling
        case CmdCreateAnnotText:
        case CmdCreateAnnotFreeText:
        case CmdCreateAnnotStamp:
        case CmdCreateAnnotCaret:
        case CmdCreateAnnotSquare:
        case CmdCreateAnnotLine:
        case CmdCreateAnnotCircle: {
            annot = EngineMupdfCreateAnnotation(engine, annotType, pageNoUnderCursor, ptOnPage);
            UpdateAnnotationsList(tab->editAnnotsWindow);
            break;
        }
        case CmdCreateAnnotHighlight: {
            annot = MakeAnnotationsFromSelection(tab, AnnotationType::Highlight);
            break;
        }
        case CmdCreateAnnotSquiggly: {
            annot = MakeAnnotationsFromSelection(tab, AnnotationType::Squiggly);
            break;
        }
        case CmdCreateAnnotStrikeOut: {
            annot = MakeAnnotationsFromSelection(tab, AnnotationType::StrikeOut);
            break;
        }
        case CmdCreateAnnotUnderline: {
            annot = MakeAnnotationsFromSelection(tab, AnnotationType::Underline);
            break;
        }
        case CmdCreateAnnotInk:
        case CmdCreateAnnotPolyLine:
            // TODO: implement me
            break;
    }
    if (annot) {
        ShowEditAnnotationsWindow(tab);
        SetSelectedAnnotation(tab, annot);
    }
    /*
        { _TR("Line"), CmdCreateAnnotLine, },
        { _TR_TODON("Highlight"), CmdCreateAnnotHighlight, },
        { _TR_TODON("Underline"), CmdCreateAnnotUnderline, },
        { _TR_TODON("Strike Out"), CmdCreateAnnotStrikeOut, },
        { _TR_TODON("Squiggly"), CmdCreateAnnotSquiggly, },
        { _TR_TODON("File Attachment"), CmdCreateAnnotFileAttachment, },
        { _TR_TODON("Redact"), CmdCreateAnnotRedact, },
    */
    // TODO: those require creating
    /*
        { _TR_TODON("Polygon"), CmdCreateAnnotPolygon, },
        { _TR_TODON("Poly Line"), CmdCreateAnnotPolyLine, },
    */
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
    MENUITEMINFOW mii{};
    mii.cbSize = sizeof(MENUITEMINFOW);

    int n = GetMenuItemCount(hmenu);
    for (int i = 0; i < n; i++) {
        mii.fMask = MIIM_DATA | MIIM_FTYPE | MIIM_SUBMENU;
        BOOL ok = GetMenuItemInfoW(hmenu, (uint)i, TRUE /* by position */, &mii);
        CrashIf(!ok);
        auto modi = (MenuOwnerDrawInfo*)mii.dwItemData;
        if (modi != nullptr) {
            FreeMenuOwnerDrawInfo(modi);
            mii.dwItemData = 0;
            mii.fType &= ~MFT_OWNERDRAW;
            SetMenuItemInfoW(hmenu, (uint)i, TRUE /* by position */, &mii);
        }
        if (mii.hSubMenu != nullptr) {
            FreeMenuOwnerDrawInfoData(mii.hSubMenu);
        }
    };
}
void MarkMenuOwnerDraw(HMENU hmenu) {
    if (!gCurrentTheme->colorizeControls) {
        return;
    }
    WCHAR buf[1024];

    MENUITEMINFOW mii{};
    mii.cbSize = sizeof(MENUITEMINFOW);

    int n = GetMenuItemCount(hmenu);

    for (int i = 0; i < n; i++) {
        buf[0] = 0;
        mii.fMask = MIIM_BITMAP | MIIM_CHECKMARKS | MIIM_DATA | MIIM_FTYPE | MIIM_STATE | MIIM_SUBMENU | MIIM_STRING;
        mii.dwTypeData = &(buf[0]);
        mii.cch = dimof(buf);
        BOOL ok = GetMenuItemInfoW(hmenu, (uint)i, TRUE /* by position */, &mii);
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
        SetMenuItemInfoW(hmenu, (uint)i, TRUE /* by position */, &mii);

        if (mii.hSubMenu != nullptr) {
            MarkMenuOwnerDraw(mii.hSubMenu);
        }
    }
}

constexpr int kMenuPaddingY = 4;
constexpr int kMenuPaddingX = 8;

void MenuCustomDrawMesureItem(HWND hwnd, MEASUREITEMSTRUCT* mis) {
    if (ODT_MENU != mis->CtlType) {
        return;
    }
    auto modi = (MenuOwnerDrawInfo*)mis->itemData;

    bool isSeparator = bit::IsMaskSet(modi->fType, (uint)MFT_SEPARATOR);
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
        size = TextSizeInHwnd(hwnd, "    ", font);
        dx += size.dx;
        size = TextSizeInHwnd(hwnd, mt.shortcutText, font);
        dx += size.dx;
    }
    auto padX = DpiScale(hwnd, kMenuPaddingX);
    auto padY = DpiScale(hwnd, kMenuPaddingY);

    auto cxMenuCheck = GetSystemMetrics(SM_CXMENUCHECK);
    mis->itemHeight += padY * 2;
    mis->itemWidth = uint(dx + DpiScale(hwnd, cxMenuCheck) + (padX * 2));
}

// https://gist.github.com/kjk/1df108aa126b7d8e298a5092550a53b7
// TODO: improve how we paint the menu:
// - position text the right way (not just DT_CENTER)
//   taking into account LTR mode
// - paint shortcut (part after \t if exists) separately
// - paint MFS_DISABLED state
// - paint icons for system menus
// - for submenus, the triangle on the right doesn't draw in the right color
//   I don't know who's drawing it
void MenuCustomDrawItem(HWND hwnd, DRAWITEMSTRUCT* dis) {
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
    // bool isBitmap = bit::IsMaskSet(modi->fType, (uint)MFT_BITMAP);

    // ???
    // bool isMenuBarBreak = bit::IsMaskSet(modi->fType, (uint)MFT_MENUBARBREAK);

    // ???
    // bool isMenuBreak = bit::IsMaskSet(modi->fType, (uint)MFT_MENUBREAK);

    bool isSeparator = bit::IsMaskSet(modi->fType, (uint)MFT_SEPARATOR);

    // default should be drawn in bold
    // bool isDefault = bit::IsMaskSet(modi->fState, (uint)MFS_DEFAULT);

    // disabled should be drawn grayed out
    // bool isDisabled = bit::IsMaskSet(modi->fState, (uint)MFS_DISABLED);

    // don't know what that means
    // bool isHilited = bit::IsMaskSet(modi->fState, (uint)MFS_HILITE);

    // checked/unchecked state for check and radio menus
    bool isChecked = bit::IsMaskSet(modi->fState, (uint)MFS_CHECKED);

    // if isChecked, show as radio button (i.e. circle)
    bool isRadioCheck = bit::IsMaskSet(modi->fType, (uint)MFT_RADIOCHECK);

    auto hdc = dis->hDC;
    HFONT font = GetMenuFont();
    ScopedSelectFont restoreFont(hdc, font);

    COLORREF bgCol = GetMainWindowBackgroundColor();
    COLORREF txtCol = gCurrentTheme->window.textColor;
    // TODO: if isDisabled, pick a color that represents disabled
    // either add it to theme definition or auto-generate
    // (lighter if dark color, darker if light color)

    bool isSelected = bit::IsMaskSet(dis->itemState, (uint)ODS_SELECTED);
    if (isSelected) {
        // TODO: probably better colors
        std::swap(bgCol, txtCol);
    }

    RECT rc = dis->rcItem;
    int rcDy = RectDy(rc);

    int padY = DpiScale(hwnd, kMenuPaddingY);
    int padX = DpiScale(hwnd, kMenuPaddingX);
    int dxCheckMark = DpiScale(hwnd, GetSystemMetrics(SM_CXMENUCHECK));

    COLORREF prevTxtCol = SetTextColor(hdc, txtCol);
    COLORREF prevBgCol = SetBkColor(hdc, bgCol);
    defer {
        SetTextColor(hdc, prevTxtCol);
        SetBkColor(hdc, prevBgCol);
    };

    auto brBg = CreateSolidBrush(bgCol);
    FillRect(hdc, &rc, brBg);
    auto brTxt = CreateSolidBrush(txtCol);

    defer {
        DeleteObject(brBg);
        DeleteObject(brTxt);
    };

    if (isSeparator) {
        CrashIf(modi->text);
        int sx = rc.left + dxCheckMark;
        int y = rc.top + (rcDy / 2);
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

    constexpr int kRadioCircleDx = 6;
    if (isChecked) {
        rc = dis->rcItem;
        // draw radio check indicator (a circle)
        if (isRadioCheck) {
            int dx = DpiScale(hwnd, kRadioCircleDx);
            int offX = DpiScale(hwnd, 1); // why? beause it looks better
            rc.left = rc.left + offX + (dxCheckMark / 2) - (dx / 2);
            rc.right = rc.left + dx;
            rc.top = rc.top + (rcDy / 2) - (dx / 2);
            rc.bottom = rc.top + dx;
            ScopedSelectObject restoreBrush(hdc, brTxt);
            Ellipse(hdc, rc.left, rc.top, rc.right, rc.bottom);
            return;
        }

        // draw a checkmark
        AutoDeletePen pen(CreatePen(PS_SOLID, 2, txtCol));
        ScopedSelectPen restorePen(hdc, pen);
        POINT points[3];
        int offX = DpiScale(hwnd, 6); // 6 is chosen experimentally
        points[0] = {rc.left + offX, rc.top + (rcDy / 2)};
        points[1] = {rc.left + (dxCheckMark / 2), rc.bottom - (padY * 3)};
        points[2] = {rc.left + dxCheckMark - offX, rc.top + (padY * 3)};
        Polyline(hdc, points, dimof(points));
    }
}

HMENU BuildMenu(MainWindow* win) {
    WindowTab* tab = win->CurrentTab();

    BuildMenuCtx buildCtx;
    FillBuildMenuCtx(tab, &buildCtx, Point{0, 0});

    HMENU mainMenu = BuildMenuFromMenuDef(menuDefMenubar, CreateMenu(), &buildCtx);

    MarkMenuOwnerDraw(mainMenu);
    return mainMenu;
}

void UpdateAppMenu(MainWindow* win, HMENU m) {
    CrashIf(!win);
    if (!win) {
        return;
    }
    UINT_PTR id = (UINT_PTR)GetMenuItemID(m, 0);
    if (id == menuDefFile[0].idOrSubmenu) {
        RebuildFileMenu(win->CurrentTab(), m);
    } else if (id == menuDefFavorites[0].idOrSubmenu) {
        MenuEmpty(m);
        BuildMenuFromMenuDef(menuDefFavorites, m, nullptr);
        RebuildFavMenu(win, m);
    }
    MenuUpdateStateForWindow(win);
    MarkMenuOwnerDraw(win->menu);
}

// show/hide top-level menu bar. This doesn't persist across launches
// so that accidental removal of the menu isn't catastrophic
void ToggleMenuBar(MainWindow* win, bool showTemporarily) {
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
