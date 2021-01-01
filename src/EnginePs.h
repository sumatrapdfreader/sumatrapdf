/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

bool IsPsEngineAvailable();
bool IsPsEngineSupportedFileType(Kind);
EngineBase* CreatePsEngineFromFile(const WCHAR* fileName);
