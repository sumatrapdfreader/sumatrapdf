/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct EditAnnotationsWindow;

void StartEditAnnotations(TabInfo*, Annotation*);
void CloseAndDeleteEditAnnotationsWindow(EditAnnotationsWindow*);
bool IsEditAnnotationsWindowOpen(TabInfo*);
