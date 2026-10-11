/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// --- shared by UpdateCheckCommon.cpp and each app's UpdateCheck.cpp ---

struct UpdateInfo;
extern UpdateInfo* gPendingUpdate;
static const Str kExpectedDlHost = StrL("https://www.sumatrapdfreader.org/");
bool IsTrustedUpdateDlUrl(Str dlURL);
void BuildUpdateURL(str::Builder& url, Str baseURL, UpdateCheck updateCheckType);

#if !OS_WIN
// implemented by ng: base/Win.h has them on Windows
bool IsProcess64();
bool IsArmBuild();
#endif

struct UpdateInfo {
    // the window that asked: orig keeps its frame, ng the window
    HWND hwndParent = nullptr;
    MainWindow* win = nullptr;
    Str latestVer;

    Str installer64;
    Str portable64;

    Str installer32;
    Str portable32;

    Str installerArm64;
    Str portableArm64;

    Str dlURL;
    Str installerPath;
    Str builtOn; // optional "yyyy-mm-dd" from the update-check file
    // optional, where the browser gets this build when we can't install it (ng)
    Str downloadPage;

    UpdateInfo() = default;
    ~UpdateInfo() {
        str::Free(latestVer);
        str::Free(installer64);
        str::Free(portable64);
        str::Free(installer32);
        str::Free(portable32);
        str::Free(installerArm64);
        str::Free(portableArm64);
        str::Free(dlURL);
        str::Free(installerPath);
        str::Free(builtOn);
        str::Free(downloadPage);
    }
};

UpdateInfo* ParseUpdateInfo(Str d);
