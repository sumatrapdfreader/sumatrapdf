/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "AIChatCommon.h"

struct WindowTab;

void ShowSelectionTranslateDialog(WindowTab* tab, AIChatBackend backend);

// backend: 0=Claude, 1=Grok, 2=Codex
char* TestSelectionTranslateResult(int backend, const char* srcLang, const char* dstLang, const char* text,
                                   int* exitCode);