/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD */

extern Str gCrashFilePath;
extern Str gSymbolsDir;

void InstallCrashHandler(Str crashDumpPath, Str crashFilePath, Str symDir, bool localOnly);
void UninstallCrashHandler();
bool CrashHandlerDownloadSymbols();
bool AreSymbolsDownloaded(Str symDir);
bool InitializeDbgHelp(bool force);
bool SetSymbolsDir(Str symDir);

// The crash report is accumulated in one buffer owned by CrashHandler.cpp.
// GetProgramInfo() / GetStressTestInfo(), which each app implements, append to
// it with this instead of being handed a Builder.
void CrashInfoAppend(Str s);
