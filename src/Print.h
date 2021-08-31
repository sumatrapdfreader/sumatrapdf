/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// information about a printer
struct Printer {
    WCHAR* name{nullptr};
    DEVMODEW* devMode{nullptr};
    PRINTER_INFO_2* info{nullptr};

    // number of paper sizes supported by the printer
    int nPaperSizes{0};
    // papers[i] is DMPAPER_LETTER etc.
    WORD* papers{nullptr};       // DC_PAPERS
    WCHAR** paperNames{nullptr}; // DC_PAPERNAMES
    POINT* paperSizes{nullptr};  // DC_PAPERSIZE

    Printer() = default;
    ~Printer();
};

Printer* NewPrinter(const WCHAR* name);

bool PrintFile(const WCHAR* fileName, WCHAR* printerName = nullptr, bool displayErrors = true,
               const WCHAR* settings = nullptr);
bool PrintFile(EngineBase* engine, WCHAR* printerName = nullptr, bool displayErrors = true,
               const WCHAR* settings = nullptr);
void OnMenuPrint(WindowInfo* win, bool waitForCompletion = false);
void AbortPrinting(WindowInfo* win);
