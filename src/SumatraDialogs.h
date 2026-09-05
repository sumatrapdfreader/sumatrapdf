/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct MainWindow;
struct WindowTab;

void ShowAddFavoriteDialog(MainWindow* win, Str filePath, int pageNo, Str pageLabel, Str name);
void ShowAdvancedSettingsDialog(MainWindow* win);
TempStr AdvSettingsRowsResultTemp(Str action, int arg, int* exitCodeOut);
void ShowChangeBackgroundColorDialog(MainWindow* win);
void ShowChangeLanguageDialog(MainWindow* win);
void ShowChangeScrollbarDialog(MainWindow* win);
void ShowChangeThemeDialog(MainWindow* win);
void ShowSetDocumentColorsFollowThemeDialog(MainWindow* win);
void ShowSetTabColorDialog(MainWindow* win, WindowTab* tab);
void ShowCustomZoomDialog(MainWindow* win);
void ShowPageGridDialog(MainWindow* win);
void ResetPageGridToDefaults();
TempStr PageGridStateTemp();
void ShowSignDocumentDialog(MainWindow* win, Str fieldName = {}, bool hasField = false);
bool IsPlacingSignature(MainWindow* win);
bool CancelPlacingSignature(MainWindow* win);
void CloseSignDocumentDialog(MainWindow* win);
bool FinishSignaturePlacement(MainWindow* win, int x, int y, bool aborted);
void ShowEbookSettingsDialog(MainWindow* win);
Str ShowGetPasswordDialog(HWND hwndParent, Str fileName, bool* rememberPassword, bool* showPassword);
void ShowGoToPageDialog(MainWindow* win);
void ShowInverseSearchDialog(MainWindow* win);
void ShowSettingsDialog(MainWindow* win);

enum class PrintRangeAdv {
    All = 0,
    Even,
    Odd
};
enum class PrintScaleAdv {
    None = 0,
    Shrink,
    Fit,
    Stretch
};
enum class PrintRotationAdv {
    Auto = 0,
    Portrait,
    Landscape
};

struct Print_Advanced_Data {
    PrintRangeAdv range;
    PrintScaleAdv scale;
    PrintRotationAdv rotation;
    bool autoRotate;
    bool centerHorizontally;
    // when true, let the printer pick the input tray whose paper matches the
    // document's page size (DMBIN_FORMSOURCE), independent of page scaling
    bool paperSourceByPageSize;
    // when true, set the paper size to each page's own size before printing it,
    // so mixed page size documents print to the right paper/tray
    bool perPagePaperSize;
    // extra rotation applied to the printout, in degrees (0, 90, 180 or 270),
    // on top of the automatic rotation; lets the user fix wrong orientation
    // (e.g. upside-down output on virtual printers), issue #1246
    int extraRotation;

    explicit Print_Advanced_Data(PrintRangeAdv range = PrintRangeAdv::All, PrintScaleAdv scale = PrintScaleAdv::Shrink,
                                 PrintRotationAdv rotation = PrintRotationAdv::Auto, bool autoRotate = true,
                                 bool centerHorizontally = false, bool paperSourceByPageSize = false,
                                 bool perPagePaperSize = false, int extraRotation = 0)
        : range(range),
          scale(scale),
          rotation(rotation),
          autoRotate(autoRotate),
          centerHorizontally(centerHorizontally),
          paperSourceByPageSize(paperSourceByPageSize),
          perPagePaperSize(perPagePaperSize),
          extraRotation(extraRotation) {}
};

HPROPSHEETPAGE CreatePrintAdvancedPropSheet(Print_Advanced_Data* data, ScopedMem<DLGTEMPLATE>& dlgTemplate);
