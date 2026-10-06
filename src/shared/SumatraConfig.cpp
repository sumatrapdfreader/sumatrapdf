/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"

#include "resource.h"
#include "Version.h"
#include "SumatraConfig.h"

bool gIsDebugBuild = IS_DEBUG;
bool gIsAsanBuild = IS_ASAN;

#ifdef PRE_RELEASE_VER
bool gIsPreReleaseBuild = true;
#else
bool gIsPreReleaseBuild = false;
#endif

#ifdef BUILT_ON
Str gBuiltOn = Str(QM(BUILT_ON));
#else
Str gBuiltOn;
#endif

Str currentVersion = Str(CURR_VERSION_STRA);

#ifdef PRE_RELEASE_VER
Str preReleaseVersion = Str(QM(PRE_RELEASE_VER));
#else
Str preReleaseVersion;
#endif

#ifdef GIT_COMMIT_ID
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
