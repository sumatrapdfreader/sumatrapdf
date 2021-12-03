/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct EditAnnotationsWindow;

void StartEditAnnotations(TabInfo*, Annotation*);
void StartEditAnnotations(TabInfo*, Vec<Annotation*>&);
void CloseAndDeleteEditAnnotationsWindow(EditAnnotationsWindow*);
void AddAnnotationToEditWindow(EditAnnotationsWindow*, Annotation*);
void SelectAnnotationInEditWindow(EditAnnotationsWindow*, Annotation*);
void DeleteAnnotationAndUpdateUI(TabInfo*, EditAnnotationsWindow*, Annotation*);
