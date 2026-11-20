/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// orig's OLE drag and drop of the canvas (Canvas.cpp), through gpui

// a press on the selection or an image may become a drag out of the window
bool CanDragOut(MainWindow* win);
// drag the selected text / win->imageDragElement out of the window
void StartTextDragDrop(MainWindow* win);
void StartImageDragDrop(MainWindow* win);
// text or a link (a browser's image) dropped on the canvas
void CanvasOnTextDrop(MainWindow* win, Str text, Str url);
// splits gpui's newline-separated dropped paths into `out`; returns how many
int SplitDroppedPaths(Str all, Str* out, int maxPaths);
