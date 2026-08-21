/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// Interactive PDF form (AcroForm) filling: in-place editing of text fields.
// Checkbox / radio toggling lives in Annotation.cpp (ToggleFormButton).

struct MainWindow;
struct Annotation;
struct Gfx;

bool StartFormFieldEdit(MainWindow* win, Annotation* widget);
bool StartSignatureFieldSigning(MainWindow* win, Annotation* widget);

void CommitFormFieldEdit(bool save);

void CancelFormFieldEditIfWidget(Annotation* widget);

bool IsFormFieldEditActive();
void PaintFormFieldHighlights(MainWindow* win, Gfx* gfx);
