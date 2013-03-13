/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef AppPrefs_h
#define AppPrefs_h

#include "DisplayState.h"

bool ParseViewMode(DisplayMode *mode, const WCHAR *txt);

/* enum from windowState */
enum {
    WIN_STATE_NORMAL = 1, /* use remembered position and size */
    WIN_STATE_MAXIMIZED,  /* ignore position and size, maximize the window */
    WIN_STATE_FULLSCREEN,
    WIN_STATE_MINIMIZED,
};

/* Most of the global settings that we persist in preferences file. */
struct SerializableGlobalPrefs {
    ~SerializableGlobalPrefs() {
        free(versionToSkip);
        free(inverseSearchCmdLine);
        free(lastUpdateTime);
    }

    bool globalPrefsOnly;
    /* pointer to a static string that is part of LangDef, don't free */
    const char *currLangCode;

    bool toolbarVisible;
    bool favVisible;

    /* If false, we won't ask the user if he wants Sumatra to handle PDF files */
    bool pdfAssociateDontAskAgain;
    /* If pdfAssociateDontAskAgain is TRUE, says whether we should
       silently associate or not */
    bool pdfAssociateShouldAssociate;

    bool enableAutoUpdate;

    /* if true, we remember which files we opened and their settings */
    bool rememberOpenedFiles;

    /* used for the Start page, About page and Properties dialog
       (negative values indicate that the default color will be used) */
    int  bgColor;
    bool escToExit;
    /* whether to display documents black-on-white or in system colors */
    bool useSysColors;

    /* pattern used to launch the editor when doing inverse search */
    WCHAR *inverseSearchCmdLine;
    /* whether to expose the SyncTeX enhancements to the user */
    bool enableTeXEnhancements;

    /* When we show 'new version available', user has an option to check
       'skip this version'. This remembers which version is to be skipped.
       If NULL - don't skip */
    WCHAR *versionToSkip;

    char *lastUpdateTime;

    DisplayMode defaultDisplayMode;
    float defaultZoom;
    /* Default state of Sumatra window */
    int   windowState;
    /* Default position (can be on any monitor) */
    RectI windowPos;

    bool tocVisible;
    // if sidebar (favorites and/or bookmarks) is visible, this is
    // the width of the left sidebar panel containing them
    int  sidebarDx;
    // if both favorites and bookmarks parts of sidebar are
    // visible, this is the height of bookmarks (table of contents) part
    int  tocDy;

    /* Forward search highlighting settings  */
    struct {
        int  offset;    /* if <=0 then use the standard (inline) highlighting style, otherwise use the
                           margin highlight (i.e. coloured block on the left side of the page) */
        int  color;     /* highlight color of the forward-search for both the standard and margin style */
        int  width;     /* width of the coloured blocks for the margin style */
        bool permanent; /* if false then highlights are hidden automatically after a short period of time,
                           if true then highlights remain until the next forward search */
    } fwdSearch;

    bool showStartPage; /* whether to display Frequently Read documents or the About page in an empty window */
    int  openCountWeek; /* week count since 2011-01-01 needed to "age" openCount values in file history */

    FILETIME lastPrefUpdate; /* modification time of the preferences file when it was last read */

    bool cbxR2L; /* display CBX double pages from right to left */
};

extern SerializableGlobalPrefs gGlobalPrefs;

namespace DisplayModeConv {

const WCHAR *   NameFromEnum(DisplayMode var);
bool            EnumFromName(const WCHAR *txt, DisplayMode *resOut);

}

bool LoadPrefs();
bool SavePrefs();
bool ReloadPrefs();

#endif
