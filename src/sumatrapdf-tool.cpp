/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// sumatrapdf-tool.exe runs the mupdf-derived command-line tools (draw,
// convert, info, ...). Unlike SumatraPDF.exe - a GUI (Windows subsystem) app
// that can't write to a console or be piped reliably - this is a console
// subsystem app, so it behaves well under cmd.exe / PowerShell.
//
// It links libmupdf.dll for everything (the tool implementations live there),
// so the executable itself stays tiny.
//
// Usage: sumatrapdf-tool.exe <tool> [args...]   e.g. sumatrapdf-tool.exe info file.pdf

#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <string.h>

// implemented in (and exported from) libmupdf.dll - see src/libmupdf.def
extern "C" {
// str-port: mupdf C main entry points
int muconvert_main(int argc, char* argv[]); // str-port: mupdf
int mudraw_main(int argc, char* argv[]);    // str-port: mupdf
int mutrace_main(int argc, char* argv[]);   // str-port: mupdf
int murun_main(int argc, char* argv[]);     // str-port: mupdf

int pdfclean_main(int argc, char* argv[]);   // str-port: mupdf
int pdfextract_main(int argc, char* argv[]); // str-port: mupdf
int pdfinfo_main(int argc, char* argv[]);    // str-port: mupdf
int pdfposter_main(int argc, char* argv[]);  // str-port: mupdf
int pdfshow_main(int argc, char* argv[]);    // str-port: mupdf
int pdfpages_main(int argc, char* argv[]);   // str-port: mupdf
int pdfcreate_main(int argc, char* argv[]);  // str-port: mupdf
int pdfmerge_main(int argc, char* argv[]);   // str-port: mupdf
int pdfsign_main(int argc, char* argv[]);    // str-port: mupdf
int pdfrecolor_main(int argc, char* argv[]); // str-port: mupdf
int pdftrim_main(int argc, char* argv[]);    // str-port: mupdf
int pdfbake_main(int argc, char* argv[]);    // str-port: mupdf
int mubar_main(int argc, char* argv[]);      // str-port: mupdf
int mugrep_main(int argc, char* argv[]);     // str-port: mupdf
int pdfaudit_main(int argc, char* argv[]);   // str-port: mupdf

// convert wide argv (so paths are correct regardless of the ANSI code page)
// to the UTF-8 char argv the tools expect
char** fz_argv_from_wargv(int argc, wchar_t** wargv); // str-port: mupdf
void fz_free_argv(int argc, char** argv);             // str-port: mupdf
}

// must match premake5.lua and src/SumatraStartup.cpp
#define FZ_ENABLE_JS 1
#define FZ_ENABLE_PDF 1
#define FZ_ENABLE_BARCODE 0

using MutoolFunc = int (*)(int argc, char* argv[]); // str-port: mupdf

struct Tool {
    const char* name; // str-port: C-string
    MutoolFunc fn;
    const char* desc; // str-port: C-string
};

static Tool gTools[] = {
#if FZ_ENABLE_JS
    {"run", murun_main, "run javascript"},
#endif
    {"draw", mudraw_main, "convert document"},
    {"convert", muconvert_main, "convert document (with simpler options)"},
#if FZ_ENABLE_PDF
    {"audit", pdfaudit_main, "produce usage stats from PDF files"},
    {"bake", pdfbake_main, "bake PDF form into static content"},
    {"clean", pdfclean_main, "rewrite PDF file"},
    {"create", pdfcreate_main, "create PDF document"},
    {"extract", pdfextract_main, "extract font and image resources"},
    {"info", pdfinfo_main, "show information about PDF resources"},
    {"merge", pdfmerge_main, "merge pages from multiple PDF sources into a new PDF"},
    {"pages", pdfpages_main, "show information about PDF pages"},
    {"poster", pdfposter_main, "split large PDF page into many tiles"},
    {"recolor", pdfrecolor_main, "change colorspace of PDF document"},
    {"show", pdfshow_main, "show internal PDF objects"},
    {"sign", pdfsign_main, "manipulate PDF digital signatures"},
    {"trim", pdftrim_main, "trim PDF page contents"},
#endif
    {"grep", mugrep_main, "search for text"},
    {"trace", mutrace_main, "trace device calls"},
#if FZ_ENABLE_BARCODE
    {"barcode", mubar_main, "encode/decode barcodes"},
#endif
};

static const Tool* FindTool(const char* name) { // str-port: C-string
    for (const Tool& t : gTools) {
        if (_stricmp(t.name, name) == 0) {
            return &t;
        }
    }
    return nullptr;
}

static void PrintUsage() {
    fprintf(stderr, "sumatrapdf-tool: run a mupdf command-line tool\n");
    fprintf(stderr, "usage: sumatrapdf-tool <tool> [args...]\n\n");
    fprintf(stderr, "tools:\n");
    for (const Tool& t : gTools) {
        fprintf(stderr, "  %-9s %s\n", t.name, t.desc);
    }
}

int main() {
    int argc = 0;
    WCHAR** wargv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!wargv) {
        fprintf(stderr, "CommandLineToArgvW() failed\n");
        return 1;
    }
    if (argc < 2) {
        PrintUsage();
        LocalFree(wargv);
        return 1;
    }

    // argv[0] is the program path; the tool name is argv[1]. Drop argv[0] so the
    // tool sees its own name as argv[0], matching what the mupdf tools expect.
    int toolArgc = argc - 1;
    char** toolArgv = fz_argv_from_wargv(toolArgc, wargv + 1);
    LocalFree(wargv); // fz_argv_from_wargv copied the strings, so this is safe

    const Tool* tool = FindTool(toolArgv[0]);
    int res;
    if (tool) {
        res = tool->fn(toolArgc, toolArgv);
    } else {
        fprintf(stderr, "unknown tool: %s\n\n", toolArgv[0]);
        PrintUsage();
        res = 1;
    }
    fz_free_argv(toolArgc, toolArgv);
    return res;
}
