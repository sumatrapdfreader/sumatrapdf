/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/UITask.h"
#if OS_WIN
#include "base/Win.h"
#endif
#include "base/SquareTreeParser.h"
#include "base/Http.h"
#include "base/File.h"
#include "base/Crypto.h"
#include "gui/UIModels.h"
#include "Settings.h"
#include "AppTools.h"
#include "AppSettings.h"
#include "Version.h"
#include "SumatraConfig.h"
#include "Translations.h"
#include "SumatraPDF.h"
#include "Notifications.h"
#include "MainWindow.h"
#include "HomePage.h"
#include "UpdateCheck.h"
#include "UpdateCheckCommon.h"

// an available update surfaced by the pre-release startup notification; the
// "Download and update" link downloads & installs it (owned here until then)
UpdateInfo* gPendingUpdate = nullptr;

bool HasPendingPreReleaseUpdate() {
    return gPendingUpdate != nullptr;
}

bool IsTrustedUpdateDlUrl(Str dlURL) {
    return str::StartsWith(dlURL, kExpectedDlHost);
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

void BuildUpdateURL(str::Builder& url, Str baseURL, UpdateCheck updateCheckType) {
    url.Reset(baseURL);
    AppendClientInfoQuery(url);
    url.Append(StrL("&withPromo"));
    if (UpdateCheck::UserInitiated == updateCheckType) {
        url.Append(StrL("&force"));
    }
}

UpdateInfo* ParseUpdateInfo(Str d) {
    // if a user configures os-wide proxy that is not a regular ie proxy
    // (which we pick up) we might get garbage http response
    // check if response looks valid
    if (len(d) == 0) {
        return nullptr;
    }
    Str prefix = (d.s[0] == '[') ? StrL("[SumatraPDF]") : StrL("SumatraPDF");
    if (!str::StartsWith(d, prefix)) {
        return nullptr;
    }

    SquareTreeNode* root = ParseSquareTree(d);
    if (!root) {
        return nullptr;
    }
    AutoDelete delRoot(root);

    SetPromoString(SerializeSquareTreeNodeTemp(root->GetChild(StrL("Promo"))));

    SquareTreeNode* node = root->GetChild(StrL("SumatraPDF"));
    if (!node) {
        return nullptr;
    }

    Str latestVer = node->GetValue(StrL("Latest"));
    if (!IsValidProgramVersion(latestVer)) {
        return nullptr;
    }
    auto* res = new UpdateInfo();
    res->latestVer = str::Dup(latestVer);
    Str onDate = node->GetValue(StrL("BuiltOn"));
    if (onDate) {
        res->builtOn = str::Dup(onDate);
    }

    // those are optional. if missing, we'll just tell the user to go to website to download
    res->installer64 = str::Dup(node->GetValue(StrL("Installer64")));
    res->installerArm64 = str::Dup(node->GetValue(StrL("InstallerArm64")));
    res->installer32 = str::Dup(node->GetValue(StrL("Installer32")));

    res->portable64 = str::Dup(node->GetValue(StrL("PortableExe64")));
    res->portableArm64 = str::Dup(node->GetValue(StrL("PortableExeArm64")));
    res->portable32 = str::Dup(node->GetValue(StrL("PortableExe32")));
    res->downloadPage = str::Dup(node->GetValue(StrL("DownloadPage")));

    // figure out which executable to download
    Str dlURL;
    bool isDll = IsDllBuild();
    if (IsArmBuild()) {
        dlURL = isDll ? res->installerArm64 : res->portableArm64;
    } else if (IsProcess64()) {
        dlURL = isDll ? res->installer64 : res->portable64;
    } else {
        dlURL = isDll ? res->installer32 : res->portable32;
    }
    res->dlURL = str::Dup(dlURL);
    return res;
}
