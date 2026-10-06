/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/File.h"
#include "base/DirScan.h"
#if OS_WIN
#include "base/Win.h"
#endif

#include "UpdateTemp.h"

// GetTempFileName keeps only the first 3 letters of the prefix, so
// "sumatra-installer" is created as sum<4 hex>.tmp. The download is that
// path plus ".exe". The stub is unused. The exe is the installer.
bool IsUpdateTempFileName(Str name) {
    constexpr Str kPrefix = StrL("sum");
    constexpr int kHexLen = 4;
    int tailOff = len(kPrefix) + kHexLen;
    if (len(name) < tailOff + LenL(".tmp") || !str::StartsWithI(name, kPrefix)) {
        return false;
    }
    for (int i = len(kPrefix); i < tailOff; i++) {
        if (str::HexDigitVal(name.s[i]) < 0) {
            return false;
        }
    }
    Str tail(name.s + tailOff, len(name) - tailOff);
    return str::EqI(tail, StrL(".tmp")) || str::EqI(tail, StrL(".tmp.exe"));
}

#if OS_WIN
// minAgeSec applies to the installer exe. The .tmp stub is never used, so it goes at once.
void DeleteStaleUpdateTemps(Str dir, Str skip, int minAgeSec) {
    if (len(dir) == 0) {
        return;
    }
    FILETIME now{};
    GetSystemTimeAsFileTime(&now);
    StrVec doomed;
    DirIter di(dir);
    for (DirIterEntry* e : di) {
        if (!e->isFile || !IsUpdateTempFileName(e->name)) {
            continue;
        }
        if (skip && str::EqI(e->filePath, skip)) {
            continue;
        }
        bool stub = str::EndsWithI(e->name, StrL(".tmp"));
        // GetTempFileName leaves a 0-byte file. A non-empty one belongs to someone else.
        if (stub && e->size != 0) {
            continue;
        }
        if (!stub && FileTimeDiffInSecs(now, e->modificationTime) < minAgeSec) {
            continue;
        }
        doomed.Append(e->filePath);
    }
    for (Str p : doomed) {
        file::Delete(p);
    }
}

static bool gTempInstallerRelaunched = false;

void NoteTempInstallerRelaunch() {
    gTempInstallerRelaunched = true;
}

// The exe is still mapped, so a helper deletes it a few seconds after we exit.
void ScheduleDeleteTempInstaller() {
    if (gTempInstallerRelaunched) {
        return;
    }
    TempStr self = GetSelfExePathTemp();
    TempStr name = path::GetBaseNameTemp(self);
    if (!str::EndsWithI(name, StrL(".tmp.exe")) || !IsUpdateTempFileName(name)) {
        return;
    }
    TempStr temp = GetTempDirTemp();
    if (!path::IsInDir(self, temp)) {
        return;
    }
    TempStr cmd = fmt("cmd.exe /d /c ping 127.0.0.1 -n 4 >nul & del /f /q \"%s\"", self);
    HANDLE h = LaunchProcessInDir(cmd, Str(), CREATE_NO_WINDOW);
    if (h) {
        CloseHandle(h);
    }
    logf("ScheduleDeleteTempInstaller: '%s'\n", self);
}
#else
void DeleteStaleUpdateTemps(Str, Str, int) {}
void NoteTempInstallerRelaunch() {}
void ScheduleDeleteTempInstaller() {}
#endif
