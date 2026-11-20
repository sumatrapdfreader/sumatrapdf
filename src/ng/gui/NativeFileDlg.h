/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct SavePathArgs;

bool NativeFileDlgEnabled();
void NativeSaveFileDlg(SavePathArgs* args);
bool NativeOpenFileDlg(MainWindow* win, Str title, Str filter, Str initialPath, bool multiSelect, StrVec* pathsOut);
#if OS_WIN
bool NativeOpenDocsDlg(MainWindow* win, StrVec* pathsOut);
#endif
