/* Copyright Krzysztof Kowalczyk 2006-2009
   License: GPLv3 */
#ifndef APP_PREFS_H_
#define APP_PREFS_H_

#include "DisplayState.h"

#define DEFAULT_WIN_POS (int)-1

// TODO: Move this somewhere more appropriate?
#define MAX_RECENT_FILES_IN_MENU 10

/* enum from m_windowState */
enum {
    WIN_STATE_NORMAL = 1 /* use remebered position and size */
    ,WIN_STATE_MAXIMIZED /* ignore position and size, maximize the window */    
    ,WIN_STATE_FULLSCREEN
    ,WIN_STATE_MINIMIZED
};

/* Most of the global settings that we persist in preferences file. */
typedef struct {
    BOOL m_showToolbar;
    /* If false, we won't ask the user if he wants Sumatra to handle PDF files */
    BOOL m_pdfAssociateDontAskAgain;
    /* If m_pdfAssociateDontAskAgain is TRUE, says whether we should 
       silently associate or not */
    BOOL m_pdfAssociateShouldAssociate;

    BOOL m_enableAutoUpdate;

    /* if true, we remember which files we opened and their settings */
    BOOL m_rememberOpenedFiles;

    int  m_bgColor;
    BOOL m_escToExit;

    /* pattern used to launch the editor when doing inverse search */
    TCHAR *m_inverseSearchCmdLine;

    /* When we show 'new version available', user has an option to check
       'skip this version'. This remembers which version is to be skipped.
       If NULL - don't skip */
    TCHAR *m_versionToSkip;

    char *m_lastUpdateTime;

    /* Default state of Sumatra window */
    /* TODO: I would also like to remember a monitor, but that seems a bit complicated */
    DisplayMode m_defaultDisplayMode;

    double m_defaultZoom;
    int  m_windowState;
    int  m_windowPosX;
    int  m_windowPosY;
    int  m_windowDx;
    int  m_windowDy;

    int  m_showToc;
    int  m_globalPrefsOnly;
    /* Forward search highlighting settings  */
    int  m_fwdsearchOffset; /* if <=0 then use the standard (inline) highlighting style, otherwise use the margin highlight (i.e., coloured block on the left side of the page) */
    int  m_fwdsearchColor;  /* highlight color of the forward-search for both the standard and margin style*/
    int  m_fwdsearchWidth;  /* width of the coloured blocks for the margin style */

    BOOL m_invertColors; /* invert all colors for accessibility reasons (experimental!) */
} SerializableGlobalPrefs;

extern SerializableGlobalPrefs gGlobalPrefs;

struct FileHistoryList;

const char *Prefs_Serialize(FileHistoryList **root, size_t* lenOut);
bool        Prefs_Deserialize(const char *prefsTxt, size_t prefsTxtLen, FileHistoryList **fileHistoryRoot);

#endif

