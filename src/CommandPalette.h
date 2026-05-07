/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

constexpr const char* kPalettePrefixCommands = ">";
constexpr const char* kPalettePrefixFileHistory = "#";
constexpr const char* kPalettePrefixTabs = "@";
constexpr const char* kPalettePrefixEverything = ":";

void RunCommandPalette(MainWindow*, const char* prefix, int smartTabAdvance);
HWND CommandPaletteHwndForAccelerator(HWND hwnd);

void SplitFilterToWords(const char* filter, StrVec& words);
bool FilterMatches(const char* str, const StrVec& words);

struct DrawMaybeHighlightedTextArgs {
    HDC hdc = nullptr;
    RECT rc{};
    const char* text = nullptr;
    const StrVec& filterWords;
    Vec<u8>& highlighted;
    COLORREF colBg = 0;
    bool isRtl = false;
    uint drawFmt = 0;

    DrawMaybeHighlightedTextArgs(const StrVec& fw, Vec<u8>& hl) : filterWords(fw), highlighted(hl) {}
};

void DrawMaybeHighlightedText(DrawMaybeHighlightedTextArgs& args);
