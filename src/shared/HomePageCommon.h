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
