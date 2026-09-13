/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct CustomCommand;

// the CmdOpenWith* commands for viewers we detect ourselves, in menu order,
// 0-terminated
extern const int gOpenWithKnownExternalViewerCmds[];

bool IsOpenWithKnownExternalViewerCmd(int cmdId);
bool IsOpenWithKnownExternalViewerCmd(CustomCommand* cmd);

bool HasKnownExternalViewerForCmd(int cmd);

void DetectExternalViewers();
void FreeExternalViewers();
bool CanViewWithKnownExternalViewer(WindowTab* tab, int cmd);
bool ViewWithKnownExternalViewer(WindowTab* tab, int cmd);

bool CanSendAsEmailAttachment(WindowTab* tab = nullptr);
bool SendAsEmailAttachment(WindowTab* tab, HWND hwndParent = nullptr);

bool CouldBePDFDoc(WindowTab*);
bool IsPdfDoc(WindowTab*);
bool PathMatchFilter(Str path, Str filter);

bool RunWithExe(WindowTab* tab, Str cmdLine, Str filter);
