/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

char* TestSynctexResult(const char* pdfPath, const char* srcPath, int line);
char* TestSearchResult(const char* pdfPath, const char* needle, const char* password = nullptr);
char* TestDestResult(const char* pdfPath, int destNo);
char* TestNamedDestResult(const char* pdfPath, const char* destName);
char* TestChmResult(const char* chmPath, int* exitCodeOut = nullptr);
