/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

bool IsPsEngineAvailable();
bool IsPsEngineSupportedFileType(Kind);
EngineBase* CreatePsEngineFromFile(const WCHAR* fileName);
