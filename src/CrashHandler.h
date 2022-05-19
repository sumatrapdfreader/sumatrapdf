/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD */

extern char* gCrashFilePath;

void InstallCrashHandler(const char* crashDumpPath, const char* crashFilePath, const char* symDir);
void UninstallCrashHandler();
bool CrashHandlerDownloadSymbols();
bool SetSymbolsDir(const char* symDir);
