/* Copyright Krzysztof Kowalczyk 2006-2007
   License: GPLv2 */
#ifndef APP_PREFS_H_
#define APP_PREFS_H_

/* Most of the global settings that we persist in preferences file. */
typedef struct SerializableGlobalPrefs {
    BOOL m_showToolbar;
    BOOL m_useFitz;
    /* If false, we won't ask the user if he wants Sumatra to handle PDF files */
    BOOL m_pdfAssociateDontAskAgain;
    /* If m_pdfAssociateDontAskAgain is TRUE, says whether we should 
       silently associate or not */
    BOOL m_pdfAssociateShouldAssociate;
} SerializableGlobalPrefs;

extern SerializableGlobalPrefs gGlobalPrefs;

#if 0
bool        Prefs_Serialize(FileHistoryList **root, DString *strOut);
#endif
struct FileHistoryList;
bool        Prefs_Deserialize(const char *prefsTxt, FileHistoryList **fileHistoryRoot);

const char *Prefs_Serialize2(FileHistoryList **root, size_t* lenOut);
bool        Prefs_Deserialize2(const char *prefsTxt, size_t prefsTxtLen, FileHistoryList **fileHistoryRoot);

#endif

