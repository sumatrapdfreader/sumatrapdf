/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

extern const char* gIconFileOpen;
extern const char* gIconPrint;
extern const char* gIconPagePrev;
extern const char* gIconPageNext;
extern const char* gIconLayoutContinuous;
extern const char* gIconLayoutSinglePage;
extern const char* gIconZoomOut;
extern const char* gIconZoomIn;
extern const char* gIconSearchPrev;
extern const char* gIconSearchNext;
extern const char* gIconMatchCase;
extern const char* gIconSave;
extern const char* gIconRotateLeft;
extern const char* gIconRotateRight;
extern const char* gIconSpeak;
extern const char* gIconPauseSpeaking;
extern const char* gIconNavigateBack;
extern const char* gIconNavigateForward;
extern const char* gIconSearch;
extern const char* gIconChevronUp;
extern const char* gIconChevronDown;
extern const char* gIconClose;
extern const char* gIconArrowsDiagonal;
extern const char* gIconArrowsDiagonalMinimize;
extern const char* gIconMatchWholeWord;
extern const char* gIconHomeList;
extern const char* gIconHomeThumbnails;
extern const char* gIconPin;

struct Pixmap;

Pixmap* GetCachedPixmapForSvg(Str svg, int dx, int dy, Color fg = kColorUnset, Color bg = kColorUnset);
void DestroySvgPixmapIconsCache();
