/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "Translations.h"
#include "PdfSync.h"

struct MainWindow;

#include "SumatraControlCommon.h"
#include "UpdateCheck.h"
#include "UpdateCheckCommon.h"

// Same layout as SearchAndDDECommon.h. That header needs the find-UI types.
struct OpenDdeArgs {
    bool parsed = false;
    TempStr filePath;
    int newWindow = 0;
    int setFocus = 0;
    int forceRefresh = 0;
    int inCurrentTab = 0;
    Str next;
};

// Wait-idle commands run on the control thread. Everything else, including
// CrashMe, goes to the UI thread. Quit is reported; the app decides whether
// a failed write still stops the listener.
ControlFrameAction ControlFrameActionFor(u16 cmd) {
    if (cmd == (u16)ControlFrameCmd::WaitRenderIdle) {
        return ControlFrameAction::WaitRenderIdle;
    }
    if (cmd == (u16)ControlFrameCmd::WaitSessionRestored) {
        return ControlFrameAction::WaitSessionRestored;
    }
    return ControlFrameAction::RunOnUi;
}

bool ControlCmdEndsListener(u16 cmd) {
    return cmd == (u16)ControlFrameCmd::Quit;
}

// [Open("path")] or [Open("path",new,focus,refresh[,inTab])].
OpenDdeArgs ParseOpenDdeCmd(Str cmd) {
    OpenDdeArgs a;
    a.next = str::Parse(cmd, "[Open(\"%s\")]", &a.filePath);
    if (str::IsNull(a.next)) {
        a.next = str::Parse(cmd, "[Open(\"%s\",%u,%u,%u,%u)]", &a.filePath, &a.newWindow, &a.setFocus, &a.forceRefresh,
                            &a.inCurrentTab);
    }
    if (str::IsNull(a.next)) {
        a.next = str::Parse(cmd, "[Open(\"%s\",%u,%u,%u)]", &a.filePath, &a.newWindow, &a.setFocus, &a.forceRefresh);
    }
    a.parsed = !str::IsNull(a.next);
    return a;
}

// newWindow replaces the current tab only when it is 0.
int ApplyOpenNewWindow(int newWindow, int inCurrentTab) {
    if (newWindow != 0 && inCurrentTab != 0) {
        logf("HandleOpenCmd: setting inCurrentTab to 0 because newWindow != 0\n");
        return 0;
    }
    return inCurrentTab;
}

// Short terms (1-2 chars) match a lot, so wait longer (issue #4626).
int FindDebounceDelayMs(int nChars) {
    constexpr int kFindDebounceDelayMs = 500;
    constexpr int kFindDebounceShortDelayMs = 1000;
    return nChars <= 2 ? kFindDebounceShortDelayMs : kFindDebounceDelayMs;
}

// Text shown for a failed forward search. Codes that never notified stay empty.
TempStr ForwardSearchFailTextTemp(int ret, Str fileName, int line, int page) {
    if (ret == PDFSYNCERR_INVALID_PAGE_NUMBER) {
        return fmt(Tr("Page %u does not exist").s, page);
    }
    if (ret == PDFSYNCERR_UNKNOWN_SOURCEFILE) {
        return fmt(Tr("Unknown source file (%s)").s, fileName);
    }
    if (ret == PDFSYNCERR_NORECORD_IN_SOURCEFILE) {
        return fmt(Tr("Source file %s has no synchronization point").s, fileName);
    }
    if (ret == PDFSYNCERR_NORECORD_FOR_THATLINE || ret == PDFSYNCERR_NOSYNCPOINT_FOR_LINERECORD) {
        return fmt(Tr("No result found around line %u in file %s").s, line, fileName);
    }
    return {};
}

// Automatic checks wait out the first start, then a day. Pre-release builds
// check on every startup. forcePreReleaseOnly is orig's FORCE_AUTO_UPDATE.
UpdateCheckChoice DecideUpdateCheck(const UpdateCheckFacts& f) {
    if (f.inProgress) {
        logf("CheckForUpdate: skipping because gUpdateCheckInProgress\n");
        return UpdateCheckChoice::Skip;
    }
    if (f.forcePreReleaseOnly) {
        if (f.userInitiated) {
            logf("CheckForUpdate: checking, user initiated (FORCE_AUTO_UPDATE)\n");
            return UpdateCheckChoice::Check;
        }
        logf("CheckForUpdate: skipping auto check, FORCE_AUTO_UPDATE downloads pre-release\n");
        return UpdateCheckChoice::Skip;
    }
    if (!f.hasInternet) {
        logf("CheckForUpdate: skipping because no internet access\n");
        return UpdateCheckChoice::Skip;
    }
    if (f.userInitiated) {
        logf("CheckForUpdate: checking, user initiated\n");
        return UpdateCheckChoice::Check;
    }
    if (!f.canSavePrefs) {
        logf("CheckForUpdate: skipping auto check because no prefs access\n");
        return UpdateCheckChoice::Skip;
    }
    if (!f.checkForUpdates) {
        logf("CheckForUpdate: skipping auto check because CheckForUpdates is false\n");
        return UpdateCheckChoice::Skip;
    }
    if (f.firstStart) {
        logf("CheckForUpdate: skipping auto check, first start (TimeOfLastUpdateCheck not set)\n");
        return UpdateCheckChoice::Skip;
    }
    if (f.isPreRelease) {
        logf("CheckForUpdate: checking, pre-release build checks on every startup\n");
        return UpdateCheckChoice::Check;
    }

    constexpr int kSecondsInDay = 60 * 60 * 24;
    constexpr int kSecondsInWeek = 7 * 60 * 60 * 24;
    int secsBetweenChecks = f.isPreRelease ? kSecondsInWeek : kSecondsInDay;
    bool checkUpdate = f.secsSinceLast > secsBetweenChecks;
    logf("CheckForUpdate: %s auto check, %d secs since the last one, %d secs between checks\n",
         checkUpdate ? StrL("doing") : StrL("skipping"), f.secsSinceLast, secsBetweenChecks);
    return checkUpdate ? UpdateCheckChoice::Check : UpdateCheckChoice::Skip;
}

bool ShouldOfferProgramUpdate(Str latestVer, Str myVer, bool isStoreBuild) {
    if (isStoreBuild) {
        return false;
    }
    return CompareProgramVersion(latestVer, myVer) > 0;
}

UpdateInfoFetch ClassifyUpdateInfoFetch(Str url, int error, int httpStatus, int bodyLen, const Str* allowed,
                                        int nAllowed) {
    if (error != 0) {
        return UpdateInfoFetch::TransportError;
    }
    if (httpStatus != 200) {
        return UpdateInfoFetch::BadStatus;
    }
    bool listed = false;
    for (int i = 0; i < nAllowed; i++) {
        if (str::StartsWith(url, allowed[i])) {
            listed = true;
            break;
        }
    }
    if (!listed) {
        return UpdateInfoFetch::UnlistedUrl;
    }
    if (bodyLen <= 0) {
        return UpdateInfoFetch::EmptyBody;
    }
    return UpdateInfoFetch::Ok;
}
