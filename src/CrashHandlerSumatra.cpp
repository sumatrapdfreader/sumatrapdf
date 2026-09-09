/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"

#include "base/File.h"
#include "base/CrashHandler.h"

#include "Settings.h"
#include "AppTools.h"
#include "SumatraConfig.h"
#include "AppSettings.h"
#include "SumatraPDF.h"
#include "UpdateCheck.h"
#include "CrashHandlerSumatra.h"
#include "SumatraLog.h"

#if IS_DEBUG
#define kMinidumpSubmitUrl "http://127.0.0.1:9321/uploadminidump"
#else
#define kMinidumpSubmitUrl "https://www.sumatrapdfreader.org/uploadminidump"
#endif

// implemented in SumatraPDF.cpp
extern void GetProgramInfo(str::Builder&);
extern void ShowCrashHandlerMessage();

// where InstallCrashHandler() writes the .dmp, in the crash arena
static Str gCrashDumpPath;

// serialized settings, minus FileStates; lives in the crash arena so the
// minidump comment can use it without allocating
static Str gSettingsFile;

void CrashHandlerSetSettings(Str settings) {
    Arena* a = CrashHandlerArena();
    if (!a) {
        return;
    }
    gSettingsFile = {};
    if (len(settings) == 0) {
        return;
    }
    gSettingsFile = str::Dup(a, settings);
}

// Message from MuPDF's uncaught-throw abort (error.c). Looked up at crash time
// so we do not need a hard link for every tool that builds CrashHandlerNoOp.
// libsumatrapdf.dll (or the static main module) exports fz_last_uncaught_error.
static const char* LookupUncaughtMupdfError() {
#if OS_WIN
    using Fn = const char* (*)();
    HMODULE modules[2] = {
        GetModuleHandleW(L"libsumatrapdf.dll"),
        GetModuleHandleW(nullptr),
    };
    for (HMODULE h : modules) {
        if (!h) {
            continue;
        }
        auto fn = (Fn)GetProcAddress(h, "fz_last_uncaught_error");
        if (fn) {
            const char* msg = fn();
            if (msg && msg[0]) {
                return msg;
            }
        }
    }
#endif
    return nullptr;
}

static void AppendUncaughtMupdfError(Arena* a, str::Builder& b) {
    const char* msg = LookupUncaughtMupdfError();
    if (!msg || !msg[0]) {
        return;
    }
    // High-visibility: a crash with nothing interesting on the stack (the
    // intentional null-write) still needs to explain the real failure
    // (MuPDF throw with no fz_try).
    b.Append(str::Format(a, "Uncaught MuPDF error: %s\n\n", Str(msg)));
}

static void AppendLogAndSettings(str::Builder& b) {
    b.Append(StrL("\n-------- Log -----------------\n\n"));
    if (gLogBuf) {
        b.Append(ToStr(*gLogBuf));
    } else {
        b.Append(StrL("(no log - crashed before initializing logging)\n"));
    }
    if (len(gSettingsFile) == 0) {
        return;
    }
    b.Append(StrL("\n--- settings ---\n"));
    b.Append(gSettingsFile);
    b.Append(StrL("\n"));
}

// The .dmp we write alongside this already has the stacks, the modules and the
// exception record, in a form a debugger can actually use, so the text is for
// what it can't hold: what we are, what tripped, and the log and settings.
// Runs on the crashing thread, so everything is allocated from a.
static Str GetCrashComment(Arena* a, Str condStr, Str fileLine, bool isCrash) {
    str::Builder b(a);
    b.Reserve(16 * 1024);
    if (isCrash) {
        b.Append(StrL("Type: crash (minidump)\n"));
    } else {
        b.Append(StrL("Type: debug report (not crash)\n"));
    }
    b.Append(str::Format(a, "Minidump: %s\n", gCrashDumpPath));
    if (condStr) {
        b.Append(str::Format(a, "Cond: %s @ %s\n", condStr, fileLine));
    }
    GetProgramInfo(b);
    AppendUncaughtMupdfError(a, b);
    Str sysInfo = CrashHandlerSystemInfo();
    if (sysInfo) {
        b.Append(sysInfo);
        b.Append(StrL("\n"));
    }
    AppendLogAndSettings(b);
    return ToStr(b);
}

static void OnCrashBegin() {
    gReducedLogging = true;
}

// FileStates are the largest part and we don't need them in a crash report
static void CaptureSettings() {
    // installer/uninstaller don't use app settings; reading them here would
    // trigger GetAppDataDirTemp() before installation is complete
    if (IsInstallerOrUninstallerExe()) {
        return;
    }
    TempStr path = GetSettingsPathTemp();
    // can be empty on first run but that's fine because then we know it has default values
    Str prefsData = file::ReadFile(path);
    if (len(prefsData) == 0) {
        return;
    }
    Settings* gp = NewSettings(prefsData);
    DeleteFileStates(gp->fileStates);
    gp->fileStates = new Vec<FileState*>();
    // TODO: also sessionData?
    Str d = SerializeSettings(gp, {});
    CrashHandlerSetSettings(d);
    str::Free(d);
    DeleteSettings(gp);
    str::Free(prefsData);
}

// the client info is the same as for the update check and doesn't change while
// we run, so build the url now rather than at crash time
static TempStr BuildSubmitUrlTemp() {
    str::Builder url(GetTempArena());
    url.Append(StrL(kMinidumpSubmitUrl));
    AppendClientInfoQuery(url);
    return ToStr(url);
}

void InstallSumatraCrashHandler(bool localOnly) {
    if (gIsAsanBuild) {
        return;
    }
    // a crash report we could never send is not worth the machinery, so this
    // must run after InitializePolicies()
    if (!HasPermission(Perm::InternetAccess)) {
        log(StrL("InstallSumatraCrashHandler: skipping, no Perm::InternetAccess\n"));
        return;
    }

    TempStr crashInfoDir = GetCrashInfoDirTemp();

    CrashHandlerConfig cfg{};
    cfg.crashDumpPath = path::JoinTemp(crashInfoDir, StrL("sumatrapdfcrash.dmp"));
    cfg.submitUrl = BuildSubmitUrlTemp();
    cfg.fullDumpEnvVar = StrL("SUMATRAPDF_FULLDUMP");
    cfg.localOnly = localOnly;
    cfg.forTesting = gForTesting;
    // a debug build is likely someone else modifying the code
    cfg.uploadCrashes = !gIsDebugBuild && !gIsAsanBuild;
    // a debug report carries too much info to send from a release build
    cfg.uploadDebugReports = gIsPreReleaseBuild;
    cfg.getCrashComment = GetCrashComment;
    cfg.onCrashBegin = OnCrashBegin;
    cfg.showCrashMessage = ShowCrashHandlerMessage;

    InstallCrashHandler(cfg);
    Arena* a = CrashHandlerArena();
    if (a) {
        gCrashDumpPath = str::Dup(a, cfg.crashDumpPath);
    }
    CaptureSettings();
}
