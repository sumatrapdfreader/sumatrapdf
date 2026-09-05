/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct MainWindow;

void ShowPdfBakeDialog(MainWindow* win);
void ShowPdfExtractTextDialog(MainWindow* win);
void ShowPdfCompressDialog(MainWindow* win);
void ShowPdfDecompressDialog(MainWindow* win);
void ShowPdfDeletePageDialog(MainWindow* win);
void ShowPdfExtractPagesDialog(MainWindow* win);
void ShowPdfEncryptDialog(MainWindow* win);
void ShowPdfDecryptDialog(MainWindow* win);
// comic books / image folders / single images → multi-page PDF (issue #4118)
void ShowConvertToPdfDialog(MainWindow* win);
TempStr ConvertImageCollectionToPdfResultTemp(Str srcPath, Str destPath, int* exitCodeOut);
TempStr ExtractPdfPagesResultTemp(Str destPath, Str pagesSpec, int annotsOnly, int* exitCodeOut);
// PDF pages → PNG / JPEG / BMP files (issue #5991)
void ShowConvertPdfToImagesDialog(MainWindow* win);
TempStr ConvertPagesToImagesResultTemp(Str templatePath, Str pagesSpec, int* exitCodeOut);
