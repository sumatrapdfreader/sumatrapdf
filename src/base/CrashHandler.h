/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// Everything app-specific the crash handler needs. Callbacks are optional
// (null is fine) and run on the crashing thread, so they must not allocate
// from the process heap: append with CrashInfoAppend(), format into
// CrashHandlerArena().
struct CrashHandlerConfig {
    Str crashDumpPath;
    Str submitUrl;      // where the .dmp is POSTed; empty disables upload
    Str fullDumpEnvVar; // if set in the environment, write a full dump
    bool localOnly;
    bool forTesting; // terminate on a debug report so automated tests fail
    bool uploadCrashes;
    bool uploadDebugReports;

    void (*appendProgramInfo)();
    void (*appendExtraInfo)();       // app-specific detail shown before the callstacks
    void (*appendMinidumpComment)(); // extra text for the .dmp comment stream
    void (*onCrashBegin)();
    void (*showCrashMessage)();
    void (*appendUploadQuery)(str::Builder& url);
};

void InstallCrashHandler(const CrashHandlerConfig& cfg);
void UninstallCrashHandler();

// The crash report is accumulated in one buffer owned by CrashHandler.cpp.
// The config callbacks append to it with this instead of being handed a Builder.
void CrashInfoAppend(Str s);
Arena* CrashHandlerArena();
