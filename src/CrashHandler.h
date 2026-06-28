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
