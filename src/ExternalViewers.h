/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

bool HasExternalViewerForCmd(int cmd);
bool ViewWithExternalViewer(WindowTab* tab, size_t idx);

void DetectExternalViewers();
void FreeExternalViewers();
bool CanViewWithKnownExternalViewer(WindowTab* tab, int cmd);
bool ViewWithKnownExternalViewer(WindowTab* tab, int cmd);

bool CanSendAsEmailAttachment(WindowTab* tab = nullptr);
bool SendAsEmailAttachment(WindowTab* tab, HWND hwndParent = nullptr);

bool CouldBePDFDoc(WindowTab*);
bool PathMatchFilter(const char* path, char* filter);
