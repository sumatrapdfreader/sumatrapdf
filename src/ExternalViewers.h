/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

bool HasKnownExternalViewerForCmd(int cmd);

void DetectExternalViewers();
void FreeExternalViewers();
bool CanViewWithKnownExternalViewer(WindowTab* tab, int cmd);
bool ViewWithKnownExternalViewer(WindowTab* tab, int cmd);

bool CanSendAsEmailAttachment(WindowTab* tab = nullptr);
bool SendAsEmailAttachment(WindowTab* tab, HWND hwndParent = nullptr);

bool CouldBePDFDoc(WindowTab*);
bool PathMatchFilter(const char* path, const char* filter);

void CreateExternalViewersCommands();
ExternalViewer* CustomExternalViewerForCmdId(int cmdId);
bool ViewWithCustomExternalViewer(WindowTab* tab, int cmdId);
bool RunWithExe(WindowTab* tab, const char* cmdLine, const char* filter);
