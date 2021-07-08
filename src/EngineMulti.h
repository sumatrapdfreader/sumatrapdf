/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

void CalcEndPageNo(TocItem* root, int nPages);

bool IsEngineMultiSupportedFileType(Kind);
EngineBase* CreateEngineMultiFromFiles(std::string_view dir, VecStr& files);
EngineBase* CreateEngineMultiFromDirectory(const WCHAR* dirW);
TocItem* CreateWrapperItem(EngineBase* engine);
