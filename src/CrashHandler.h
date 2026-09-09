/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD */

void InstallCrashHandler(Str crashDumpPath, bool localOnly);
void UninstallCrashHandler();
void CrashHandlerSetSettings(Str settings);
bool InitializeDbgHelp(bool force);

// The crash report is accumulated in one buffer owned by CrashHandler.cpp.
// GetProgramInfo() / GetStressTestInfo(), which each app implements, append to
// it with this instead of being handed a Builder.
void CrashInfoAppend(Str s);
