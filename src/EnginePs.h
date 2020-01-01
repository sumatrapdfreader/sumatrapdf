/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

bool IsPsEngineAvailable();
bool IsPsEngineSupportedFile(const WCHAR* fileName, bool sniff = false);
EngineBase* CreatePsEngineFromFile(const WCHAR* fileName);
