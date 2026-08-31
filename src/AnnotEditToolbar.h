/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct MainWindow;
struct WindowTab;
struct Annotation;
struct Gfx;
struct PlatformFont;
struct StrVec;
template <typename T>
struct Vec;

void UpdateAnnotEditToolbar(MainWindow*);
void HideAnnotEditToolbar(MainWindow*);
void StartSelectedAnnotContentsEdit(MainWindow*);
void RepositionAnnotEditToolbar(MainWindow*);
void RefreshAnnotEditToolbar(MainWindow*);
void DeleteAnnotEditToolbar(MainWindow*);
TempStr AnnotEditToolbarStateTemp(MainWindow*);

bool StartFreeTextInPlaceEdit(MainWindow*, Annotation*);
bool StartFreeTextInPlaceEditAt(MainWindow*, Point);
bool IsEditingFreeTextInPlace(MainWindow*);
bool AnnotContentsEditJustEnded();
void EndFreeTextInPlaceEdit(bool accept);
void RepositionFreeTextInPlaceEdit(MainWindow*);
TempStr FreeTextInPlaceEditStateTemp(MainWindow*);

void DeleteAnnotationAndUpdateUI(WindowTab*, Annotation*);
void SetSelectedAnnotation(WindowTab*, Annotation*);
void RefreshAnnotationLists(WindowTab*);
void NotifyAnnotationsChanged(WindowTab*);
void StartLoadingAnnotationsForUi(WindowTab*);
void DrawAnnotationListRow(Gfx*, PlatformFont*, Rect, Annotation*, const StrVec& filterWords, Vec<u8>& hlScratch,
                           Color colBg, Color colText, bool selected);
void DetachAnnotationFromUI(Annotation*);
void InvalidateEditAnnotationsOnEngineChange(WindowTab*);
void RefreshEditAnnotationsAfterEngineChange(WindowTab*);
void CloseAnnotationUiForTab(WindowTab*);
void UpdateAnnotationHoverOverlay(MainWindow*);
void RepositionAnnotationHoverOverlay(MainWindow*);
void HideAnnotationHoverOverlay(MainWindow*);
void RefreshAnnotationHoverOverlay(MainWindow*);
void DeleteAnnotationHoverOverlay(MainWindow*);
TempStr AnnotationHoverOverlayStateTemp(MainWindow*);
TempStr AnnotEditorLayoutResultTemp(int clientDy, int selectItem, int* exitCodeOut = nullptr, int selectLast = 0);
SeqStrings AnnotationIconNames(Annotation*);
SeqStrings AnnotEditorColorNames();
int AnnotEditorColorCount();
PdfColor AnnotEditorColorAt(int);
Str AnnotEditorColorNameAt(int);
SeqStrings AnnotEditorLineEndingStyles();
SeqStrings AnnotEditorFontNames();
SeqStrings AnnotEditorFontReadableNames();
