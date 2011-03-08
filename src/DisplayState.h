/* Copyright Krzysztof Kowalczyk 2006-2011
   License: GPLv3 */
#ifndef DISPLAY_STATE_H_
#define DISPLAY_STATE_H_

#include "BaseUtil.h"

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
#define ZOOM_FIT_CONTENT    -3
#define ZOOM_ACTUAL_SIZE    100.0f
#define ZOOM_MAX            6400.1f /* max zoom in % */
#define ZOOM_MIN            8.0f    /* min zoom in % */
#define INVALID_ZOOM        -99.0f

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
#define TOC_DX_STR                  "Toc DX"
#define TOC_STATE_STR               "TocToggles"
#define BG_COLOR_STR                "BgColor"
#define ESC_TO_EXIT_STR             "EscToExit"
#define INVERSE_SEARCH_COMMANDLINE  "InverseSearchCommandLine"
#define ENABLE_TEX_ENHANCEMENTS_STR "ExposeInverseSearch"
#define VERSION_TO_SKIP_STR         "VersionToSkip"
#define LAST_UPDATE_STR             "LastUpdate"
#define ENABLE_AUTO_UPDATE_STR      "EnableAutoUpdate"
#define REMEMBER_OPENED_FILES_STR   "RememberOpenedFiles"
#define PRINT_COMMANDLINE           "PrintCommandLine"
#define GLOBAL_PREFS_ONLY_STR       "GlobalPrefsOnly"
#define USE_GLOBAL_VALUES_STR       "UseGlobalValues"
#define DECRYPTION_KEY_STR          "Decryption Key"

#define FWDSEARCH_OFFSET            "ForwardSearch_HighlightOffset"
#define FWDSEARCH_COLOR             "ForwardSearch_HighlightColor"
#define FWDSEARCH_WIDTH             "ForwardSearch_HighlightWidth"
#define FWDSEARCH_PERMANENT         "ForwardSearch_HighlightPermanent"


class DisplayState {
public:
    DisplayState() :
        filePath(NULL), decryptionKey(NULL), useGlobalValues(FALSE),
        displayMode(DM_AUTOMATIC), scrollX(0), scrollY(0), pageNo(1),
        zoomVirtual(100.0), rotation(0), windowState(0), windowX(0),
        windowY(0), windowDx(0), windowDy(0), showToc(TRUE), tocDx(0),
        tocState(NULL) { }

    ~DisplayState() {
        free((void *)filePath);
        free((void *)decryptionKey);
        free(tocState);
    }

    const TCHAR *       filePath;
    const char *        decryptionKey; // hex encoded MD5 fingerprint of file content (32 chars) followed by crypt key (64 chars)
    BOOL                useGlobalValues;

    enum DisplayMode    displayMode;
    int                 scrollX;
    int                 scrollY;
    int                 pageNo;
    float               zoomVirtual;
    int                 rotation;
    int                 windowState;
    int                 windowX;
    int                 windowY;
    int                 windowDx;
    int                 windowDy;
    BOOL                showToc;
    int                 tocDx;
    int *               tocState;
};

void    normalizeRotation(int *rotation);
BOOL    validRotation(int rotation);
BOOL    ValidZoomVirtual(float zoomVirtual);

const char *      DisplayModeNameFromEnum(DisplayMode var);
bool              DisplayModeEnumFromName(const char *txt, DisplayMode *resOut);

#endif
