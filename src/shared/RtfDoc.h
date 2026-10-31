/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

class EngineBase;
struct PasswordUI;

Str RtfToHtml(Str rtf);
EngineBase* CreateEngineRtfFromFile(Str path, PasswordUI* pwdUI);
