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
