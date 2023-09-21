/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct EditAnnotationsWindow;

void ShowEditAnnotationsWindow(WindowTab*);
void CloseAndDeleteEditAnnotationsWindow(EditAnnotationsWindow*);
void DeleteAnnotationAndUpdateUI(WindowTab*, Annotation*);
void SetSelectedAnnotation(WindowTab*, Annotation*);
void UpdateAnnotationsList(EditAnnotationsWindow*);
