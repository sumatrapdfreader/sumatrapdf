/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// orig's OLE drag and drop of the canvas (Canvas.cpp). Windows only: gpui has
// no drag out of a window and surfaces only dropped files.

#if OS_WIN
// drag the selected text / win->imageDragElement out of the window
void StartTextDragDrop(MainWindow* win);
void StartImageDragDrop(MainWindow* win);
void DisconnectLastDragDataObject();
// files, image URLs and text dropped on the frame
void RegisterCanvasDropTarget(HWND hwnd);
void RevokeCanvasDropTarget(HWND hwnd);
// orig's OnDropFiles, for WM_DROPFILES and for an OLE drop (gui/NativeWindow.cpp)
bool AppShellOnDropFiles(HWND hwnd, HDROP hdrop, POINT ptDrop, bool inClient, bool dragFinish);
// for the automation channel; see the .cpp
// `toolWindow`: the name of the tool window to drop on instead of the frame
TempStr OleDragDropTestTemp(MainWindow* win, Str what, Str arg, int x, int y, Str toolWindow = {});
#endif
