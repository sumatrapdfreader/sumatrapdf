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
static bool IsHex4(Str s) {
    if (len(s) != 4) {
        return false;
    }
    for (int i = 0; i < 4; i++) {
        char c = s.s[i];
        bool hex = (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
        if (!hex) {
            return false;
        }
    }
    return true;
}

static bool UpdateTempTail(Str name, Str& tail) {
    if (len(name) < 11 || !str::StartsWithI(name, StrL("sum"))) {
        return false;
    }
    if (!IsHex4(Str(name.s + 3, 4))) {
        return false;
    }
    tail = Str(name.s + 7, len(name) - 7);
    return true;
}

bool IsUpdateTempFileName(Str name) {
    Str tail;
    if (!UpdateTempTail(name, tail)) {
        return false;
    }
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
        Str tail;
        if (!UpdateTempTail(e->name, tail)) {
            continue;
        }
        bool stub = str::EqI(tail, StrL(".tmp"));
        if (stub) {
            // GetTempFileName leaves a 0-byte file. A non-empty one belongs to someone else.
            if (e->size != 0) {
                continue;
            }
        } else {
            int age = FileTimeDiffInSecs(now, e->modificationTime);
            if (age < minAgeSec) {
                continue;
            }
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
