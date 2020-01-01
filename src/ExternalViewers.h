/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

bool CouldBePDFDoc(TabInfo* tab);

bool CanViewWithFoxit(TabInfo* tab = nullptr);
bool ViewWithFoxit(TabInfo* tab, const WCHAR* args = nullptr);
bool CanViewWithPDFXChange(TabInfo* tab = nullptr);
bool ViewWithPDFXChange(TabInfo* tab, const WCHAR* args = nullptr);
bool CanViewWithAcrobat(TabInfo* tab = nullptr);
bool ViewWithAcrobat(TabInfo* tab, const WCHAR* args = nullptr);

bool CanViewWithXPSViewer(TabInfo* tab);
bool ViewWithXPSViewer(TabInfo* tab, const WCHAR* args = nullptr);

bool CanViewWithHtmlHelp(TabInfo* tab);
bool ViewWithHtmlHelp(TabInfo* tab, const WCHAR* args = nullptr);

bool ViewWithExternalViewer(TabInfo* tab, size_t idx);

bool CanSendAsEmailAttachment(TabInfo* tab = nullptr);
bool SendAsEmailAttachment(TabInfo* tab, HWND hwndParent = nullptr);
