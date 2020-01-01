/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"

#include "Version.h"
#include "SumatraConfig.h"

#if defined(DEBUG)
bool isDebugBuild = true;
#else
bool isDebugBuild = false;
#endif

#if defined(ASAN_BUILD)
bool isAsanBuild = true;
#else
bool isAsanBuild = false;
#endif

// those are set in BuildConfig.h by build.go
#if defined(IS_DAILY_BUILD)
bool isDailyBuild = true;
#else
bool isDailyBuild = false;
#endif

#if defined(SVN_PRE_RELEASE_VER)
bool isPreReleaseBuild = true;
#else
bool isPreReleaseBuild = false;
#endif

#if defined(BUILT_ON)
const char* builtOn = QM(BUILT_ON);
#else
const char* builtOn = nullptr;
#endif

const char* currentVersion = CURR_VERSION_STRA;

#if defined(SVN_PRE_RELEASE_VER)
const char* preReleaseVersion = QM(SVN_PRE_RELEASE_VER);
#else
const char* preReleaseVersion = nullptr;
#endif

#if defined(GIT_COMMIT_ID)
const char* gitSha1 = QM(GIT_COMMIT_ID);
#else
const char* gitSha1 = nullptr;
#endif
