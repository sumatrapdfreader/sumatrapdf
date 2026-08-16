/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct LinuxWindow;

LinuxWindow* LinuxWindowCreate(GtkApplication* app);
void LinuxWindowOpenFile(LinuxWindow* window, GFile* file);
void LinuxWindowPresent(LinuxWindow* window);
