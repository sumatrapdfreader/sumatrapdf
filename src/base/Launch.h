/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// ng: launching another program and handing a path to the desktop. The Windows
// implementations are in Win.cpp (ShellExecuteEx / CreateProcess), which only
// builds there; Launch_posix.cpp is fork + exec with xdg-open / open.

// set in test mode (-for-testing): nothing is launched or handed to the
// desktop, so tests don't open a browser or Finder in front of whoever uses
// the computer. ExternalLaunchBlocked logs what would have been opened.
extern bool gBlockExternalLaunch;
// how many were blocked, for a test to check
extern int gExternalLaunchesBlocked;
bool ExternalLaunchBlocked(Str what);
void SetExternalLaunchAllowDir(Str dir);
void OpenFilesInDefaultManager(Str folder, const StrVec& files);

#if !OS_WIN
// hands `path` to the desktop (xdg-open on Linux, open on mac); params are ignored
bool LaunchFileShell(Str path, Str params = {}, Str verb = {}, bool hidden = false);
#if !OS_WASM
bool LaunchFileShellArgs(const StrVec& args);
void ProgramArgs(Str exe, Str argsTemplate, const Func1<Str*>& expand, StrVec& args);
#endif
bool LaunchProgram(Str exe, Str cmdLine);
bool LaunchBrowser(Str url);
// shows the file's directory in the desktop file manager
void OpenPathInDefaultFileManager(Str path);
#endif
