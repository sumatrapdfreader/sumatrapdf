/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

namespace gpui {
struct Ctx;
struct El;
} // namespace gpui

struct MainWindow;
struct WindowTab;

enum class TranslateEngine {
    Default = 0, // engine remembered in settings (TranslateEngine), Google if none
    Google,
    DeepL,
    Grok,
    Claude,
    Codex,
    AntiGravity,
};

void ShowSelectionTranslateDialog(WindowTab* tab, TranslateEngine engine);
bool IsSelectionTranslateDialogVisible();
// Enter in the dialog: orig's default button
bool SelectionTranslateOnEnter();
void CloseSelectionTranslateDialog();
gpui::El* SelectionTranslateDialogBuild(MainWindow* win, gpui::Ctx* cx);

TempStr SelectionTranslateResultTemp(int backend, Str srcLang, Str dstLang, Str text, int* exitCode);
