/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct MainWindow;
enum class PrintScaleAdv;

// Opens the Windows 11 print dialog (with preview) for the current document.
// Returns false when it isn't available -- older Windows, no fixed-page
// document, a selection to print, or the WinRT pipeline failing to start -- and
// the caller falls back to the classic PrintDlgEx path.
bool TryPrintCurrentFileWin11(MainWindow* win, PrintScaleAdv defaultScale);

// frees the print session; call once, on app shutdown
void ShutdownWin11Printing();
