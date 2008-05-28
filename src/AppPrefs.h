/* Copyright Krzysztof Kowalczyk 2006-2007
   License: GPLv2 */
#ifndef APP_PREFS_H_
#define APP_PREFS_H_

#include "DisplayState.h"

#define DEFAULT_WIN_POS (int)-1

/* enum from m_windowState */
enum {
    WIN_STATE_NORMAL = 1, /* use remebered position and size */
    WIN_STATE_MAXIMIZED /* ignore position and size, maximize the window */
};

/* Most of the global settings that we persist in preferences file. */
typedef struct {
    BOOL m_showToolbar;
    BOOL m_useFitz;
    /* If false, we won't ask the user if he wants Sumatra to handle PDF files */
    BOOL m_pdfAssociateDontAskAgain;
    /* If m_pdfAssociateDontAskAgain is TRUE, says whether we should 
       silently associate or not */
    BOOL m_pdfAssociateShouldAssociate;

    int  m_bgColor;
    BOOL m_escToExit;
    BOOL m_fullScreen;

    /* pattern used to launch the editor when doing inverse search */
    char *m_inverseSearchCmdLine;

    /* When we show 'new version available', user has an option to check
       'skip this version'. This remembers which version is to be skipped.
       If NULL - don't skip */
    char *m_versionToSkip;

    char *m_guid;
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
    int  m_pdfsOpened;
} SerializableGlobalPrefs;

extern SerializableGlobalPrefs gGlobalPrefs;

struct FileHistoryList;
bool        Prefs_DeserializeOld(const char *prefsTxt, FileHistoryList **fileHistoryRoot);

const char *Prefs_Serialize(FileHistoryList **root, size_t* lenOut);
bool        Prefs_Deserialize(const char *prefsTxt, size_t prefsTxtLen, FileHistoryList **fileHistoryRoot);

#endif

