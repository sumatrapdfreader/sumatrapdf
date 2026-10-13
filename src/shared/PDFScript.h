/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct MainWindow;

struct PDFScriptResult {
    int exitCode = -1;
    Str output;
    ~PDFScriptResult();
};

int PDFScriptCount();
Str PDFScriptName(int index);
Str PDFScriptSource(int index);
TempStr PDFScriptOutputPath(Str input);
void GetPDFScriptArgs(Str script, Str input, StrVec& values, StrVec& hints);
TempStr ExpandPDFScript(Str script, const StrVec& args);
void StartPDFScript(Str script, const StrVec& args, const Func1<PDFScriptResult*>& onDone);
void ShowPDFScriptDialog(MainWindow* win);

constexpr char kPDFScriptHelpDocUri[] = "/Run-PDF-script";
