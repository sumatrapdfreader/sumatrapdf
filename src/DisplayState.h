/* Copyright Krzysztof Kowalczyk 2006-2009
   License: GPLv2 */
#ifndef DISPLAY_STATE_H_
#define DISPLAY_STATE_H_

#include "base_util.h"

enum DisplayMode {
    DM_FIRST = 1,
    DM_SINGLE_PAGE = DM_FIRST,
    DM_FACING,
    DM_CONTINUOUS,
    DM_CONTINUOUS_FACING,
    DM_LAST = DM_CONTINUOUS_FACING
};

#define ZOOM_FIT_PAGE       -1
#define ZOOM_FIT_WIDTH      -2
#define ZOOM_MAX            6401.0  /* max zoom in % */
#define ZOOM_MIN            8.0    /* min zoom in % */

#define DM_SINGLE_PAGE_STR          "single page"
#define DM_FACING_STR               "facing"
#define DM_CONTINUOUS_STR           "continuous"
#define DM_CONTINUOUS_FACING_STR    "continuous facing"

#define FILE_HISTORY_STR            "File History"

#define FILE_STR                    "File"
#define DISPLAY_MODE_STR            "Display Mode"
#define VISIBLE_STR                 "Visible"
#define PAGE_NO_STR                 "Page"
#define ZOOM_VIRTUAL_STR            "ZoomVirtual"
#define ROTATION_STR                "Rotation"
#define SCROLL_X_STR                "Scroll X"
#define SCROLL_Y_STR                "Scroll Y"
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
#define GLOBAL_PREFS_ONLY_STR       "GlobalPrefsOnly"
#define USE_GLOBAL_VALUES_STR       "UseGlobalValues"

typedef struct DisplayState {
    const TCHAR *       filePath;
    enum DisplayMode    displayMode;
    BOOL                visible;     /* if TRUE, currently shown on the screen */
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

