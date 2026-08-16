/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct LinuxWindow;

LinuxWindow* LinuxWindowCreate(GtkApplication* app);
void LinuxWindowOpenFile(LinuxWindow* window, GFile* file);
void LinuxWindowRestoreSession(LinuxWindow* window);
void LinuxWindowSaveSession(LinuxWindow* window);
void LinuxWindowDispatchCommand(LinuxWindow* window, int commandId);
void LinuxWindowFindText(LinuxWindow* window, Str text);
void LinuxWindowGoToFavorite(LinuxWindow* window, int index);
void LinuxWindowGoToPage(LinuxWindow* window, int pageNo);
void LinuxWindowGoToTocItem(LinuxWindow* window, int index);
void LinuxWindowPresent(LinuxWindow* window);
