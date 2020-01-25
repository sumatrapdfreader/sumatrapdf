/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"

#include "resource.h"
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

#if defined(DEBUG) || defined(PRE_RELEASE_VER)
bool gWithTocEditor = true;
#else
bool gWithTocEditor = false;
#endif

#if defined(RAMICRO)
bool gIsRaMicroBuild = true;
#else
bool gIsRaMicroBuild = false;
#endif

const WCHAR* getAppName() {
    if (gIsRaMicroBuild) {
        return L"RA-MICRO PDF Viewer";
    }
    return L"SumatraPDF";
}

const WCHAR* getExeName() {
    if (gIsRaMicroBuild) {
        return L"RA-MICRO PDF Viewer.exe";
    }
    return L"SumatraPDF.exe";
}

int getAppIconID() {
    if (gIsRaMicroBuild) {
        return IDI_RAMICRO;
    }
    return IDI_SUMATRAPDF;
}