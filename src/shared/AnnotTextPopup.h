/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

namespace gpui {
struct Ctx;
struct El;
} // namespace gpui

struct MainWindow;
struct Annotation;

bool AnnotationHasText(Annotation*);
bool ShowAnnotationTextPopup(MainWindow*, Annotation*);
void HideAnnotationTextPopup(MainWindow*);
void HideAnnotationTextPopupFor(MainWindow*, Annotation*);
bool IsAnnotationTextPopupShown(MainWindow*);
bool IsAnnotationTextPopupShownFor(MainWindow*, Annotation*);
void RepositionAnnotationTextPopup(MainWindow*);
void DeleteAnnotationTextPopup(MainWindow*);
gpui::El* AnnotTextPopupBuild(MainWindow*, gpui::Ctx*);
