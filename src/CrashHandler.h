/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD */

extern char* gCrashFilePath;
extern char* gSymbolsDir;

void InstallCrashHandler(const char* crashDumpPath, const char* crashFilePath, const char* symDir);
void UninstallCrashHandler();
bool CrashHandlerDownloadSymbols();
bool AreSymbolsDownloaded(const char* symDir);
bool InitializeDbgHelp(bool force);
bool SetSymbolsDir(const char* symDir);
