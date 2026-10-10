/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// --- shared by SignDocumentDialogCommon.cpp and each app's SignDocumentDialog.cpp ---

EngineBase* GetPdfEngine(MainWindow* win);
RectF SelectionRect(WindowTab* tab, int* pageNoOut);
RectF DefaultSignatureRectAt(DisplayModel* dm, int pageNo, PointF pt);
