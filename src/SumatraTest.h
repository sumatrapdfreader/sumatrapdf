/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

TempStr SynctexResultTemp(Str pdfPath, Str srcPath, int line);
TempStr InverseSearchResultTemp(Str pdfPath, int pageNo, int x, int y);
TempStr SearchResultTemp(Str pdfPath, Str needle, Str password = Str());
TempStr FindPageRangeResultTemp(Str pdfPath, Str needle, int first, int last, Str spec = {},
                                int* exitCodeOut = nullptr);
TempStr DestResultTemp(Str pdfPath, int destNo);
TempStr NamedDestResultTemp(Str pdfPath, Str destName);
TempStr ChmResultTemp(Str chmPath, int* exitCodeOut = nullptr);
TempStr TripleClickLineSelectResultTemp(Str pdfPath, Str clickWord, Str expectedLine, int* exitCodeOut = nullptr);
TempStr ContextMenuSelectionResultTemp(Str word1, Str word2, Str cursorWord, int* exitCodeOut = nullptr);
TempStr ClickClearsSelectionResultTemp(Str word, int* exitCodeOut = nullptr);
TempStr RectSelectionDragResultTemp(Str word, int* exitCodeOut = nullptr);
TempStr GoToFindMatchResultTemp(Str word, Str typed, int* exitCodeOut = nullptr);
TempStr ScrollToLinkResultTemp(int minViewportDelta, int* exitCodeOut = nullptr);
TempStr I18nErrorStringResultTemp(int* exitCodeOut = nullptr);
TempStr GetTocResultTemp(Str path, int* exitCodeOut = nullptr);
TempStr PageLinksResultTemp(Str path, int pageNo, int* exitCodeOut = nullptr);
TempStr PageCommentsResultTemp(Str path, int pageNo, int* exitCodeOut = nullptr);
TempStr TocNavigateResultTemp(int destNo, int* exitCodeOut = nullptr);
TempStr DestZoomNavResultTemp(int destNo, int startZoomPerc, int* exitCodeOut = nullptr);
TempStr MarkdownTocNavigateResultTemp(int destNo, int minScrollY, int* exitCodeOut = nullptr);
TempStr MarkdownFollowLinkResultTemp(Str href, bool follow, int* exitCodeOut = nullptr);
TempStr CadEnhanceColorsResultTemp(Str path, int pageNo, int zoomPercent, int* exitCodeOut = nullptr);
