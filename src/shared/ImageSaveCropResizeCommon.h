/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// --- shared by ImageSaveCropResizeCommon.cpp and each app's ImageSaveCropResize.cpp ---

bool ExtMatchesOriginal(Str ext, Str originalExt);
TempStr PathWithExtTemp(Str path, Str ext);

enum class DragEdge {
    None,
    Left,
    Right,
    Top,
    Bottom,
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight,
    Move,   // only used in crop mode
    NewCrop // only used in crop mode
};

// a format an image can be saved in. Each app has its own gImageFormats.
struct ImageFormat {
    Str label;
    Str ext;
    bool needsProbe; // if true, check if an encoder is available before offering
    bool available;  // set after probing
    bool isPdf;      // PDF is created via PdfCreator, not an image encoder
};
constexpr int kImageFormatCount = 7;
extern ImageFormat gImageFormats[kImageFormatCount];
// index of the PDF entry in gImageFormats
constexpr int kPdfFormatIdx = 6;
constexpr int kDefaultFormatIdx = 0; // PNG
int FormatIdxFromExt(Str ext);
