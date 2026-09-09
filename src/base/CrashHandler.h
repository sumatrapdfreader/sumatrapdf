/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// Everything app-specific the crash handler needs. Callbacks are optional
// (null is fine) and run on the crashing thread, so they must not allocate
// from the process heap.
struct CrashHandlerConfig {
    Str crashDumpPath;
    Str submitUrl;      // full url the .dmp is POSTed to, query included; empty disables upload
    Str fullDumpEnvVar; // if set in the environment, write a full dump
    bool localOnly;
    bool forTesting; // terminate on a debug report so automated tests fail
    bool uploadCrashes;
    bool uploadDebugReports;

    // Builds the whole text of a report: attached to the .dmp as its comment
    // stream and, for a local-only report, written to stderr. condStr/fileLine
    // describe the ReportIf() that fired and are empty for a crash. Everything
    // it returns must be allocated from a, which is the crash arena.
    Str (*getCrashComment)(Arena* a, Str condStr, Str fileLine, bool isCrash);
    void (*onCrashBegin)();
    void (*showCrashMessage)();
};

void InstallCrashHandler(const CrashHandlerConfig& cfg);
void UninstallCrashHandler();

// pre-allocated, so that a crash doesn't have to touch the process heap
Arena* CrashHandlerArena();

// os, cpu, memory and graphics driver info, gathered at install time so that
// getCrashComment() can use it without asking the system anything
Str CrashHandlerSystemInfo();
