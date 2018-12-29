/* Copyright 2018 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// code used in both Installer.cpp and Uninstaller.cpp

// TODO: not all those are needed
#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/WinDynCalls.h"
#include <tlhelp32.h>
#include <io.h>
#include "utils/FileUtil.h"
#include "Translations.h"
#include "Resource.h"
#include "utils/Timer.h"
#include "Version.h"
#include "utils/WinUtil.h"
#include "Installer.h"
#include "utils/CmdLineParser.h"
#include "CrashHandler.h"
#include "utils/Dpi.h"
#include "utils/FrameTimeoutCalculator.h"
#include "utils/DebugLog.h"

#include "utils/ByteOrderDecoder.h"
#include "utils/LzmaSimpleArchive.h"

#include "../ifilter/PdfFilter.h"
#include "../previewer/PdfPreview.h"

#define TEN_SECONDS_IN_MS 10 * 1000

static bool IsProcWithName(DWORD processId, const WCHAR* modulePath) {
    ScopedHandle hModSnapshot(CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, processId));
    if (!hModSnapshot.IsValid())
        return false;

    MODULEENTRY32W me32 = {0};
    me32.dwSize = sizeof(me32);
    BOOL ok = Module32FirstW(hModSnapshot, &me32);
    while (ok) {
        if (path::IsSame(modulePath, me32.szExePath))
            return true;
        ok = Module32NextW(hModSnapshot, &me32);
    }
    return false;
}

// Kill a process with given <processId> if it has a module (dll or exe) <modulePath>.
// If <waitUntilTerminated> is true, will wait until process is fully killed.
// Returns TRUE if killed a process
static bool KillProcIdWithName(DWORD processId, const WCHAR* modulePath, bool waitUntilTerminated) {
    if (!IsProcWithName(processId, modulePath))
        return false;

    BOOL inheritHandle = FALSE;
    // Note: do I need PROCESS_QUERY_INFORMATION and PROCESS_VM_READ?
    DWORD dwAccess = PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | PROCESS_TERMINATE;
    ScopedHandle hProcess(OpenProcess(dwAccess, inheritHandle, processId));
    if (!hProcess.IsValid())
        return false;

    BOOL killed = TerminateProcess(hProcess, 0);
    if (!killed)
        return false;

    if (waitUntilTerminated)
        WaitForSingleObject(hProcess, TEN_SECONDS_IN_MS);

    return true;
}

// returns number of killed processes that have a module (exe or dll) with a given
// modulePath
// returns -1 on error, 0 if no matching processes
int KillProcess(const WCHAR* modulePath, bool waitUntilTerminated) {
    ScopedHandle hProcSnapshot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
    if (INVALID_HANDLE_VALUE == hProcSnapshot)
        return -1;

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(pe32);
    if (!Process32First(hProcSnapshot, &pe32))
        return -1;

    int killCount = 0;
    do {
        if (KillProcIdWithName(pe32.th32ProcessID, modulePath, waitUntilTerminated))
            killCount++;
    } while (Process32Next(hProcSnapshot, &pe32));

    if (killCount > 0) {
        UpdateWindow(FindWindow(nullptr, L"Shell_TrayWnd"));
        UpdateWindow(GetDesktopWindow());
    }
    return killCount;
}

/* if the app is running, we have to kill it so that we can over-write the executable */
void KillSumatra() {
    WCHAR* exePath = GetInstalledExePath();
    KillProcess(exePath, true);
    str::Free(exePath);
}
