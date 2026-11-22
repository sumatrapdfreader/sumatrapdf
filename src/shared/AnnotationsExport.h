/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// one annotation as `SumatraPDF cli annotations` and Export Annotations show it
struct AnnotRow {
    int pageNo = 0;
    AnnotationType type = AnnotationType::Unknown;
    RectF rect;
    // the page text a highlight (underline, ...) is over
    TempStr marked;
    Str contents;
    Str author;
    // "#rrggbb", empty if none
    TempStr color;
    // ISO 8601, empty if not known
    TempStr modified;
};

bool IsTextMarkupAnnot(AnnotationType tp);
void CollectAnnotRows(EngineBase* engine, const Vec<int>& pages, Vec<AnnotRow>& rows);
TempStr AnnotationsMarkdownTemp(const Vec<AnnotRow>& rows, Str docName);
TempStr AnnotationsCsvTemp(const Vec<AnnotRow>& rows);
