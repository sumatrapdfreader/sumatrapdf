/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

bool IsValidProgramVersion(const char* txt);
int CompareVersion(const WCHAR* txt1, const WCHAR* txt2);

bool AdjustVariableDriveLetter(WCHAR* path);

bool IsUntrustedFile(const WCHAR* filePath, const WCHAR* fileUrl = nullptr);
void RelaunchElevatedIfNotDebug();
