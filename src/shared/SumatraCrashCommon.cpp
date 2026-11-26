/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/File.h"
#include "base/Crypto.h"
#include "base/SettingsUtil.h"
#include "Settings.h"
#include "UpdateCheck.h"
#include "AppSettings.h"
#include "AppTools.h"
#include "base/CrashHandler.h"
#include "SumatraConfig.h"
#include "Version.h"
#include "SumatraPDF.h"
#include "SumatraCrashCommon.h"

#include "SumatraLog.h"

// What a crash report says about the app, the same in orig and ng.

TempStr GetFileSizeAsStrTemp(Str path) {
    i64 fileSize = file::GetSize(path);
    return str::FormatFileSizeTemp(fileSize);
}

// serialized settings, minus FileStates; lives in the crash arena so the
// minidump comment can use it without allocating
static Str gSettingsFile;

static AtomicBool gCrashReportsDisabled = 0;

bool CrashReportsDisabled() {
    return AtomicBoolGet(&gCrashReportsDisabled);
}

void CrashHandlerSetSettings(const Settings* settings) {
    AtomicBoolSet(&gCrashReportsDisabled, settings && settings->disableCrashReports);
    Arena* a = CrashHandlerArena();
    if (!a || !settings) {
        return;
    }

    Vec<FileState*> empty;
    Settings captured = *settings;
    captured.fileStates = &empty;
    Str serialized = SerializeSettings(&captured, {}, SerializeOpt::NoSecrets);
    gSettingsFile = str::Dup(a, serialized);
    str::Free(serialized);
    // Settings are UTF-8 BOM + CRLF. The report comment uses LF.
    str::TrimPrefix(gSettingsFile, StrL(kUtf8Bom));
    str::NormalizeNewlinesToLFInPlace(gSettingsFile);
}

void InstallConfiguredCrashHandler(CrashHandlerConfig cfg, CrashSettingsSource source) {
    Settings* settings = nullptr;
    if (source == CrashSettingsSource::File) {
        Str prefs = file::ReadFile(GetSettingsPathTemp());
        if (len(prefs) > 0) {
            settings = NewSettings(prefs);
        }
        str::Free(prefs);
    }

    // The POSIX handler may start sending pending reports during installation.
    AtomicBoolSet(&gCrashReportsDisabled, settings && settings->disableCrashReports);
    cfg.uploadsDisabled = CrashReportsDisabled;
    InstallCrashHandler(cfg);
    if (settings) {
        CrashHandlerSetSettings(settings);
        DeleteSettings(settings);
    }
}

void AppendLogAndSettings(str::Builder& b) {
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

void OnCrashBegin() {
    gReducedLogging = true;
}

void AppendUncaughtMupdfError(Arena* a, str::Builder& b) {
    const char* msg = LookupUncaughtMupdfError();
    if (!msg || !msg[0]) {
        return;
    }
    // High-visibility: a crash with nothing interesting on the stack (the
    // intentional null-write) still needs to explain the real failure
    // (MuPDF throw with no fz_try).
    b.Append(str::Format(a, "Uncaught MuPDF error: %s\n\n", Str(msg)));
}
