/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// --- shared by EbookSettingsDialogCommon.cpp and each app's EbookSettingsDialog.cpp ---

Str FontDefaultLabel();
void ParseMargin(Str s, Vec<float>& out);
TempStr MarginTextTemp(const Vec<float>* margin);
bool MarginEq(const Vec<float>& a, const Vec<float>* b);
