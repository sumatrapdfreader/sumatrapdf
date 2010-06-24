/* Copyright Krzysztof Kowalczyk 2006-2009
   License: GPLv3 */
#ifndef DISPLAY_STATE_H_
#define DISPLAY_STATE_H_

#include "base_util.h"

enum DisplayMode {
    DM_FIRST = 0,
    // automatic means: the continuous form of single page, facing or
    // book view - depending on the document's desired PageLayout
    DM_AUTOMATIC = DM_FIRST,
    DM_SINGLE_PAGE,
    DM_FACING,
    DM_BOOK_VIEW,
    DM_CONTINUOUS,
    DM_CONTINUOUS_FACING,
    DM_CONTINUOUS_BOOK_VIEW
};

#define ZOOM_FIT_PAGE       -1
#define ZOOM_FIT_WIDTH      -2
#define ZOOM_ACTUAL_SIZE    100.0
#define ZOOM_MAX            6400.5  /* max zoom in % */
#define ZOOM_MIN            8.0    /* min zoom in % */

#define DM_AUTOMATIC_STR            "automatic"
#define DM_SINGLE_PAGE_STR          "single page"
#define DM_FACING_STR               "facing"
#define DM_BOOK_VIEW_STR            "book view"
#define DM_CONTINUOUS_STR           "continuous"
#define DM_CONTINUOUS_FACING_STR    "continuous facing"
#define DM_CONTINUOUS_BOOK_VIEW_STR "continuous book view"

#define FILE_HISTORY_STR            "File History"

#define FILE_STR                    "File"
#define DISPLAY_MODE_STR            "Display Mode"
#define PAGE_NO_STR                 "Page"
#define ZOOM_VIRTUAL_STR            "ZoomVirtual"
#define ROTATION_STR                "Rotation"
#define SCROLL_X_STR                "Scroll X2"
#define SCROLL_Y_STR                "Scroll Y2"
#define WINDOW_STATE_STR            "Window State"
#define WINDOW_X_STR                "Window X"
#define WINDOW_Y_STR                "Window Y"
#define WINDOW_DX_STR               "Window DX"
#define WINDOW_DY_STR               "Window DY"
#define SHOW_TOOLBAR_STR            "ShowToolbar"
#define PDF_ASSOCIATE_DONT_ASK_STR  "PdfAssociateDontAskAgain"
#define PDF_ASSOCIATE_ASSOCIATE_STR "PdfAssociateShouldAssociate"
#define UI_LANGUAGE_STR             "UILanguage"
#define SHOW_TOC_STR                "ShowToc"
#define BG_COLOR_STR                "BgColor"
#define ESC_TO_EXIT_STR             "EscToExit"
#define INVERSE_SEARCH_COMMANDLINE  "InverseSearchCommandLine"
#define VERSION_TO_SKIP_STR         "VersionToSkip"
#define LAST_UPDATE_STR             "LastUpdate"
#define ENABLE_AUTO_UPDATE_STR      "EnableAutoUpdate"
#define REMEMBER_OPENED_FILES_STR   "RememberOpenedFiles"
#define PRINT_COMMANDLINE           "PrintCommandLine"
#define GLOBAL_PREFS_ONLY_STR       "GlobalPrefsOnly"
#define USE_GLOBAL_VALUES_STR       "UseGlobalValues"

#define FWDSEARCH_OFFSET            "ForwardSearch_HighlightOffset"
#define FWDSEARCH_COLOR             "ForwardSearch_HighlightColor"
#define FWDSEARCH_WIDTH             "ForwardSearch_HighlightWidth"

typedef struct DisplayState {
    const TCHAR *       filePath;
    enum DisplayMode    displayMode;
    int                 scrollX;
    int                 scrollY;
    int                 pageNo;
    double              zoomVirtual;
    int                 rotation;
    int                 windowState;
    int                 windowX;
    int                 windowY;
    int                 windowDx;
    int                 windowDy;
    BOOL                showToc;
    BOOL                useGlobalValues;
} DisplayState;

void    normalizeRotation(int *rotation);
BOOL    validRotation(int rotation);
BOOL    ValidZoomVirtual(double zoomVirtual);

const char *      DisplayModeNameFromEnum(DisplayMode var);
bool              DisplayModeEnumFromName(const char *txt, DisplayMode *resOut);

void    DisplayState_Init(DisplayState *ds);
void    DisplayState_Free(DisplayState *ds);
#if 0
bool    DisplayState_Serialize(DisplayState *ds, DString *strOut);
#endif

#endif

