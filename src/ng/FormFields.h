/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// Interactive PDF form (AcroForm) filling: in-place editing of text fields.
// Checkbox / radio toggling lives in Annotation.cpp (ToggleFormButton).

namespace gpui {
struct Ctx;
struct El;
struct PaintCtx;
} // namespace gpui

struct MainWindow;
struct Annotation;

bool StartFormFieldEdit(MainWindow* win, Annotation* widget);
bool StartSignatureFieldSigning(MainWindow* win, Annotation* widget);

void CommitFormFieldEdit(bool save);

void CancelFormFieldEditIfWidget(Annotation* widget);

bool IsFormFieldEditActive();
// Tab / Shift+Tab move to the next editable field on the page
bool FormFieldEditOnTab(bool back);
// the keys of orig's native list box for a choice field: the arrows, Home and
// End move the selection, Enter commits it
bool FormFieldEditOnKeyDown(int vk);
void PaintFormFieldHighlights(MainWindow* win, gpui::PaintCtx* ctx);
gpui::El* FormFieldEditBuild(MainWindow* win, gpui::Ctx* cx);
TempStr FormFieldEditStateTemp();
