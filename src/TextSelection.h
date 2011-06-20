/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef TextSelection_h
#define TextSelection_h

#include "BaseEngine.h"

class StrVec;

#define iswordchar(c) IsCharAlphaNumeric(c)

struct TextSel {
    int len;
    int *pages;
    RectI *rects;
};

class TextSelection
{
public:
    TextSelection(BaseEngine *engine);
    ~TextSelection();

    bool IsOverGlyph(int pageNo, double x, double y);
    void StartAt(int pageNo, int glyphIx);
    void StartAt(int pageNo, double x, double y) {
        StartAt(pageNo, FindClosestGlyph(pageNo, x, y));
    }
    void SelectUpTo(int pageNo, int glyphIx);
    void SelectUpTo(int pageNo, double x, double y) {
        SelectUpTo(pageNo, FindClosestGlyph(pageNo, x, y));
    }
    void SelectWordAt(int pageNo, double x, double y);
    void CopySelection(TextSelection *orig);
    TCHAR *ExtractText(TCHAR *lineSep);
    void Reset();

    TextSel result;

protected:
    BaseEngine* engine;
    RectI    ** coords;
    TCHAR    ** text;
    int       * lens;

    int         startPage, endPage;
    int         startGlyph, endGlyph;

    int FindClosestGlyph(int pageNo, double x, double y);
    void FillResultRects(int pageNo, int glyph, int length, StrVec *lines=NULL);
};

#endif
