/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct WindowTab;
enum class AIChatBackend;

void ShowSelectionTranslateDialog(WindowTab* tab, AIChatBackend backend);

// backend: 0=Claude, 1=Grok, 2=Codex
Str TestSelectionTranslateResult(int backend, Str srcLang, Str dstLang, Str text, int* exitCode);