/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// ng: launching another program and handing a path to the desktop. The Windows
// implementations are in Win.cpp (ShellExecuteEx / CreateProcess), which only
// builds there; Launch_posix.cpp is fork + exec with xdg-open / open.

#if !OS_WIN
// runs `path` with `params` as its command line. A path that isn't an
// executable is handed to the desktop (xdg-open on Linux, open on mac).
bool LaunchFileShell(Str path, Str params = {}, Str verb = {}, bool hidden = false);
bool LaunchFileShellArgs(const StrVec& args);
bool LaunchBrowser(Str url);
// shows the file's directory in the desktop file manager
void OpenPathInDefaultFileManager(Str path);
#endif
