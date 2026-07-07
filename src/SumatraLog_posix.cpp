/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/File.h"

#include "SumatraLog.h"

Str gLogAppName = StrL("SumatraPDF");
Arena* gLogAllocator = nullptr;
str::Builder* gLogBuf = nullptr;
bool gLogToConsole = false;
bool gLogToDebugger = false;
bool gReducedLogging = false;
bool gLogToPipe = false;
Str gLogFilePath;

static Mutex gLogMutex;
static bool gDestroyedLogging = false;

static void log2(Str s, bool) {
    if (!s || gDestroyedLogging) {
        return;
    }

    gLogMutex.Lock();
    AtomicIntInc(&gAllowAllocFailure);
    AutoCall decAllowAlloc(AtomicIntDec, &gAllowAllocFailure);

    if (!gLogBuf) {
        gLogAllocator = ArenaNew();
        gLogBuf = new str::Builder(32 * 1024, gLogAllocator);
    }
    gLogBuf->Append(s);

    if (gLogToConsole) {
        fwrite(s.s, 1, (size_t)s.len, stdout);
        fflush(stdout);
    }
    if (gLogFilePath) {
        FILE* f = fopen(gLogFilePath.s, "a");
        if (f) {
            fwrite(s.s, 1, (size_t)s.len, f);
            fclose(f);
        }
    }
    gLogMutex.Unlock();
}

void log(Str s) {
    log2(s, false);
}

void loga(Str s) {
    log2(s, true);
}

void StartLogToFile(Str path, bool removeIfExists) {
    str::FreePtr(&gLogFilePath);
    gLogFilePath = str::Dup(path);
    if (removeIfExists) {
        file::Delete(path);
    }
}

bool WriteCurrentLogToFile(Str path) {
    if (!gLogBuf) {
        return false;
    }
    Str slice = ToStr(*gLogBuf);
    if (!slice) {
        return false;
    }
    return dir::CreateForFile(path) && file::WriteFile(path, slice);
}

void DestroyLogging() {
    gDestroyedLogging = true;
    gLogMutex.Lock();
    delete gLogBuf;
    gLogBuf = nullptr;
    ArenaDelete(gLogAllocator);
    gLogAllocator = nullptr;
    gLogMutex.Unlock();
    str::FreePtr(&gLogFilePath);
}
