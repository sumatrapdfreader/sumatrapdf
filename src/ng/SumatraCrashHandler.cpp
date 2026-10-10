/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: orig keeps this in SumatraPDF.cpp; it is everything application-specific
// that base/CrashHandler.cpp asks for - where the minidump goes, where it is
// POSTed, and the text of the report. Split out because SumatraPDF.cpp is big
// enough, and because the POSIX half (a signal handler that logs a backtrace)
// lives next to it in base/CrashHandler_posix.cpp.

#include "base/Base.h"
#include "base/File.h"
#include "base/Crypto.h"
#include "base/CrashHandler.h"
#include "base/SettingsUtil.h"
#if OS_WIN
#include "base/WinDynCalls.h"
#include "base/DbgHelpDyn.h"
#include "base/Win.h"
#endif

#include "Settings.h"
#include "AppTools.h"
#include "AppSettings.h"
#include "Version.h"
#include "SumatraConfig.h"
#include "SumatraPDF.h"
#include "UpdateCheck.h"
#include "ChapterTable.h"
#include "ReadAloud.h"
#include "SumatraCrashHandler.h"
#include "SumatraCrashCommon.h"

#include "SumatraLog.h"

#if IS_DEBUG
#define kCrashServer "http://127.0.0.1:9321"
#else
#define kCrashServer "https://www.sumatrapdfreader.org"
#endif

// Windows POSTs a minidump, macOS a text report; each has its own store on
// the server. Empty: nothing to upload to.
#if OS_WIN
#define kCrashSubmitUrl kCrashServer "/app/sumatrapdfng-win/uploadminidump"
#elif OS_DARWIN
#define kCrashSubmitUrl kCrashServer "/app/sumatrapdfng-mac/uploadcrash"
#else
#define kCrashSubmitUrl ""
#endif

// where InstallCrashHandler() writes the .dmp, in the crash arena
static Str gCrashDumpPath;

// ng: orig has GetSumatraBuildSpecificDirTemp(); here the crash info goes next
// to the screenshots, under the application data directory
TempStr GetCrashInfoDirTemp() {
    TempStr dir = GetAppDataDirTemp();
    if (len(dir) == 0) {
        return {};
    }
    return path::JoinTemp(dir, StrL("crashinfo"));
}

// Message from MuPDF's uncaught-throw abort (error.c). Looked up at crash time
// so we do not need a hard link for every tool that builds CrashHandlerNoOp.
const char* LookupUncaughtMupdfError() {
#if OS_WIN
    using Fn = const char* (*)();
    HMODULE h = GetModuleHandleW(nullptr);
    if (h) {
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

static void GetProgramInfo(str::Builder& b) {
    TempStr exePath = GetSelfExePathTemp();
    b.Append(fmt("Exe: %s %s\n", exePath, GetFileSizeAsStrTemp(exePath)));
    TempStr signer = GetExecutableSignerTemp(exePath);
    b.Append(fmt("Signer: %s\n", signer ? signer : StrL("(not signed)")));
    b.Append(fmt("BuiltOn: %s\n", BuiltOnDate()));
    Str instType = IsRunningInPortableMode() ? StrL("portable") : StrL("installed");
    b.Append(fmt("ExeType: %s\n", instType));
    b.Append(fmt("Ver: %s", currentVersion));
    if (gIsPreReleaseBuild) {
        b.Append(StrL(" pre-release"));
    }
#if OS_WIN
    if (IsProcess64()) {
        b.Append(StrL(" 64-bit"));
    } else {
        b.Append(StrL(" 32-bit"));
        if (IsRunningInWow64()) {
            b.Append(StrL(" Wow64"));
        }
    }
#endif
    if (gIsDebugBuild) {
        if (!str::Contains(currentVersion, StrL(" (dbg)"))) {
            b.Append(StrL(" (dbg)"));
        }
    }
    b.Append(StrL("\n"));

    if (gitCommidId) {
        b.Append(fmt("Git: %s (https://github.com/sumatrapdfreader/sumatrapdf/commit/%s)\n", gitCommidId, gitCommidId));
    }
}

// ng: orig shows a win32 MsgBox and, on Cancel, the crash-report page. The
// port's MsgBox is a gpui dialog that needs a live window and a running frame
// loop, neither of which a crashed process has, so this is the raw win32 one
static void ShowCrashHandlerMessage() {
    log(StrL("ShowCrashHandlerMessage\n"));
    if (!CanAccessDisk()) {
        log(StrL("ShowCrashHandlerMessage: skipping because !CanAccessDisk()\n"));
        return;
    }
#if OS_WIN
    Str msg = StrL("SumatraPDF crashed.\n\nPress 'Cancel' to see the crash report.");
    uint flags = MB_ICONERROR | MB_OKCANCEL | MB_SETFOREGROUND | MB_TOPMOST;
    int res = MessageBoxW(nullptr, CWStrTemp(msg), L"SumatraPDF crashed", flags);
    if (IDCANCEL != res) {
        return;
    }
    LaunchFileShell(StrL("https://www.sumatrapdfreader.org/docs/Submit-crash-report.html"), {}, StrL("open"));
#endif
}

// The .dmp we write alongside this already has the stacks, the modules and the
// exception record, in a form a debugger can actually use, so the text is for
// what it can't hold: what we are, what tripped, and the log and settings.
// Runs on the crashing thread, so everything is allocated from a.
static Str GetCrashComment(Arena* a, Str condStr, Str fileLine, bool isCrash) {
    str::Builder b(a);
    b.Reserve(16 * 1024);
#if OS_WIN
    if (isCrash) {
        b.Append(StrL("Type: crash (minidump)\n"));
    } else {
        b.Append(StrL("Type: debug report (not crash)\n"));
    }
    b.Append(str::Format(a, "Minidump: %s\n", gCrashDumpPath));
#else
    b.Append(isCrash ? StrL("Type: crash\n") : StrL("Type: debug report (not crash)\n"));
#endif
    if (condStr) {
        b.Append(str::Format(a, "Cond: %s @ %s\n", condStr, fileLine));
    }
#if OS_WIN
    if (!isCrash) {
        // Belt and braces for a debug report: we run on the reporting thread,
        // so walk it now, before any dump machinery gets involved
        b.Append(StrL("\n--- reporting thread ---\n"));
        if (!dbghelp::GetCurrentThreadCallstack(b)) {
            b.Append(StrL("(callstack not available)\n"));
        }
        b.Append(StrL("\n"));
    }
#endif
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

// FileStates are the largest part and we don't need them in a crash report
static void CaptureSettings() {
    TempStr path = GetSettingsPathTemp();
    // can be empty on first run but that's fine because then we know it has default values
    Str prefsData = file::ReadFile(path);
    if (len(prefsData) == 0) {
        return;
    }
    Settings* gp = NewSettings(prefsData);
    DeleteFileStates(gp->fileStates);
    gp->fileStates = new Vec<FileState*>();
    Str d = SerializeSettings(gp, {});
    CrashHandlerSetSettings(d);
    str::Free(d);
    DeleteSettings(gp);
    str::Free(prefsData);
}

// the client info is the same as for the update check and doesn't change while
// we run, so build the url now rather than at crash time
static TempStr BuildSubmitUrlTemp() {
    if (sizeof(kCrashSubmitUrl) == 1) {
        return {};
    }
    str::Builder url(GetTempArena());
    url.Append(StrL(kCrashSubmitUrl));
    AppendClientInfoQuery(url);
    return ToStr(url);
}

#define kOfficialSigner "Krzysztof Kowalczyk"

// crashes from third-party builds (forks, distro rebuilds) aren't ours to fix
static bool IsOfficialBuild() {
#if OS_DARWIN
    // the mac build is ad-hoc signed, so there is no signer to tell builds apart
    return true;
#else
    TempStr signer = GetExecutableSignerTemp(GetSelfExePathTemp());
    return str::Eq(signer, StrL(kOfficialSigner));
#endif
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
    cfg.uploadCrashes = !gIsAsanBuild;
    // a debug report carries too much info to send from a release build
    cfg.uploadDebugReports = gIsPreReleaseBuild;
    if (!gIsDebugBuild && !IsOfficialBuild()) {
        log(StrL("InstallSumatraCrashHandler: not signed by us, not uploading\n"));
        cfg.uploadCrashes = false;
        cfg.uploadDebugReports = false;
    }
    cfg.getCrashComment = GetCrashComment;
    cfg.onCrashBegin = OnCrashBegin;
    cfg.showCrashMessage = ShowCrashHandlerMessage;
    // a SAPI voice engine that faults on its own thread ends only that thread
    cfg.canEndCrashedThread = TtsOnEngineCrash;

    InstallCrashHandler(cfg);
    Arena* a = CrashHandlerArena();
    if (a) {
        gCrashDumpPath = str::Dup(a, cfg.crashDumpPath);
    }
    logf("InstallSumatraCrashHandler: dump '%s', localOnly=%d\n", cfg.crashDumpPath, (int)localOnly);
    CaptureSettings();
}
