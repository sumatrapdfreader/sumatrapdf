/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef AppPrefs_h
#define AppPrefs_h

#include "DisplayState.h"

// number of most recently used files that will be shown in the menu
// (and remembered in the preferences file, if just filenames are
//  to be remembered and not individual view settings per document)
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
    bool m_globalPrefsOnly;
    /* pointer to a static string returned by Trans::GetLanguageCode, don't free */
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

    bool m_invertColors; /* invert all colors for accessibility reasons (experimental!) */

    FILETIME m_lastPrefUpdate; /* modification time of the preferences file when it was last read */
} SerializableGlobalPrefs;

class FileHistory;

namespace Prefs {

bool    Load(TCHAR *filepath, SerializableGlobalPrefs *globalPrefs, FileHistory *fileHistory);
bool    Save(TCHAR *filepath, SerializableGlobalPrefs *globalPrefs, FileHistory *fileHistory);

}

#endif
