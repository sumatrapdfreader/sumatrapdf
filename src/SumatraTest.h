/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

Str TestSynctexResult(Str pdfPath, Str srcPath, int line);
Str TestInverseSearchResult(Str pdfPath, int pageNo, int x, int y);
Str TestSearchResult(Str pdfPath, Str needle, Str password = Str());
Str TestDestResult(Str pdfPath, int destNo);
Str TestNamedDestResult(Str pdfPath, Str destName);
Str TestChmResult(Str chmPath, int* exitCodeOut = nullptr);
Str TestTripleClickLineSelectResult(Str pdfPath, Str clickWord, Str expectedLine, int* exitCodeOut = nullptr);
Str TestContextMenuSelectionResult(Str word1, Str word2, Str cursorWord, int* exitCodeOut = nullptr);
Str TestGoToFindMatchResult(Str word, Str typed, int* exitCodeOut = nullptr);
Str TestScrollToLinkResult(int minViewportDelta, int* exitCodeOut = nullptr);
Str TestI18nErrorStringResult(int* exitCodeOut = nullptr);
Str TestGetTocResult(Str path, int* exitCodeOut = nullptr);
Str TestPageLinksResult(Str path, int pageNo, int* exitCodeOut = nullptr);