/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

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

TempStr SelectionTranslateResultTemp(int backend, Str srcLang, Str dstLang, Str text, int* exitCode);
