/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct EditAnnotationsWindow;
struct MainWindow;

enum class EditAnnotFocus {
    Default,
    Edit,
    List,
};

void ShowEditAnnotationsWindow(WindowTab*, Annotation*, EditAnnotFocus focus = EditAnnotFocus::Default);
bool CloseAndDeleteEditAnnotationsWindow(WindowTab*);
void DeleteAnnotationAndUpdateUI(WindowTab*, Annotation*);
void SetSelectedAnnotation(WindowTab*, Annotation*, bool isNew = false, EditAnnotFocus focus = EditAnnotFocus::Default);
void UpdateAnnotationsList(EditAnnotationsWindow*);
void NotifyAnnotationsChanged(EditAnnotationsWindow*);
void DetachAnnotationFromUI(Annotation*);
void InvalidateEditAnnotationsOnEngineChange(WindowTab*);
void RefreshEditAnnotationsAfterEngineChange(WindowTab*);
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
