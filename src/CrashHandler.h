/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD */

extern WCHAR* gCrashFilePath;

void InstallCrashHandler(const WCHAR* crashDumpPath, const WCHAR* crashFilePath, const WCHAR* symDir);
void SubmitCrashInfo();
void UninstallCrashHandler();
bool CrashHandlerDownloadSymbols();
bool SetSymbolsDir(const WCHAR* symDir);
