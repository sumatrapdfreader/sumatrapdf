/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct Annotation;

Vec<Annotation>* LoadFileModifications(const WCHAR* filePath);
bool SaveFileModifications(const WCHAR* filePath, Vec<Annotation>* annots);
bool IsModificationsFile(const WCHAR* filePath);
