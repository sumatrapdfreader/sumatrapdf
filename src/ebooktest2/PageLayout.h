/* Copyright 2011-2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef PageLayout2_h
#define PageLayout2_h

// reuse src/PageLayout as far as possible
#include "../PageLayout.h"

#include "BaseEbookDoc.h"

Vec<PageData*> *LayoutHtml2(LayoutInfo li, BaseEbookDoc *doc);
void DrawPageLayout2(Graphics *g, PageData *data, PointF offset, bool showBbox);

#endif
