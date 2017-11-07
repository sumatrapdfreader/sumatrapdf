/* Copyright 2015 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct PageAnnotation;

Vec<PageAnnotation>* LoadFileModifications(const WCHAR* filePath);
bool SaveFileModifictions(const WCHAR* filePath, Vec<PageAnnotation>* list);
bool IsModificationsFile(const WCHAR* filePath);
