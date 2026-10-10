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

// Day this file was compiled, e.g. "2026-10-03" for a __DATE__ of "Oct  3 2026".
TempStr BuiltOnDate() {
    const int kMonthLen = 3;
    const int kDayPos = 4;
    const int kYearPos = 7;
    const int kYearLen = 4;
    const char* months = "JanFebMarAprMayJunJulAugSepOctNovDec";
    const char* d = __DATE__;

    int month = 1;
    while (month < 12 && memcmp(d, months + (month - 1) * kMonthLen, kMonthLen) != 0) {
        month++;
    }

    // day is space-padded
    int tens = d[kDayPos] == ' ' ? 0 : d[kDayPos] - '0';
    int day = tens * 10 + (d[kDayPos + 1] - '0');

    return fmt("%s-%02d-%02d", Str(d + kYearPos, kYearLen), month, day);
}

Str currentVersion = Str(CURR_VERSION_STRA);

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
