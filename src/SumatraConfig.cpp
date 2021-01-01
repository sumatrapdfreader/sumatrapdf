/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"

#include "resource.h"
#include "Commands.h"
#include "Version.h"
#include "SumatraConfig.h"

#if defined(DEBUG)
bool gIsDebugBuild = true;
#else
bool gIsDebugBuild = false;
#endif

#if defined(ASAN_BUILD)
bool gIsAsanBuild = true;
#else
bool gIsAsanBuild = false;
#endif

// those are set in BuildConfig.h by build.go
#if defined(IS_DAILY_BUILD)
bool gIsDailyBuild = true;
#else
bool gIsDailyBuild = false;
#endif

#if defined(PRE_RELEASE_VER)
bool gIsPreReleaseBuild = true;
#else
bool gIsPreReleaseBuild = false;
#endif

#if defined(BUILT_ON)
const char* builtOn = QM(BUILT_ON);
#else
const char* builtOn = nullptr;
#endif

const char* currentVersion = CURR_VERSION_STRA;

#if defined(PRE_RELEASE_VER)
const char* preReleaseVersion = QM(PRE_RELEASE_VER);
#else
const char* preReleaseVersion = nullptr;
#endif

#if defined(GIT_COMMIT_ID)
const char* gitSha1 = QM(GIT_COMMIT_ID);
#else
const char* gitSha1 = nullptr;
#endif

#if defined(DEBUG) || defined(PRE_RELEASE_VER) || defined(RAMICRO)
bool gWithTocEditor = true;
#else
bool gWithTocEditor = false;
#endif

#if defined(RAMICRO)
bool gIsRaMicroBuild = true;
#else
bool gIsRaMicroBuild = false;
#endif

// experimental, unfinished theme support for menus by making them owner-drawn
#if defined(EXP_MENU_OWNER_DRAW)
bool gOwnerDrawMenu = true;
#else
bool gOwnerDrawMenu = false;
#endif

#if defined(DEBUG) || defined(PRE_RELEASE_VER)
bool gShowDebugMenu = true;
#else
bool gShowDebugMenu = false;
#endif

#ifdef DISABLE_DOCUMENT_RESTRICTIONS
bool gDisableDocumentRestrictions = true;
#else
bool gDisableDocumentRestrictions = false;
#endif

const WCHAR* GetAppName() {
    if (gIsRaMicroBuild) {
        return L"RA-MICRO PDF Viewer";
    }
    return L"SumatraPDF";
}

const WCHAR* GetExeName() {
    if (gIsRaMicroBuild) {
        return L"RA-MICRO PDF Viewer.exe";
    }
    return L"SumatraPDF.exe";
}

int GetAppIconID() {
    if (gIsRaMicroBuild) {
        return IDI_RAMICRO;
    }
    return IDI_SUMATRAPDF;
}
