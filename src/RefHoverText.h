/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

class EngineBase;
struct RefHoverState;

bool RefHoverTryPlainText(RefHoverState* s, EngineBase* engine, int srcPage, Point pagePos, int& destPageOut,
                          float& destXOut, float& destYOut, RectF& srcRectOut);

void RefHoverFreeLookupCache(RefHoverState* s);

float RefHoverResolveDestYFromSourceText(EngineBase* engine, int srcPage, RectF srcRect, int destPage);
