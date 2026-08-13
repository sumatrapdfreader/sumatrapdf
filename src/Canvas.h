/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

void UpdateDeltaPerLine();

LRESULT CALLBACK WndProcCanvas(HWND, UINT, WPARAM, LPARAM);
LRESULT WndProcCanvasAbout(MainWindow*, HWND, UINT, WPARAM, LPARAM);
bool IsDragDistance(int x1, int x2, int y1, int y2);
void CancelDrag(MainWindow*);
void StartAutoScrollAtCursor(MainWindow*);
bool ShowImageOutlines();
void ToggleShowImageOutlines();
bool ShowFitContentArea();
void ToggleShowFitContentArea();
bool IsLaserPointerActive();
void ToggleLaserPointer(MainWindow*);
void DeleteLaserPointerCursor();
void DrawCanvasKeyboardFocusIfNeeded(MainWindow* win, HDC hdc);
void InvalidateCanvasKeyboardFocus(MainWindow* win);

extern Kind kNotifAnnotation;

void RegisterCanvasDropTarget(HWND hwndCanvas);
void RevokeCanvasDropTarget(HWND hwndCanvas);
void FillCanvasThemeBackground(HWND hwndCanvas);
void DisconnectLastDragDataObject();

// Timer for mouse wheel smooth scrolling
constexpr UINT_PTR kSmoothScrollTimerID = 6;
// Timer for smooth middle-click auto-scroll (issue #2693)
constexpr UINT_PTR kAutoScrollTimerID = 7;
// Debounce for re-numbering keyboard link targets after scrolling (issue #2629)
constexpr UINT_PTR kLinkFollowTimerID = 11;
constexpr uint kLinkFollowRecomputeDelayInMs = 300;
// Blink for the keyboard text selection caret (issues #4684, #4116)
constexpr UINT_PTR kTextSelectCaretTimerID = 12;
// Debounce for popping up the floating selection toolbar after a selection
constexpr UINT_PTR kSelectionToolbarShowTimerID = 13;
constexpr uint kSelectionToolbarShowDelayInMs = 500;
// A finger held still selects the word under it (issue #538). Touch reaches us
// either as a gesture stream or as synthesized mouse messages, so the hold is
// detected both ways; this timer is the mouse-message half.
constexpr UINT_PTR kTouchLongPressTimerID = 14;
