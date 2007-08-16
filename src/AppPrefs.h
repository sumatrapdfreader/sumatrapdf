/* Copyright Krzysztof Kowalczyk 2006-2007
   License: GPLv2 */
#ifndef APP_PREFS_H_
#define APP_PREFS_H_

#include "DisplayState.h"
#include "FileHistory.h"

bool        Prefs_Serialize(FileHistoryList **root, DString *strOut);
bool        Prefs_Deserialize(const char *prefsTxt, FileHistoryList **fileHistoryRoot);

const char *Prefs_Serialize2(FileHistoryList **root, size_t* lenOut);
bool        Prefs_Deserialize2(const char *prefsTxt, size_t prefsTxtLen, FileHistoryList **fileHistoryRoot);

#endif

