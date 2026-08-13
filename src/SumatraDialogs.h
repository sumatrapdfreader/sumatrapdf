/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

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
