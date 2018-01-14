/* Copyright 2018 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD */

void InstallCrashHandler(const WCHAR* crashDumpPath, const WCHAR* symDir);
void SubmitCrashInfo();
void UninstallCrashHandler();
