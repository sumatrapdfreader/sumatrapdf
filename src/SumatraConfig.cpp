/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"

#include "resource.h"
#include "Version.h"

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
Str builtOn = Str(QM(BUILT_ON));
#else
Str builtOn;
#endif

Str currentVersion = Str(CURR_VERSION_STRA);

#if defined(PRE_RELEASE_VER)
Str preReleaseVersion = Str(QM(PRE_RELEASE_VER));
#else
Str preReleaseVersion;
#endif

#if defined(GIT_COMMIT_ID)
Str gitCommidId = Str(QM(GIT_COMMIT_ID));
#else
Str gitCommidId;
#endif

#ifdef DISABLE_DOCUMENT_RESTRICTIONS
bool gDisableDocumentRestrictions = true;
#else
bool gDisableDocumentRestrictions = false;
#endif

bool gIsStoreBuild = false;

// set by -for-testing cmd-line flag, used for ad-hoc testing by humans
// or agents. Always starts a new instance, doesn't restore a session and
// doesn't save settings
bool gForTesting = false;

int GetAppIconID() {
    return IDI_SUMATRAPDF;
}
