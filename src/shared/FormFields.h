/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

namespace gpui {
struct Ctx;
struct El;
struct PaintCtx;
} // namespace gpui

struct MainWindow;
struct Annotation;
struct Gfx;

bool StartFormFieldEdit(MainWindow* win, Annotation* widget);
bool StartSignatureFieldSigning(MainWindow* win, Annotation* widget);
void CommitFormFieldEdit(bool save);
void CancelFormFieldEditIfWidget(Annotation* widget);
bool IsFormFieldEditActive();
bool FormFieldEditOnTab(bool back);
bool FormFieldEditOnKeyDown(int vk);
void PaintFormFieldHighlights(MainWindow* win, Gfx* gfx);
void PaintFormFieldHighlights(MainWindow* win, gpui::PaintCtx* ctx);
gpui::El* FormFieldEditBuild(MainWindow* win, gpui::Ctx* cx);
TempStr FormFieldEditStateTemp();
