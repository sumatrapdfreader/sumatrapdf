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

// experimental, unfinished theme support for menus by making them owner-drawn
#if defined(EXP_MENU_OWNER_DRAW)
bool gOwnerDrawMenu = true;
#else
bool gOwnerDrawMenu = false;
#endif

#ifdef DISABLE_DOCUMENT_RESTRICTIONS
bool gDisableDocumentRestrictions = true;
#else
bool gDisableDocumentRestrictions = false;
#endif

const WCHAR* GetAppNameTemp() {
    return L"SumatraPDF";
}

const WCHAR* GetExeNameTemp() {
    return L"SumatraPDF.exe";
}

int GetAppIconID() {
    return IDI_SUMATRAPDF;
}
