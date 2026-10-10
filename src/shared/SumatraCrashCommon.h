/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// --- shared by SumatraCrashCommon.cpp and orig's SumatraPDF.cpp and ng's SumatraCrashHandler.cpp ---

TempStr GetFileSizeAsStrTemp(Str path);
void AppendLogAndSettings(str::Builder& b);
void OnCrashBegin();

// --- shared by SumatraCrashCommon.cpp and each app's SumatraPDF.cpp ---

void AppendUncaughtMupdfError(Arena* a, str::Builder& b);

// implemented by each app
const char* LookupUncaughtMupdfError();
