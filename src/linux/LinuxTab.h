/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct DocumentView;
struct LinuxTab;

LinuxTab* LinuxTabCreate(Str title, const Func0& onStateChanged);
void LinuxTabDestroy(LinuxTab* tab);
bool LinuxTabOpenFile(LinuxTab* tab, GFile* file);
GtkWidget* LinuxTabWidget(LinuxTab* tab);
DocumentView* LinuxTabView(LinuxTab* tab);
Str LinuxTabTitle(LinuxTab* tab);
Str LinuxTabPath(LinuxTab* tab);
