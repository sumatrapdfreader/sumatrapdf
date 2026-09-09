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
extern void GetProgramInfo();
extern void ShowCrashHandlerMessage();

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

static void AppendUncaughtMupdfError() {
    const char* msg = LookupUncaughtMupdfError();
    if (!msg || !msg[0]) {
        return;
    }
    // High-visibility: empty callstacks from the intentional null-write still
    // need to explain the real failure (MuPDF throw with no fz_try).
    CrashInfoAppend(str::Format(CrashHandlerArena(), "Uncaught MuPDF error: %s\n\n", Str(msg)));
}

// the .dmp already carries stacks and modules, so the comment adds what it
// can't: the log and the user's settings
static void AppendMinidumpComment() {
    CrashInfoAppend(StrL("\n-------- Log -----------------\n\n"));
    if (gLogBuf) {
        CrashInfoAppend(ToStr(*gLogBuf));
    } else {
        CrashInfoAppend(StrL("(no log - crashed before initializing logging)\n"));
    }
    if (len(gSettingsFile) == 0) {
        return;
    }
    CrashInfoAppend(StrL("\n--- settings ---\n"));
    CrashInfoAppend(gSettingsFile);
    CrashInfoAppend(StrL("\n"));
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
    cfg.submitUrl = StrL(kMinidumpSubmitUrl);
    cfg.fullDumpEnvVar = StrL("SUMATRAPDF_FULLDUMP");
    cfg.localOnly = localOnly;
    cfg.forTesting = gForTesting;
    // a debug build is likely someone else modifying the code
    cfg.uploadCrashes = !gIsDebugBuild && !gIsAsanBuild;
    // a debug report carries too much info to send from a release build
    cfg.uploadDebugReports = gIsPreReleaseBuild;
    cfg.appendProgramInfo = GetProgramInfo;
    cfg.appendExtraInfo = AppendUncaughtMupdfError;
    cfg.appendMinidumpComment = AppendMinidumpComment;
    cfg.onCrashBegin = OnCrashBegin;
    cfg.showCrashMessage = ShowCrashHandlerMessage;
    cfg.appendUploadQuery = AppendClientInfoQuery;

    InstallCrashHandler(cfg);
    CaptureSettings();
}
