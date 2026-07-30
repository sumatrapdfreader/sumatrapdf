/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct EditAnnotationsWindow;

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
// Drop non-owning Annotation* held by UI (selection, drag, hover, form edit).
// Call before DeleteAnnotation frees the wrapper, or when the engine is about
// to die and raw Annotation* must not be used again.
void DetachAnnotationFromUI(Annotation*);
// Clear the edit-annotations list (and listbox) before the engine is destroyed
// so ReloadDocument cannot leave dangling Annotation* in the open panel.
void InvalidateEditAnnotationsOnEngineChange(WindowTab*);
