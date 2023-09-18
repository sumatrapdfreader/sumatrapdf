/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct EditAnnotationsWindow;

void StartEditAnnotations(WindowTab*, Annotation*);
void StartEditAnnotations(WindowTab*, Vec<Annotation*>&);
void CloseAndDeleteEditAnnotationsWindow(EditAnnotationsWindow*);
void AddAnnotationToEditWindow(EditAnnotationsWindow*, Annotation*);
void SelectAnnotationInEditWindow(EditAnnotationsWindow*, Annotation*);
void DeleteAnnotationAndUpdateUI(WindowTab*, EditAnnotationsWindow*, Annotation*);
