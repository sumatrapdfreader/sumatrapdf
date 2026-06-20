/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

class EngineBase;
struct DisplayModel;
struct MainWindow;
struct StrBuilder;
struct TextSelection;

struct ReadAloudByteLoc {
    int pageNo = -1;
    int x = 0;
    int y = 0;
    int dx = 0;
    int dy = 0;
};

struct ReadAloudHighlightMap {
    int len = 0;
    int cap = 0;
    ReadAloudByteLoc* locs = nullptr;
};

void ReadAloudHighlightFree(ReadAloudHighlightMap* map);

bool ReadAloudHighlightBuildFromPage(EngineBase* engine, int pageNo, ReadAloudHighlightMap* map,
                                     StrBuilder& cleanedOut);

bool ReadAloudHighlightBuildFromTextSelection(TextSelection* ts, ReadAloudHighlightMap* map, StrBuilder& cleanedOut);

bool ReadAloudGetViewportStart(DisplayModel* dm, int* startPageOut, int* startGlyphOut);

bool ReadAloudCanReadFromCursor(DisplayModel* dm, Point screenPt);

bool ReadAloudGetCursorStart(DisplayModel* dm, Point screenPt, int* startPageOut, int* startGlyphOut);

bool ReadAloudHighlightBuildFromDocument(DisplayModel* dm, int startPage, int startGlyph, ReadAloudHighlightMap* map,
                                         StrBuilder& cleanedOut);

void ReadAloudHighlightTimerStart(MainWindow* win);
void ReadAloudHighlightTimerStop(MainWindow* win);

void ReadAloudOnUserViewChanged(MainWindow* win);
void ReadAloudUpdateAutoScroll(MainWindow* win);

void PaintReadAloudHighlight(MainWindow* win, HDC hdc);