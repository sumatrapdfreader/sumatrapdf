/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

bool IsValidProgramVersion(const char* txt);
int CompareVersion(const char* txt1, const char* txt2);
bool AdjustVariableDriveLetter(WCHAR* path);

bool IsUntrustedFile(const WCHAR* filePath, const WCHAR* fileUrl = nullptr);
