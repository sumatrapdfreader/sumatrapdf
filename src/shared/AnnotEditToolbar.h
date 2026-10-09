/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

namespace gpui {
struct Ctx;
struct El;
} // namespace gpui

struct MainWindow;
struct WindowTab;
struct Annotation;
struct Gfx;
struct PlatformFont;
struct StrVec;

// the compact property row under the selected annotation ("Edit PDF" mode)
void UpdateAnnotEditToolbar(MainWindow*);
void HideAnnotEditToolbar(MainWindow*);
void RefreshAnnotEditToolbar(MainWindow*);
void DeleteAnnotEditToolbar(MainWindow*);
void SetAnnotEditToolbarClickPos(Annotation*, PointF pagePt);
gpui::El* AnnotEditToolbarBuild(MainWindow*, gpui::Ctx*);
TempStr AnnotEditToolbarStateTemp(MainWindow*);

// the contents editor inside the row; also what edits a free text annotation
void StartSelectedAnnotContentsEdit(MainWindow*);
bool IsEditingAnnotContents(MainWindow*);
void EndAnnotContentsEdit(bool accept);
void AnnotContentsKeepOnKillFocus(MainWindow*);
bool AnnotContentsEditJustEnded();
// free text is edited on the page, in a text box over the annotation
bool StartFreeTextInPlaceEdit(MainWindow*, Annotation*);
bool StartFreeTextInPlaceEditAt(MainWindow*, Point pt);
bool IsEditingFreeTextInPlace(MainWindow*);
void EndFreeTextInPlaceEdit(bool accept);
gpui::El* FreeTextInPlaceEditBuild(MainWindow*, gpui::Ctx*);

// the card with the properties of the annotation under the cursor, in Edit PDF
// ng: `mousePos` is the cursor on the canvas; orig asks GetCursorPos
void UpdateAnnotationHoverOverlay(MainWindow*, Point mousePos);
void HideAnnotationHoverOverlay(MainWindow*);
void RefreshAnnotationHoverOverlay(MainWindow*);
void DeleteAnnotationHoverOverlay(MainWindow*);
bool IsAnnotationHoverOverlayVisible(MainWindow*);
TempStr AnnotationHoverOverlayStateTemp(MainWindow*);
gpui::El* AnnotationHoverOverlayBuild(MainWindow*, gpui::Ctx*);

void DeleteAnnotationAndUpdateUI(WindowTab*, Annotation*);
void SetSelectedAnnotation(WindowTab*, Annotation*);
void RefreshAnnotationLists(WindowTab*);
void NotifyAnnotationsChanged(WindowTab*);
void StartLoadingAnnotationsForUi(WindowTab*);
void DetachAnnotationFromUI(Annotation*);
void InvalidateEditAnnotationsOnEngineChange(WindowTab*);
void RefreshEditAnnotationsAfterEngineChange(WindowTab*);
void CloseAnnotationUiForTab(WindowTab*);

SeqStrings AnnotationIconNames(Annotation*);
SeqStrings AnnotEditorLineEndingStyles();
// "Highlight" / "Highlight  the quick brown fox" - what the annotation list and
// the command palette show for one annotation
TempStr AnnotationListRowTextTemp(Annotation*);

#if OS_WIN
void RepositionAnnotEditToolbar(MainWindow*);
void RepositionFreeTextInPlaceEdit(MainWindow*);
HBRUSH FreeTextInPlaceEditCtlColor(HWND edit, HDC hdc);
TempStr FreeTextInPlaceEditStateTemp(MainWindow*);
void DrawAnnotationListRow(Gfx*, PlatformFont*, Rect, Annotation*, const StrVec& filterWords, Vec<u8>& hlScratch,
                           Color colBg, Color colText, bool selected);
void UpdateAnnotationHoverOverlay(MainWindow*);
void RepositionAnnotationHoverOverlay(MainWindow*);
#endif
TempStr AnnotEditorLayoutResultTemp(int clientDy, int selectItem, int* exitCodeOut = nullptr, int selectLast = 0);
