/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

class EngineBase;
struct PasswordUI;

Str LitToEpubConvert(Str litData);
EngineBase* CreateEngineLitFromFile(Str path, PasswordUI* pwdUI);
