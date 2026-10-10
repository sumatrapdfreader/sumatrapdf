/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/File.h"
#include "base/Crypto.h"
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
    // The file is UTF-8 BOM + CRLF. This comment is LF text; a BOM or CR
    // here shows up as a blank line after every settings line.
    str::TrimPrefix(gSettingsFile, StrL(kUtf8Bom));
    str::NormalizeNewlinesToLFInPlace(gSettingsFile);
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
