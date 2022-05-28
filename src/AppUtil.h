/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

bool IsValidProgramVersion(const char* txt);
int CompareVersion(const char* txt1, const char* txt2);
bool AdjustVariableDriveLetter(char* path);

bool IsUntrustedFile(const char* filePath, const char* fileUrl = nullptr);
void DrawCloseButton(HWND hwnd, HDC hdc, Rect& r);
