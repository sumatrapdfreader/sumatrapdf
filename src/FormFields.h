/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// Interactive PDF form (AcroForm) filling: in-place editing of text fields.
// Checkbox / radio toggling lives in Annotation.cpp (ToggleFormButton).

struct MainWindow;
struct Annotation;

// Start editing a text form field in place (floats an edit box over the field).
// Returns false if `widget` isn't an editable (non-read-only) text widget.
bool StartFormFieldEdit(MainWindow* win, Annotation* widget);

// Commit (save=true) or cancel (save=false) the active form-field edit, if any.
void CommitFormFieldEdit(bool save);

// True while a form field is being edited in place.
bool IsFormFieldEditActive();
