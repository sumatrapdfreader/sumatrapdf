/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct WindowTab;
struct IPageElement;

void SearchWithGoogleLens(WindowTab* tab, IPageElement* imageElement = nullptr, int pageNo = 0);
void SearchGoogleLensSelection(WindowTab* tab);
void SearchGoogleLensPage(WindowTab* tab, int pageNo);
void SearchGoogleLensImage(WindowTab* tab, IPageElement* imageElement);
