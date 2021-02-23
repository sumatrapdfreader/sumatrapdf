/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

bool ViewWithExternalViewer(TabInfo* tab, size_t idx);

void DetectExternalViewers();
void FreeExternalViewers();
bool CanViewWithKnownExternalViewer(TabInfo* tab, int cmd);
bool ViewWithKnownExternalViewer(TabInfo* tab, int cmd);

bool CanSendAsEmailAttachment(TabInfo* tab = nullptr);
bool SendAsEmailAttachment(TabInfo* tab, HWND hwndParent = nullptr);

bool CouldBePDFDoc(TabInfo* tab);
