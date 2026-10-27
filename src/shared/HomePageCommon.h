/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// --- shared by HomePageCommon.cpp and each app's HomePage.cpp ---
extern StrVec gTipLines;
extern StrVec gPromoLines;
extern bool gTipsParsed;
extern bool gSelectedIsPromo;
extern int gSelectedTipIdx;
void CollectTipsFromString(Str src, StrVec* out);
Str SelectedTipLine();
void PickRandomTipOrPromo();
void PickAnotherRandomTip();
TempStr TrimGitTemp(Str s);
int CountHomePageFiles();
TempStr HomeThumbnailMetaTemp(FileState* fs, float* progressOut);
extern Str sumatraTips;

// the header row: [brand] [search] [Open icon "Open..."] [view icons]
// x and widths in the caller's units (px or dips)
struct HomeHeaderIn {
    int gridX = 0; // the thumbnail grid
    int gridDx = 0;
    int pageX = 0; // the whole page, inside its padding
    int pageDx = 0;
    int firstColDx = 0; // a thumbnail column
    int brandDx = 0;
    int openIconDx = 0;
    int openTextGap = 0;
    int openTextDx = 0;
    int viewIconsDx = 0;
    int gapX = 0;
    int searchMinDx = 0;
    int searchMaxDx = 0;
};

struct HomeHeaderOut {
    int rowX = 0; // the row; the tip wraps at its width
    int rowDx = 0;
    bool showBrand = true;
    int brandX = 0;
    bool showOpenText = true;
    int openX = 0;
    int viewIconsX = 0;
    int searchX = 0;
    int searchDx = 0;
};

void LayoutHomeHeader(const HomeHeaderIn& in, HomeHeaderOut& out);
