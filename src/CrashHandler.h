/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef CrashHandler_h
#define CrashHandler_h

// #define CMD_ARG_SEND_CRASHDUMP  _T("/sendcrashdump")

void InstallCrashHandler(const TCHAR *crashDumpPath, const TCHAR *crashTxtPath);

void SaveCrashInfoText();

#endif
