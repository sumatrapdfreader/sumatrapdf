/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

enum class PaperFormat {
    Other,
    A2,
    A3,
    A4,
    A5,
    A6,
    Letter,
    Legal,
    Tabloid,
    Statement
};
PaperFormat GetPaperFormatFromSizeApprox(SizeF size);

struct Printer {
    char* name = nullptr;
    char* output = nullptr;
    char* docName = nullptr;
    DEVMODEW* devMode = nullptr;
    PRINTER_INFO_2* info = nullptr;

    // number of paper sizes supported by the printer
    int nPaperSizes = 0;
    // papers[i] is DMPAPER_LETTER etc.
    WORD* papers = nullptr;      // DC_PAPERS
    StrVec paperNames;           // DC_PAPERNAMES
    POINT* paperSizes = nullptr; // DC_PAPERSIZE

    int nBins = 0;
    WORD* bins = nullptr; // DC_BINS
    StrVec binNames;      // DC_BINNAMES

    bool isColor = false;    // DC_COLORDEVICE
    bool isDuplex = false;   // DC_DUPLEX
    bool canStaple = false;  // DC_STAPLE
    bool canCallate = false; // DC_COLLATE
    int orientation = 0;     // DC_ORIENTATION

    Printer() = default;
    ~Printer();
    void SetDevMode(DEVMODEW*);
};

Printer* NewPrinter(const char* name);
void GetPrintersInfo(StrBuilder& out);

class EngineBase;
struct MainWindow;

// result of command-line printing; the numeric values double as the process
// exit code so an automated caller can tell why printing failed (issue #3478)
enum class PrintResult {
    Ok = 0,
    Failed = 1,             // generic / unspecified failure (reserved)
    CannotLoadFile = 2,     // couldn't open the file or unsupported format
    PrintingNotAllowed = 3, // the document doesn't allow printing
    PrinterNotFound = 4,    // the named (or default) printer doesn't exist
    PrintFailed = 5,        // the printer driver / device failed
    NoPermission = 6,       // printing is disabled by restriction policy
};

PrintResult PrintFile(const char* fileName, char* printerName = nullptr, bool displayErrors = true,
                      const char* settings = nullptr);
PrintResult PrintFile2(EngineBase* engine, char* printerName = nullptr, bool displayErrors = true,
                       const char* settings = nullptr);
void PrintCurrentFile(MainWindow* win, bool waitForCompletion = false);
void AbortPrinting(MainWindow* win);
