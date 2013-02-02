/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef FileModifications_h
#define FileModifications_h

struct PageAnnotation;

Vec<PageAnnotation> *LoadFileModifications(const WCHAR *filepath);
bool SaveFileModifictions(const WCHAR *filepath, Vec<PageAnnotation> *list);

#endif
