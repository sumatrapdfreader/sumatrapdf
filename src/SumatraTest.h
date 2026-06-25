/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

char* TestSynctexResult(const char* pdfPath, const char* srcPath, int line);
char* TestInverseSearchResult(const char* pdfPath, int pageNo, int x, int y);
char* TestSearchResult(const char* pdfPath, const char* needle, const char* password = nullptr);
char* TestDestResult(const char* pdfPath, int destNo);
char* TestNamedDestResult(const char* pdfPath, const char* destName);
char* TestChmResult(const char* chmPath, int* exitCodeOut = nullptr);
char* TestTripleClickLineSelectResult(const char* pdfPath, const char* clickWord, const char* expectedLine,
                                      int* exitCodeOut = nullptr);
char* TestContextMenuSelectionResult(const char* word1, const char* word2, const char* cursorWord,
                                     int* exitCodeOut = nullptr);
char* TestGoToFindMatchResult(const char* word, const char* typed, int* exitCodeOut = nullptr);
char* TestScrollToLinkResult(int minViewportDelta, int* exitCodeOut = nullptr);
char* TestI18nErrorStringResult(int* exitCodeOut = nullptr);
char* TestXfaResult(const char* pdfPath, int* exitCodeOut = nullptr);
char* TestXfaSerializeDataResult(const char* pdfPath, int* exitCodeOut = nullptr);
char* TestXfaSetFieldSerializeDataResult(const char* pdfPath, const char* fieldName, const char* value,
                                         int* exitCodeOut = nullptr);
char* TestXfaSelectRadioSerializeDataResult(const char* pdfPath, const char* fieldName, int pageNo, float x0, float y0,
                                            float x1, float y1, int* exitCodeOut = nullptr);
char* TestXfaSaveFieldRoundTripResult(const char* pdfPath, const char* fieldName, const char* value,
                                      const char* outPath, int* exitCodeOut = nullptr);
char* TestXfaFieldRectsResult(const char* pdfPath, int pageNo, int* exitCodeOut = nullptr);
char* TestRenderPagePngResult(const char* pdfPath, int pageNo, int zoomPct, const char* outPath,
                              int* exitCodeOut = nullptr);
