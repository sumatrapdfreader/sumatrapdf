/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef AppPrefs_h
#define AppPrefs_h

#include "DisplayState.h"

/* enum from m_windowState */
enum {
    WIN_STATE_NORMAL = 1, /* use remebered position and size */
    WIN_STATE_MAXIMIZED,  /* ignore position and size, maximize the window */    
    WIN_STATE_FULLSCREEN,
    WIN_STATE_MINIMIZED,
};

/* Most of the global settings that we persist in preferences file. */
struct SerializableGlobalPrefs {
    ~SerializableGlobalPrefs() {
        free(m_versionToSkip);
        free(m_inverseSearchCmdLine);
        free(m_lastUpdateTime);
    }

    bool m_globalPrefsOnly;
    /* pointer to a static string returned by Trans::ConfirmLanguage, don't free */
    const char *m_currentLanguage;

    bool m_showToolbar;
    /* If false, we won't ask the user if he wants Sumatra to handle PDF files */
    bool m_pdfAssociateDontAskAgain;
    /* If m_pdfAssociateDontAskAgain is TRUE, says whether we should 
       silently associate or not */
    bool m_pdfAssociateShouldAssociate;

    bool m_enableAutoUpdate;

    /* if true, we remember which files we opened and their settings */
    bool m_rememberOpenedFiles;

    /* used for the Start page, About page and Properties dialog
       (negative values indicate that the default color will be used) */
    int  m_bgColor;
    bool m_escToExit;

    /* pattern used to launch the editor when doing inverse search */
    TCHAR *m_inverseSearchCmdLine;
    /* whether to expose the SyncTeX enhancements to the user */
    bool m_enableTeXEnhancements;

    /* When we show 'new version available', user has an option to check
       'skip this version'. This remembers which version is to be skipped.
       If NULL - don't skip */
    TCHAR *m_versionToSkip;

    char *m_lastUpdateTime;

    DisplayMode m_defaultDisplayMode;
    float m_defaultZoom;
    /* Default state of Sumatra window */
    int   m_windowState;
    /* Default position (can be on any monitor) */
    RectI m_windowPos;

    bool m_showToc;
    int  m_tocDx;

    /* Forward search highlighting settings  */
    int  m_fwdsearchOffset;    /* if <=0 then use the standard (inline) highlighting style, otherwise use the margin highlight (i.e., coloured block on the left side of the page) */
    int  m_fwdsearchColor;     /* highlight color of the forward-search for both the standard and margin style*/
    int  m_fwdsearchWidth;     /* width of the coloured blocks for the margin style */
    bool m_fwdsearchPermanent; /* if false then highlights are hidden automatically after a short period of time,
                                  if true then highlights remain until the next forward search */

    bool m_showStartPage; /* whether to display Frequently Read documents or the About page in an empty window */
    int  m_openCountWeek; /* week count since 2011-01-01 needed to "age" openCount values in file history */

    FILETIME m_lastPrefUpdate; /* modification time of the preferences file when it was last read */
};

#define GLOBAL_PREFS_STR            "gp"
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
#define SHOW_RECENT_FILES_STR       "ShowStartPage"
#define OPEN_COUNT_STR              "OpenCount"
#define IS_PINNED_STR               "Pinned"
#define OPEN_COUNT_WEEK_STR         "OpenCountWeek"
#define FWDSEARCH_OFFSET            "ForwardSearch_HighlightOffset"
#define FWDSEARCH_COLOR             "ForwardSearch_HighlightColor"
#define FWDSEARCH_WIDTH             "ForwardSearch_HighlightWidth"
#define FWDSEARCH_PERMANENT         "ForwardSearch_HighlightPermanent"

#define DM_AUTOMATIC_STR            "automatic"
#define DM_SINGLE_PAGE_STR          "single page"
#define DM_FACING_STR               "facing"
#define DM_BOOK_VIEW_STR            "book view"
#define DM_CONTINUOUS_STR           "continuous"
#define DM_CONTINUOUS_FACING_STR    "continuous facing"
#define DM_CONTINUOUS_BOOK_VIEW_STR "continuous book view"

class FileHistory;

namespace Prefs {

bool    Load(TCHAR *filepath, SerializableGlobalPrefs& globalPrefs, FileHistory& fileHistory);
bool    Save(TCHAR *filepath, SerializableGlobalPrefs& globalPrefs, FileHistory& fileHistory);

}

namespace DisplayModeConv {

const TCHAR *   NameFromEnum(DisplayMode var);
bool            EnumFromName(const TCHAR *txt, DisplayMode *resOut);

}

#endif
