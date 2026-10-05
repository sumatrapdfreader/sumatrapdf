/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct MainWindow;

// A top-level window with a read-only, scrollable multi-line text editor.
void ShowTextInWindow(MainWindow* win, Str title, Str text);
void SetTextViewWindowText(Str text);
bool IsTextViewWindowTitle(Str title);
void CloseTextViewWindow();
bool IsTextViewWindowVisible();
