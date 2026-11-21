/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// --- replacing an image embedded in a PDF (the image's context menu) ---

bool ReplaceImageWithData(MainWindow* win, int pageNo, int imageIdx, Str data);
bool ImageElementPos(IPageElement* el, int* pageNo, int* imageIdx);
void ReplaceImageFromPath(MainWindow* win, WindowTab* tab, int pageNo, int imageIdx, Str path);
#if OS_WIN
void EditImageExternally(MainWindow* win, IPageElement* imageEl);
#endif
TempStr TestImageReplace(MainWindow* win, Str action, int pageNo, int imageIdx, Str path);

// implemented by each app: asks for the image file, then ReplaceImageFromPath
void ReplaceImageFromFile(MainWindow* win, IPageElement* imageEl);
