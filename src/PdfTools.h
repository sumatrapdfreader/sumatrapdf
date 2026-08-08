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
